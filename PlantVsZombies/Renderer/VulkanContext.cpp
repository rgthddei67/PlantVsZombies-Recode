#include "VulkanContext.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace pvz {

namespace {

// 用打印替代异常，让 Phase 1 烟雾测试容易调试。
#define VK_CHECK(expr)                                                      \
    do {                                                                    \
        VkResult _r = (expr);                                               \
        if (_r != VK_SUCCESS) {                                             \
            std::fprintf(stderr, "[Vulkan] %s failed (VkResult=%d)\n",      \
                         #expr, (int)_r);                                   \
            return false;                                                   \
        }                                                                   \
    } while (0)

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT        /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                  /*userData*/) {
    const char* sevTag = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)        sevTag = "ERROR";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) sevTag = "WARN";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) sevTag = "VERBOSE";
    std::fprintf(stderr, "[Vulkan %s] %s\n", sevTag,
                 data && data->pMessage ? data->pMessage : "(no message)");
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

} // anonymous namespace

VulkanContext::VulkanContext() = default;
VulkanContext::~VulkanContext() { Shutdown(); }

bool VulkanContext::Initialize(SDL_Window* window, bool enableValidation) {
    mValidationEnabled = enableValidation;

    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "[Vulkan] volkInitialize failed — vulkan-1.dll not found?\n");
        return false;
    }

    if (!CreateInstance(window, enableValidation)) return false;
    volkLoadInstance(mInstance);

    if (enableValidation && !CreateDebugMessenger()) return false;
    if (!CreateSurface(window))                     return false;
    if (!PickPhysicalDevice())                      return false;
    if (!CreateLogicalDevice())                     return false;
    volkLoadDevice(mDevice);

    if (!CreateSwapchain(window))                   return false;
    if (!CreateAllocator())                         return false;

    mInitialized = true;

    std::fprintf(stdout,
        "[Vulkan] Ready. swapchain=%ux%u format=%d images=%zu validation=%d\n",
        mSwapchainExtent.width, mSwapchainExtent.height,
        (int)mSwapchainFormat, mSwapchainImages.size(),
        mValidationEnabled ? 1 : 0);

    return true;
}

bool VulkanContext::CreateInstance(SDL_Window* window, bool enableValidation) {
    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName   = "PlantsVsZombies";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName        = "PVZ-Vulkan";
    app.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion         = VK_API_VERSION_1_3;

    // SDL 帮我们列出 surface 相关扩展（platform-specific）
    uint32_t sdlExtCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtCount, nullptr)) {
        std::fprintf(stderr, "[Vulkan] SDL_Vulkan_GetInstanceExtensions(count) failed: %s\n", SDL_GetError());
        return false;
    }
    std::vector<const char*> extensions(sdlExtCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtCount, extensions.data())) {
        std::fprintf(stderr, "[Vulkan] SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        return false;
    }
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::vector<const char*> layers;
    if (enableValidation) {
        if (HasLayer("VK_LAYER_KHRONOS_validation")) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        } else {
            std::fprintf(stderr, "[Vulkan] WARN: validation requested but VK_LAYER_KHRONOS_validation not installed (install VulkanSDK).\n");
            mValidationEnabled = false;
        }
    }

    VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    info.pApplicationInfo        = &app;
    info.enabledExtensionCount   = (uint32_t)extensions.size();
    info.ppEnabledExtensionNames = extensions.data();
    info.enabledLayerCount       = (uint32_t)layers.size();
    info.ppEnabledLayerNames     = layers.data();

    VK_CHECK(vkCreateInstance(&info, nullptr, &mInstance));
    return true;
}

bool VulkanContext::CreateDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info, nullptr, &mDebugMessenger));
    return true;
}

