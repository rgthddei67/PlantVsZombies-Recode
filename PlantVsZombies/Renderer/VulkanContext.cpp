#include "VulkanContext.h"
#include "VulkanTexturePool.h"
#include "../Logger.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

namespace pvz {
	namespace {
		// 用打印替代异常，让 Phase 1 烟雾测试容易调试。
#define VK_CHECK(expr)                                                      \
    do {                                                                    \
        VkResult _r = (expr);                                               \
        if (_r != VK_SUCCESS) {                                             \
            LOG_ERROR("VulkanContext") << #expr " failed (VkResult=" << (int)_r << ")"; \
            return false;                                                   \
        }                                                                   \
    } while (0)

		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT        /*types*/,
			const VkDebugUtilsMessengerCallbackDataEXT* data,
			void*                                  /*userData*/) {
			const char* msg = data && data->pMessage ? data->pMessage : "(no message)";
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				LOG_ERROR("VulkanContext") << msg;
			else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				LOG_WARN("VulkanContext") << msg;
			else
				LOG_DEBUG("VulkanContext") << msg;
			return VK_FALSE;
		}

		bool HasLayer(const char* name) {
			uint32_t count = 0;
			vkEnumerateInstanceLayerProperties(&count, nullptr);
			std::vector<VkLayerProperties> layers(count);
			vkEnumerateInstanceLayerProperties(&count, layers.data());
			for (const auto& l : layers) {
				if (std::strcmp(l.layerName, name) == 0) return true;
			}
			return false;
		}

		bool HasDeviceExtension(const std::vector<VkExtensionProperties>& extensions,
			const char* name) {
			return std::any_of(extensions.begin(), extensions.end(), [name](const auto& extension) {
				return std::strcmp(extension.extensionName, name) == 0;
				});
		}

		std::vector<VkExtensionProperties> EnumerateDeviceExtensions(VkPhysicalDevice device) {
			uint32_t count = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
			std::vector<VkExtensionProperties> extensions(count);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
			return extensions;
		}

		// synchronization2 的 64 位标志比传统接口更细；这里只映射项目实际使用的范围，
		// 未知标志保守扩大为 ALL_COMMANDS，避免兼容路径发生欠同步。
		VkPipelineStageFlags ToLegacyStageMask(VkPipelineStageFlags2 stages) {
			VkPipelineStageFlags result = 0;
			if (stages & VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT)
				result |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			if (stages & VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT)
				result |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			if (stages & VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT)
				result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			if (stages & VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
				result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (stages & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT)
				result |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			if (stages & (VK_PIPELINE_STAGE_2_TRANSFER_BIT |
				VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT |
				VK_PIPELINE_STAGE_2_COPY_BIT |
				VK_PIPELINE_STAGE_2_RESOLVE_BIT |
				VK_PIPELINE_STAGE_2_BLIT_BIT |
				VK_PIPELINE_STAGE_2_CLEAR_BIT)) {
				result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			return result != 0 ? result : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}

		VkAccessFlags ToLegacyAccessMask(VkAccessFlags2 accesses) {
			VkAccessFlags result = 0;
			if (accesses & VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT)
				result |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			if (accesses & VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
				result |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			if (accesses & VK_ACCESS_2_TRANSFER_READ_BIT)
				result |= VK_ACCESS_TRANSFER_READ_BIT;
			if (accesses & VK_ACCESS_2_TRANSFER_WRITE_BIT)
				result |= VK_ACCESS_TRANSFER_WRITE_BIT;
			if (accesses & (VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT))
				result |= VK_ACCESS_SHADER_READ_BIT;
			if (accesses & VK_ACCESS_2_MEMORY_READ_BIT)
				result |= VK_ACCESS_MEMORY_READ_BIT;
			if (accesses & VK_ACCESS_2_MEMORY_WRITE_BIT)
				result |= VK_ACCESS_MEMORY_WRITE_BIT;
			if (accesses == 0 || result != 0) return result;
			return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		}
	} // anonymous namespace

	VulkanContext::VulkanContext() = default;
	VulkanContext::~VulkanContext() { Shutdown(); }

	bool VulkanContext::Initialize(SDL_Window* window, bool enableValidation, bool vsync,
		bool forceVulkan12, bool forceLegacyRendering, bool forceLegacySynchronization) {
		mValidationEnabled = enableValidation;
		mForceVulkan12 = forceVulkan12;
		mForceLegacyRendering = forceLegacyRendering;
		mForceLegacySynchronization = forceLegacySynchronization;
		mWindow = window;

		// 初始化失败也必须释放已创建到一半的 instance/device，并在 SDL 卸载 loader 前
		// 清空 Volk 指针；集中走 Shutdown 可避免每个阶段各自维护回滚顺序。
		if (!InitializeVulkanLoader() ||
			!CreateInstance(window, enableValidation) ||
			(enableValidation && !CreateDebugMessenger()) ||
			!CreateSurface(window) ||
			!PickPhysicalDevice() ||
			!CreateLogicalDevice() ||
			!CreateSwapchain(window, vsync) ||
			!CreateAllocator()) {
			Shutdown();
			return false;
		}

		mInitialized = true;

		LOG_INFO("VulkanContext") << "Ready. swapchain=" << mSwapchainExtent.width << "x" << mSwapchainExtent.height
			<< " format=" << (int)mSwapchainFormat << " images=" << mSwapchainImages.size()
			<< " validation=" << (mValidationEnabled ? 1 : 0)
			<< " api=" << VK_VERSION_MAJOR(mApiVersion) << "." << VK_VERSION_MINOR(mApiVersion)
			<< " dynamicRendering=" << DynamicRenderingPathName()
			<< " synchronization=" << SynchronizationPathName();

		return true;
	}

	bool VulkanContext::InitializeVulkanLoader() {
		// SDL_WINDOW_VULKAN 已让 SDL2 加载平台 loader；沿用同一个入口可避免 SDL 与
		// Volk 分别持有不同的动态库选择或搜索路径。
		auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
			SDL_Vulkan_GetVkGetInstanceProcAddr());
		if (!getInstanceProcAddr) {
			LOG_ERROR("VulkanContext") << "SDL 未能取得 vkGetInstanceProcAddr: " << SDL_GetError();
			return false;
		}

		volkInitializeCustom(getInstanceProcAddr);
		mVolkInitialized = true;
		if (!vkCreateInstance || !vkEnumerateInstanceExtensionProperties) {
			LOG_ERROR("VulkanContext") << "Volk 未能加载 Vulkan 全局入口";
			return false;
		}

		mLoaderApiVersion = volkGetInstanceVersion();
		LOG_INFO("VulkanContext") << "Vulkan loader api "
			<< VK_VERSION_MAJOR(mLoaderApiVersion) << "."
			<< VK_VERSION_MINOR(mLoaderApiVersion) << "."
			<< VK_VERSION_PATCH(mLoaderApiVersion);
		if (mLoaderApiVersion < VK_API_VERSION_1_2) {
			LOG_ERROR("VulkanContext") << "当前渲染路径至少要求 Vulkan loader 1.2；检测到 "
				<< VK_VERSION_MAJOR(mLoaderApiVersion) << "."
				<< VK_VERSION_MINOR(mLoaderApiVersion)
				<< "。";
			return false;
		}
		mInstanceApiVersion = std::min(mLoaderApiVersion,
			mForceVulkan12 ? VK_API_VERSION_1_2 : VK_API_VERSION_1_3);
		return true;
	}

	bool VulkanContext::CreateInstance(SDL_Window* window, bool enableValidation) {
		VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app.pApplicationName = "PlantsVsZombies";
		app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app.pEngineName = "PVZ-Vulkan";
		app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app.apiVersion = mInstanceApiVersion;

		// SDL 帮我们列出 surface 相关扩展（platform-specific）
		uint32_t sdlExtCount = 0;
		if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtCount, nullptr)) {
			LOG_ERROR("VulkanContext") << "SDL_Vulkan_GetInstanceExtensions(count) failed: " << SDL_GetError();
			return false;
		}
		std::vector<const char*> extensions(sdlExtCount);
		if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtCount, extensions.data())) {
			LOG_ERROR("VulkanContext") << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError();
			return false;
		}
		if (enableValidation) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		std::vector<const char*> layers;
		if (enableValidation) {
			if (HasLayer("VK_LAYER_KHRONOS_validation")) {
				layers.push_back("VK_LAYER_KHRONOS_validation");
			}
			else {
				LOG_WARN("VulkanContext") << "validation requested but VK_LAYER_KHRONOS_validation not installed (install VulkanSDK).";
				mValidationEnabled = false;
			}
		}

		VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		info.pApplicationInfo = &app;
		info.enabledExtensionCount = (uint32_t)extensions.size();
		info.ppEnabledExtensionNames = extensions.data();
		info.enabledLayerCount = (uint32_t)layers.size();
		info.ppEnabledLayerNames = layers.data();

		VK_CHECK(vkCreateInstance(&info, nullptr, &mInstance));
		volkLoadInstance(mInstance);
		return true;
	}

	bool VulkanContext::CreateDebugMessenger() {
		// VK_EXT_debug_utils 的两个函数不在 vulkan-1.lib 的静态导出表里——它们是
		// instance-level extension，必须用 vkGetInstanceProcAddr 在运行时取。
		auto fnCreate = (PFN_vkCreateDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
		if (!fnCreate) {
			LOG_ERROR("VulkanContext") << "vkCreateDebugUtilsMessengerEXT not found (VK_EXT_debug_utils not loaded?)";
			return false;
		}

		VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		info.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		info.pfnUserCallback = DebugCallback;
		VK_CHECK(fnCreate(mInstance, &info, nullptr, &mDebugMessenger));
		return true;
	}

	bool VulkanContext::CreateSurface(SDL_Window* window) {
		if (!SDL_Vulkan_CreateSurface(window, mInstance, &mSurface)) {
			LOG_ERROR("VulkanContext") << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError();
			return false;
		}
		return true;
	}

	bool VulkanContext::PickPhysicalDevice() {
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
		if (count == 0) {
			LOG_ERROR("VulkanContext") << "No Vulkan-capable GPU found.";
			return false;
		}
		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

		struct Candidate {
			VkPhysicalDevice device = VK_NULL_HANDLE;
			VkPhysicalDeviceProperties properties{};
			uint32_t queueFamily = UINT32_MAX;
			uint32_t apiVersion = 0;
			FeaturePath dynamicRendering = FeaturePath::Legacy;
			FeaturePath synchronization = FeaturePath::Legacy;
		};

		auto inspect = [this](VkPhysicalDevice dev, Candidate& out) -> bool {
			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(dev, &props);
			const uint32_t apiVersion = std::min(mInstanceApiVersion, props.apiVersion);
			if (apiVersion < VK_API_VERSION_1_2) {
				LOG_WARN("VulkanContext") << "Skipping " << props.deviceName
					<< ": Vulkan 1.2 is required";
				return false;
			}

			const auto extensions = EnumerateDeviceExtensions(dev);
			if (!HasDeviceExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
				LOG_WARN("VulkanContext") << "Skipping " << props.deviceName
					<< ": VK_KHR_swapchain is missing";
				return false;
			}

			const bool core13 = apiVersion >= VK_API_VERSION_1_3;
			const bool hasDynamicRenderingExtension = !mForceLegacyRendering &&
				HasDeviceExtension(extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
			const bool hasSynchronization2Extension = !mForceLegacySynchronization &&
				HasDeviceExtension(extensions, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

			VkPhysicalDeviceVulkan13Features f13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRendering{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR };
			VkPhysicalDeviceSynchronization2FeaturesKHR synchronization{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR };
			VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			if (core13) {
				f12.pNext = &f13;
			}
			else if (hasDynamicRenderingExtension) {
				f12.pNext = &dynamicRendering;
				dynamicRendering.pNext = hasSynchronization2Extension ? &synchronization : nullptr;
			}
			else if (hasSynchronization2Extension) {
				f12.pNext = &synchronization;
			}
			VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			f2.pNext = &f12;
			vkGetPhysicalDeviceFeatures2(dev, &f2);

			if (!f12.descriptorIndexing ||
				!f12.runtimeDescriptorArray ||
				!f12.descriptorBindingPartiallyBound ||
				!f12.descriptorBindingSampledImageUpdateAfterBind ||
				!f12.shaderSampledImageArrayNonUniformIndexing ||
				!f12.descriptorBindingVariableDescriptorCount) {
				LOG_WARN("VulkanContext") << "Skipping " << props.deviceName
					<< ": required Vulkan 1.2 bindless descriptor features are missing";
				return false;
			}

			// 8192 个 combined image samplers 会同时消耗 sampler 与 sampled-image 限额。
			VkPhysicalDeviceVulkan12Properties properties12{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
			VkPhysicalDeviceProperties2 properties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			properties2.pNext = &properties12;
			vkGetPhysicalDeviceProperties2(dev, &properties2);
			constexpr uint32_t requiredDescriptors = VulkanTexturePool::MAX_TEXTURES; // bindless 固定容量
			if (properties12.maxPerStageDescriptorUpdateAfterBindSamplers < requiredDescriptors ||
				properties12.maxPerStageDescriptorUpdateAfterBindSampledImages < requiredDescriptors ||
				properties12.maxDescriptorSetUpdateAfterBindSamplers < requiredDescriptors ||
				properties12.maxDescriptorSetUpdateAfterBindSampledImages < requiredDescriptors ||
				properties12.maxUpdateAfterBindDescriptorsInAllPools < requiredDescriptors) {
				LOG_WARN("VulkanContext") << "Skipping " << props.deviceName
					<< ": update-after-bind descriptor limit is below " << requiredDescriptors;
				return false;
			}

			uint32_t queueFamily = UINT32_MAX;
			uint32_t queueCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, nullptr);
			std::vector<VkQueueFamilyProperties> queues(queueCount);
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, queues.data());
			for (uint32_t i = 0; i < queueCount; ++i) {
				if (!(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
				VkBool32 supportsPresent = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mSurface, &supportsPresent);
				if (supportsPresent) {
					queueFamily = i;
					break;
				}
			}
			if (queueFamily == UINT32_MAX) {
				LOG_WARN("VulkanContext") << "Skipping " << props.deviceName
					<< ": no graphics+present queue family";
				return false;
			}

			out.device = dev;
			out.properties = props;
			out.queueFamily = queueFamily;
			out.apiVersion = apiVersion;
			out.dynamicRendering = !mForceLegacyRendering && core13 && f13.dynamicRendering
				? FeaturePath::Core13
				: (hasDynamicRenderingExtension && dynamicRendering.dynamicRendering
					? FeaturePath::KhrExtension : FeaturePath::Legacy);
			out.synchronization = !mForceLegacySynchronization && core13 && f13.synchronization2
				? FeaturePath::Core13
				: (hasSynchronization2Extension && synchronization.synchronization2
					? FeaturePath::KhrExtension : FeaturePath::Legacy);
			return true;
		};

		Candidate selected{};
		int selectedScore = std::numeric_limits<int>::min();
		for (auto dev : devices) {
			Candidate candidate{};
			if (!inspect(dev, candidate)) continue;
			int score = 0;
			switch (candidate.properties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score = 300; break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 200; break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score = 100; break;
			default:                                     score = 0;   break;
			}
			if (candidate.dynamicRendering != FeaturePath::Legacy) score += 10;
			if (candidate.synchronization != FeaturePath::Legacy) score += 5;
			if (score > selectedScore) {
				selected = candidate;
				selectedScore = score;
			}
		}

		if (!selected.device) {
			LOG_ERROR("VulkanContext") << "No GPU meets requirements: Vulkan 1.2, swapchain, "
				"and 8192-slot bindless descriptors.";
			return false;
		}

		mPhysicalDevice = selected.device;
		mGraphicsQueueFamily = selected.queueFamily;
		mApiVersion = selected.apiVersion;
		mDynamicRenderingPath = selected.dynamicRendering;
		mSynchronizationPath = selected.synchronization;
		LOG_INFO("VulkanContext") << "Selected GPU: " << selected.properties.deviceName
			<< " (api " << VK_VERSION_MAJOR(mApiVersion)
			<< "." << VK_VERSION_MINOR(mApiVersion)
			<< "." << VK_VERSION_PATCH(mApiVersion) << ")";
		if (mForceVulkan12 || mForceLegacyRendering || mForceLegacySynchronization) {
			LOG_WARN("VulkanContext") << "Vulkan compatibility overrides active: api="
				<< VK_VERSION_MAJOR(mApiVersion) << "." << VK_VERSION_MINOR(mApiVersion)
				<< " dynamicRendering=" << DynamicRenderingPathName()
				<< " synchronization=" << SynchronizationPathName();
		}
		else if (!UsesDynamicRendering() || !UsesSynchronization2()) {
			LOG_WARN("VulkanContext") << "Using Vulkan 1.2 compatibility path: dynamicRendering="
				<< DynamicRenderingPathName() << " synchronization=" << SynchronizationPathName();
		}
		return true;
	}

	bool VulkanContext::CreateLogicalDevice() {
		float prio = 1.0f;
		VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		qci.queueFamilyIndex = mGraphicsQueueFamily;
		qci.queueCount = 1;
		qci.pQueuePriorities = &prio;

		VkPhysicalDeviceVulkan13Features f13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		f13.dynamicRendering = mDynamicRenderingPath == FeaturePath::Core13;
		f13.synchronization2 = mSynchronizationPath == FeaturePath::Core13;

		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRendering{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR };
		dynamicRendering.dynamicRendering = mDynamicRenderingPath == FeaturePath::KhrExtension;
		VkPhysicalDeviceSynchronization2FeaturesKHR synchronization{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR };
		synchronization.synchronization2 = mSynchronizationPath == FeaturePath::KhrExtension;

		VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		f12.descriptorIndexing = VK_TRUE;
		f12.runtimeDescriptorArray = VK_TRUE;
		f12.descriptorBindingPartiallyBound = VK_TRUE;
		f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		f12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		if (mDynamicRenderingPath == FeaturePath::Core13 ||
			mSynchronizationPath == FeaturePath::Core13) {
			f12.pNext = &f13;
		}
		else if (mDynamicRenderingPath == FeaturePath::KhrExtension) {
			f12.pNext = &dynamicRendering;
			dynamicRendering.pNext = mSynchronizationPath == FeaturePath::KhrExtension
				? &synchronization : nullptr;
		}
		else if (mSynchronizationPath == FeaturePath::KhrExtension) {
			f12.pNext = &synchronization;
		}

		VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		f2.pNext = &f12;

		std::vector<const char*> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		if (mDynamicRenderingPath == FeaturePath::KhrExtension)
			extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		if (mSynchronizationPath == FeaturePath::KhrExtension)
			extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

		VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		dci.pNext = &f2;
		dci.queueCreateInfoCount = 1;
		dci.pQueueCreateInfos = &qci;
		dci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		dci.ppEnabledExtensionNames = extensions.data();

		VK_CHECK(vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice));
		// 本项目只创建一个 VkDevice；直接加载 device 表可绕过 loader 的逐调用分发。
		volkLoadDevice(mDevice);
		if ((mDynamicRenderingPath == FeaturePath::Core13 &&
			(!vkCmdBeginRendering || !vkCmdEndRendering)) ||
			(mDynamicRenderingPath == FeaturePath::KhrExtension &&
				(!vkCmdBeginRenderingKHR || !vkCmdEndRenderingKHR)) ||
			(mSynchronizationPath == FeaturePath::Core13 &&
				(!vkCmdPipelineBarrier2 || !vkQueueSubmit2)) ||
			(mSynchronizationPath == FeaturePath::KhrExtension &&
				(!vkCmdPipelineBarrier2KHR || !vkQueueSubmit2KHR))) {
			LOG_ERROR("VulkanContext") << "Selected Vulkan feature entry points are missing";
			return false;
		}
		vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
		return true;
	}

	bool VulkanContext::CreateSwapchain(SDL_Window* window, bool vsync) {
		VkSurfaceCapabilitiesKHR caps{};
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps));

		// 选 surface 格式：优先 B8G8R8A8_UNORM（线性，匹配现有 GL 行为，不做 sRGB 偏移）
		uint32_t fcount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &fcount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(fcount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &fcount, formats.data());

		VkSurfaceFormatKHR chosen = formats[0];
		for (const auto& f : formats) {
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
				f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				chosen = f;
				break;
			}
		}
		mSwapchainFormat = chosen.format;

		// 大小：Vulkan 推荐用 surface caps.currentExtent；如果是 0xFFFFFFFF，则用窗口大小
		if (caps.currentExtent.width != UINT32_MAX) {
			mSwapchainExtent = caps.currentExtent;
		}
		else {
			int w = 0, h = 0;
			SDL_Vulkan_GetDrawableSize(window, &w, &h);
			mSwapchainExtent.width = std::clamp((uint32_t)w, caps.minImageExtent.width, caps.maxImageExtent.width);
			mSwapchainExtent.height = std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height);
		}

		uint32_t imageCount = caps.minImageCount + 1;
		if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

		// Present mode：vsync=true 必走 FIFO（spec 强制支持）；vsync=false 优先 MAILBOX、其次 IMMEDIATE，
		// 都没有再回落 FIFO。
		uint32_t pmCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &pmCount, nullptr));
		std::vector<VkPresentModeKHR> modes(pmCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &pmCount, modes.data()));
		auto has = [&](VkPresentModeKHR m) {
			return std::find(modes.begin(), modes.end(), m) != modes.end();
			};

		VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		if (!vsync) {
			if (has(VK_PRESENT_MODE_MAILBOX_KHR))        chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		sci.surface = mSurface;
		sci.minImageCount = imageCount;
		sci.imageFormat = chosen.format;
		sci.imageColorSpace = chosen.colorSpace;
		sci.imageExtent = mSwapchainExtent;
		sci.imageArrayLayers = 1;
		// TRANSFER_SRC：AutoTest 截图回读用；规范不保证支持，按 supportedUsageFlags 能力位探测
		sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		mSwapchainTransferSrc = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
		if (mSwapchainTransferSrc)
			sci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sci.preTransform = caps.currentTransform;
		sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		sci.presentMode = chosenPresentMode;
		sci.clipped = VK_TRUE;

		VK_CHECK(vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain));

		uint32_t actual = 0;
		vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual, nullptr);
		mSwapchainImages.resize(actual);
		vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual, mSwapchainImages.data());

		mSwapchainImageViews.resize(actual);
		for (uint32_t i = 0; i < actual; ++i) {
			VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			vci.image = mSwapchainImages[i];
			vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vci.format = chosen.format;
			vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			VK_CHECK(vkCreateImageView(mDevice, &vci, nullptr, &mSwapchainImageViews[i]));
		}
		return CreateLegacyRenderTargets();
	}

	bool VulkanContext::CreateLegacyRenderTargets() {
		if (UsesDynamicRendering()) return true;

		// RenderPass 只描述格式与附件生命周期，swapchain 重建通常只需重建 framebuffer。
		// 若 surface 格式真的变化，旧 pipeline 也已不兼容，明确失败比静默错配更安全。
		if (!mLegacyRenderPass) {
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = mSwapchainFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference{};
			colorReference.attachment = 0;
			colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorReference;

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			VK_CHECK(vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mLegacyRenderPass));
			mLegacyRenderPassFormat = mSwapchainFormat;
		}
		else if (mLegacyRenderPassFormat != mSwapchainFormat) {
			LOG_ERROR("VulkanContext") << "Swapchain format changed while using legacy RenderPass";
			return false;
		}

		mLegacyFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < mSwapchainImageViews.size(); ++i) {
			VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferInfo.renderPass = mLegacyRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &mSwapchainImageViews[i];
			framebufferInfo.width = mSwapchainExtent.width;
			framebufferInfo.height = mSwapchainExtent.height;
			framebufferInfo.layers = 1;
			VK_CHECK(vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr,
				&mLegacyFramebuffers[i]));
		}
		return true;
	}

	bool VulkanContext::CreateAllocator() {
		// 只把两级查询入口交给 VMA；VMA 根据实际 apiVersion 动态取得其余函数，避免
		// VMA 或 Vulkan SDK 头版本变化时重新把 vulkan-1.dll 符号写入 EXE 导入表。
		VmaVulkanFunctions functions{};
		functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo aci{};
		aci.vulkanApiVersion = mApiVersion;
		aci.instance = mInstance;
		aci.physicalDevice = mPhysicalDevice;
		aci.device = mDevice;
		aci.pVulkanFunctions = &functions;

		VK_CHECK(vmaCreateAllocator(&aci, &mAllocator));
		return true;
	}

	void VulkanContext::DestroySwapchain() {
		DestroyLegacyFramebuffers();
		for (auto v : mSwapchainImageViews) if (v) vkDestroyImageView(mDevice, v, nullptr);
		mSwapchainImageViews.clear();
		mSwapchainImages.clear();
		if (mSwapchain) { vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr); mSwapchain = VK_NULL_HANDLE; }
	}

	void VulkanContext::DestroyLegacyFramebuffers() {
		for (auto framebuffer : mLegacyFramebuffers) {
			if (framebuffer) vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
		}
		mLegacyFramebuffers.clear();
	}

	bool VulkanContext::UsesDynamicRendering() const {
		return mDynamicRenderingPath != FeaturePath::Legacy;
	}

	bool VulkanContext::UsesSynchronization2() const {
		return mSynchronizationPath != FeaturePath::Legacy;
	}

	const char* VulkanContext::DynamicRenderingPathName() const {
		switch (mDynamicRenderingPath) {
		case FeaturePath::Core13:      return "core-1.3";
		case FeaturePath::KhrExtension:return "KHR-extension";
		default:                       return "legacy-render-pass";
		}
	}

	const char* VulkanContext::SynchronizationPathName() const {
		switch (mSynchronizationPath) {
		case FeaturePath::Core13:      return "core-1.3";
		case FeaturePath::KhrExtension:return "KHR-extension";
		default:                       return "legacy";
		}
	}

	VkFramebuffer VulkanContext::LegacyFramebuffer(uint32_t imageIndex) const {
		return imageIndex < mLegacyFramebuffers.size()
			? mLegacyFramebuffers[imageIndex] : VK_NULL_HANDLE;
	}

	void VulkanContext::CmdBeginRendering(VkCommandBuffer commandBuffer,
		const VkRenderingInfo& renderingInfo) const {
		if (mDynamicRenderingPath == FeaturePath::Core13)
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
		else if (mDynamicRenderingPath == FeaturePath::KhrExtension)
			vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);
	}

	void VulkanContext::CmdEndRendering(VkCommandBuffer commandBuffer) const {
		if (mDynamicRenderingPath == FeaturePath::Core13)
			vkCmdEndRendering(commandBuffer);
		else if (mDynamicRenderingPath == FeaturePath::KhrExtension)
			vkCmdEndRenderingKHR(commandBuffer);
	}

	void VulkanContext::CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image,
		VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
		VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		const VkImageSubresourceRange& subresourceRange) const {
		if (UsesSynchronization2()) {
			VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
			barrier.srcStageMask = srcStage;
			barrier.srcAccessMask = srcAccess;
			barrier.dstStageMask = dstStage;
			barrier.dstAccessMask = dstAccess;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange = subresourceRange;

			VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dependency.imageMemoryBarrierCount = 1;
			dependency.pImageMemoryBarriers = &barrier;
			if (mSynchronizationPath == FeaturePath::Core13)
				vkCmdPipelineBarrier2(commandBuffer, &dependency);
			else
				vkCmdPipelineBarrier2KHR(commandBuffer, &dependency);
			return;
		}

		VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.srcAccessMask = ToLegacyAccessMask(srcAccess);
		barrier.dstAccessMask = ToLegacyAccessMask(dstAccess);
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;
		vkCmdPipelineBarrier(commandBuffer,
			ToLegacyStageMask(srcStage), ToLegacyStageMask(dstStage), 0,
			0, nullptr, 0, nullptr, 1, &barrier);
	}

	VkResult VulkanContext::SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkFence fence,
		VkSemaphore waitSemaphore, VkPipelineStageFlags2 waitStage,
		VkSemaphore signalSemaphore, VkPipelineStageFlags2 signalStage) const {
		if (UsesSynchronization2()) {
			VkCommandBufferSubmitInfo commandInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
			commandInfo.commandBuffer = commandBuffer;

			VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			waitInfo.semaphore = waitSemaphore;
			waitInfo.stageMask = waitStage;
			VkSemaphoreSubmitInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			signalInfo.semaphore = signalSemaphore;
			signalInfo.stageMask = signalStage;

			VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
			submit.waitSemaphoreInfoCount = waitSemaphore ? 1u : 0u;
			submit.pWaitSemaphoreInfos = waitSemaphore ? &waitInfo : nullptr;
			submit.commandBufferInfoCount = 1;
			submit.pCommandBufferInfos = &commandInfo;
			submit.signalSemaphoreInfoCount = signalSemaphore ? 1u : 0u;
			submit.pSignalSemaphoreInfos = signalSemaphore ? &signalInfo : nullptr;
			return mSynchronizationPath == FeaturePath::Core13
				? vkQueueSubmit2(mGraphicsQueue, 1, &submit, fence)
				: vkQueueSubmit2KHR(mGraphicsQueue, 1, &submit, fence);
		}

		VkPipelineStageFlags legacyWaitStage = ToLegacyStageMask(waitStage);
		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
		submit.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr;
		submit.pWaitDstStageMask = waitSemaphore ? &legacyWaitStage : nullptr;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &commandBuffer;
		submit.signalSemaphoreCount = signalSemaphore ? 1u : 0u;
		submit.pSignalSemaphores = signalSemaphore ? &signalSemaphore : nullptr;
		return vkQueueSubmit(mGraphicsQueue, 1, &submit, fence);
	}

	bool VulkanContext::RecreateSwapchain(bool vsync) {
		if (!mInitialized || !mDevice || !mWindow) return false;

		// 窗口最小化/隐藏时 surface 尺寸为 {0,0}，此时 vkCreateSwapchainKHR 会失败。
		// 不销毁现有 swapchain，让调用方在下一帧重试，直到窗口恢复。
		VkSurfaceCapabilitiesKHR caps{};
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps) == VK_SUCCESS) {
			if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
				return false;
			}
		}

		vkDeviceWaitIdle(mDevice);
		DestroySwapchain();
		if (!CreateSwapchain(mWindow, vsync)) {
			LOG_ERROR("VulkanContext") << "RecreateSwapchain failed";
			return false;
		}
		return true;
	}

	void VulkanContext::Shutdown() {
		if (!mVolkInitialized && !mInstance && !mDevice && !mAllocator) return;

		if (mDevice) vkDeviceWaitIdle(mDevice);

		if (mAllocator) { vmaDestroyAllocator(mAllocator);    mAllocator = VK_NULL_HANDLE; }
		DestroySwapchain();
		if (mLegacyRenderPass) {
			vkDestroyRenderPass(mDevice, mLegacyRenderPass, nullptr);
			mLegacyRenderPass = VK_NULL_HANDLE;
			mLegacyRenderPassFormat = VK_FORMAT_UNDEFINED;
		}
		if (mDevice) { vkDestroyDevice(mDevice, nullptr);  mDevice = VK_NULL_HANDLE; }
		if (mSurface) { vkDestroySurfaceKHR(mInstance, mSurface, nullptr); mSurface = VK_NULL_HANDLE; }
		if (mDebugMessenger) {
			auto fnDestroy = (PFN_vkDestroyDebugUtilsMessengerEXT)
				vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
			if (fnDestroy) fnDestroy(mInstance, mDebugMessenger, nullptr);
			mDebugMessenger = VK_NULL_HANDLE;
		}
		if (mInstance) { vkDestroyInstance(mInstance, nullptr); mInstance = VK_NULL_HANDLE; }

		mInitialized = false;
		mLoaderApiVersion = 0;
		mInstanceApiVersion = 0;
		mApiVersion = 0;
		mDynamicRenderingPath = FeaturePath::Legacy;
		mSynchronizationPath = FeaturePath::Legacy;
		mForceVulkan12 = false;
		mForceLegacyRendering = false;
		mForceLegacySynchronization = false;
		mWindow = nullptr;
		if (mVolkInitialized) {
			volkFinalize();
			mVolkInitialized = false;
		}
	}
} // namespace pvz