bool VulkanContext::CreateSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, mInstance, &mSurface)) {
        std::fprintf(stderr, "[Vulkan] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

bool VulkanContext::PickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    if (count == 0) {
        std::fprintf(stderr, "[Vulkan] No Vulkan-capable GPU found.\n");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

    auto isSuitable = [this](VkPhysicalDevice dev, uint32_t& outQueueFamily) -> bool {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.apiVersion < VK_API_VERSION_1_3) return false;

        // 必须的 1.2 / 1.3 feature
        VkPhysicalDeviceVulkan13Features f13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        f12.pNext = &f13;
        VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        f2.pNext = &f12;
        vkGetPhysicalDeviceFeatures2(dev, &f2);

        if (!f13.dynamicRendering || !f13.synchronization2) return false;
        if (!f12.descriptorIndexing) return false;
        if (!f12.runtimeDescriptorArray) return false;
        if (!f12.descriptorBindingPartiallyBound) return false;
        if (!f12.descriptorBindingSampledImageUpdateAfterBind) return false;
        if (!f12.shaderSampledImageArrayNonUniformIndexing) return false;
        if (!f12.descriptorBindingVariableDescriptorCount) return false;

        // 找到一个同时支持 graphics 和 present 的队列家族
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qs(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qs.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            if (!(qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            VkBool32 supportsPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mSurface, &supportsPresent);
            if (supportsPresent) { outQueueFamily = i; return true; }
        }
        return false;
    };

    // 优先 discrete GPU
    for (auto type : { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
                       VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
                       VK_PHYSICAL_DEVICE_TYPE_OTHER }) {
        for (auto dev : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (props.deviceType != type && type != VK_PHYSICAL_DEVICE_TYPE_OTHER) continue;
            uint32_t qf = UINT32_MAX;
            if (isSuitable(dev, qf)) {
                mPhysicalDevice = dev;
                mGraphicsQueueFamily = qf;
                std::fprintf(stdout, "[Vulkan] Selected GPU: %s (api %u.%u.%u)\n",
                             props.deviceName,
                             VK_VERSION_MAJOR(props.apiVersion),
                             VK_VERSION_MINOR(props.apiVersion),
                             VK_VERSION_PATCH(props.apiVersion));
                return true;
            }
        }
    }
    std::fprintf(stderr, "[Vulkan] No GPU meets feature requirements (Vulkan 1.3, dynamic rendering, descriptor indexing).\n");
    return false;
}

bool VulkanContext::CreateLogicalDevice() {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = mGraphicsQueueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    VkPhysicalDeviceVulkan13Features f13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    f13.dynamicRendering  = VK_TRUE;
    f13.synchronization2  = VK_TRUE;

    VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    f12.pNext = &f13;
    f12.descriptorIndexing                              = VK_TRUE;
    f12.runtimeDescriptorArray                          = VK_TRUE;
    f12.descriptorBindingPartiallyBound                 = VK_TRUE;
    f12.descriptorBindingSampledImageUpdateAfterBind    = VK_TRUE;
    f12.shaderSampledImageArrayNonUniformIndexing       = VK_TRUE;
    f12.descriptorBindingVariableDescriptorCount        = VK_TRUE;

    VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    f2.pNext = &f12;

    const char* exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.pNext                   = &f2;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = exts;

    VK_CHECK(vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice));
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
    return true;
}

bool VulkanContext::CreateSwapchain(SDL_Window* window) {
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
    } else {
        int w = 0, h = 0;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        mSwapchainExtent.width  = std::clamp((uint32_t)w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        mSwapchainExtent.height = std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface          = mSurface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = chosen.format;
    sci.imageColorSpace  = chosen.colorSpace;
    sci.imageExtent      = mSwapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR;  // 保底支持，无需查询
    sci.clipped          = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain));

    uint32_t actual = 0;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual, nullptr);
    mSwapchainImages.resize(actual);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual, mSwapchainImages.data());

    mSwapchainImageViews.resize(actual);
    for (uint32_t i = 0; i < actual; ++i) {
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image    = mSwapchainImages[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = chosen.format;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(mDevice, &vci, nullptr, &mSwapchainImageViews[i]));
    }
    return true;
}

bool VulkanContext::CreateAllocator() {
    // VMA 需要拿到我们已用 volk 加载的函数指针，否则会自己 dlsym 出问题（因为 VK_NO_PROTOTYPES）
    VmaVulkanFunctions fns{};
    fns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    fns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo aci{};
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    aci.instance         = mInstance;
    aci.physicalDevice   = mPhysicalDevice;
    aci.device           = mDevice;
    aci.pVulkanFunctions = &fns;

    VK_CHECK(vmaCreateAllocator(&aci, &mAllocator));
    return true;
}

void VulkanContext::DestroySwapchain() {
    for (auto v : mSwapchainImageViews) if (v) vkDestroyImageView(mDevice, v, nullptr);
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();
    if (mSwapchain) { vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr); mSwapchain = VK_NULL_HANDLE; }
}

void VulkanContext::Shutdown() {
    if (!mInitialized) return;

    if (mDevice) vkDeviceWaitIdle(mDevice);

    if (mAllocator)      { vmaDestroyAllocator(mAllocator);    mAllocator = VK_NULL_HANDLE; }
    DestroySwapchain();
    if (mDevice)         { vkDestroyDevice(mDevice, nullptr);  mDevice = VK_NULL_HANDLE; }
    if (mSurface)        { vkDestroySurfaceKHR(mInstance, mSurface, nullptr); mSurface = VK_NULL_HANDLE; }
    if (mDebugMessenger) { vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr); mDebugMessenger = VK_NULL_HANDLE; }
    if (mInstance)       { vkDestroyInstance(mInstance, nullptr); mInstance = VK_NULL_HANDLE; }

    volkFinalize();
    mInitialized = false;
}

} // namespace pvz
