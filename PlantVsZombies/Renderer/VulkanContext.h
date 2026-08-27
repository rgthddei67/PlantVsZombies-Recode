#pragma once

// Volk 只声明动态函数指针；运行时 loader 由 SDL2 选定并提供 vkGetInstanceProcAddr。
#include <volk.h>
#include <vma/vk_mem_alloc.h>

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;

namespace pvz {
	// 拥有从 instance 到 swapchain 的全部 Vulkan 设备级对象。
	// 不负责具体的帧渲染——那是 VulkanRenderer 的职责。
	class VulkanContext {
	public:
		VulkanContext();
		~VulkanContext();

		VulkanContext(const VulkanContext&) = delete;
		VulkanContext& operator=(const VulkanContext&) = delete;

		/**
		 * @brief 创建 Vulkan instance/device/swapchain，并按设备能力选择 1.3、KHR 或传统路径。
		 * @param forceVulkan12 把 instance API 协商上限限制为 1.2，供兼容矩阵测试。
		 * @param forceLegacyRendering 屏蔽 dynamic rendering，强制传统 RenderPass。
		 * @param forceLegacySynchronization 屏蔽 synchronization2，强制传统屏障与提交。
		 */
		bool Initialize(SDL_Window* window, bool enableValidation, bool vsync,
			bool forceVulkan12 = false,
			bool forceLegacyRendering = false,
			bool forceLegacySynchronization = false);
		void Shutdown();

		// 热切换 swapchain：销毁旧 swapchain/image views，按新 vsync 选 present mode 重建。
		// 调用者负责在调用前后处理 renderer 端 per-swapchain-image 资源（VulkanRenderer::OnSwapchainRecreated）。
		bool RecreateSwapchain(bool vsync);

		bool IsInitialized() const { return mInitialized; }
		/** 初始化失败时保留阶段、原始 SDL 错误或 VkResult，供 auto 回退日志使用。 */
		const std::string& LastError() const { return mLastError; }

		// 给后续 phase 的渲染层用的访问器
		VkInstance       Instance()        const { return mInstance; }
		VkPhysicalDevice PhysicalDevice()  const { return mPhysicalDevice; }
		VkDevice         Device()          const { return mDevice; }
		VkQueue          GraphicsQueue()   const { return mGraphicsQueue; }
		uint32_t         GraphicsQueueFamily() const { return mGraphicsQueueFamily; }
		VkSurfaceKHR     Surface()         const { return mSurface; }
		VmaAllocator     Allocator()       const { return mAllocator; }
		uint32_t         ApiVersion()      const { return mApiVersion; }

		bool UsesDynamicRendering() const;
		bool UsesSynchronization2() const;
		/** 返回当前设备是否已启用 pipeline statistics query 诊断能力。 */
		bool UsesPipelineStatisticsQuery() const { return mPipelineStatisticsQueryEnabled; }
		const char* DynamicRenderingPathName() const;
		const char* SynchronizationPathName() const;

		// 传统 RenderPass 回退专用；dynamic rendering 路径返回 VK_NULL_HANDLE。
		VkRenderPass LegacyRenderPass() const { return mLegacyRenderPass; }
		VkFramebuffer LegacyFramebuffer(uint32_t imageIndex) const;

		/**
		 * @brief 按当前能力路径录制 dynamic rendering 的开始/结束命令。
		 * @details Vulkan 1.3 使用核心入口，Vulkan 1.2 使用 KHR 入口；传统 RenderPass
		 *          路径不得调用这两个接口。
		 */
		void CmdBeginRendering(VkCommandBuffer commandBuffer,
			const VkRenderingInfo& renderingInfo) const;
		void CmdEndRendering(VkCommandBuffer commandBuffer) const;

		/**
		 * @brief 为颜色图像录制布局与可见性屏障，并自动选择 sync2 或传统同步接口。
		 * @details 传统路径会把本项目使用的细粒度 BLIT/ALL_TRANSFER 和
		 *          SHADER_SAMPLED_READ 标志保守映射到 Vulkan 1.0 等价标志。
		 */
		void CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image,
			VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
			VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			const VkImageSubresourceRange& subresourceRange) const;

		/**
		 * @brief 提交一个命令缓冲，并按当前路径选择 Submit2 核心/KHR 或传统 vkQueueSubmit。
		 * @param waitStage 仅 waitSemaphore 非空时使用；传统路径会映射为 32 位 stage mask。
		 * @param signalStage Submit2 的信号阶段；传统提交没有对应字段，会按完整提交完成语义处理。
		 */
		VkResult SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkFence fence,
			VkSemaphore waitSemaphore = VK_NULL_HANDLE,
			VkPipelineStageFlags2 waitStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VkSemaphore signalSemaphore = VK_NULL_HANDLE,
			VkPipelineStageFlags2 signalStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) const;

		VkSwapchainKHR   Swapchain()       const { return mSwapchain; }
		VkFormat         SwapchainFormat() const { return mSwapchainFormat; }
		VkExtent2D       SwapchainExtent() const { return mSwapchainExtent; }
		const std::vector<VkImage>& SwapchainImages()     const { return mSwapchainImages; }
		const std::vector<VkImageView>& SwapchainImageViews() const { return mSwapchainImageViews; }
		// AutoTest 截图：交换链是否实际授予了 TRANSFER_SRC 用途（规范不保证，按 supportedUsageFlags 探测）
		bool SwapchainSupportsTransferSrc() const { return mSwapchainTransferSrc; }

	private:
		/**
		 * @brief 使用 SDL2 已加载的 Vulkan loader 初始化 Volk 全局分发表。
		 */
		bool InitializeVulkanLoader();
		bool CreateInstance(SDL_Window* window, bool enableValidation);
		bool CreateDebugMessenger();
		bool CreateSurface(SDL_Window* window);
		bool PickPhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSwapchain(SDL_Window* window, bool vsync);
		bool CreateAllocator();
		// 为传统渲染路径创建持久 RenderPass 与逐 swapchain image framebuffer。
		bool CreateLegacyRenderTargets();

		void DestroySwapchain();
		void DestroyLegacyFramebuffers();

		enum class FeaturePath : uint8_t {
			Legacy,
			KhrExtension,
			Core13,
		};

		bool                       mInitialized = false;
		std::string                mLastError;
		bool                       mValidationEnabled = false;
		bool                       mVolkInitialized = false;
		bool                       mForceVulkan12 = false;
		bool                       mForceLegacyRendering = false;
		bool                       mForceLegacySynchronization = false;
		uint32_t                   mLoaderApiVersion = 0;
		uint32_t                   mInstanceApiVersion = 0;
		uint32_t                   mApiVersion = 0;
		FeaturePath                mDynamicRenderingPath = FeaturePath::Legacy;
		FeaturePath                mSynchronizationPath = FeaturePath::Legacy;
		bool                       mPipelineStatisticsQueryEnabled = false;

		SDL_Window* mWindow = nullptr;  // Recreate 时复用

		VkInstance                 mInstance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT   mDebugMessenger = VK_NULL_HANDLE;
		VkSurfaceKHR               mSurface = VK_NULL_HANDLE;
		VkPhysicalDevice           mPhysicalDevice = VK_NULL_HANDLE;
		VkDevice                   mDevice = VK_NULL_HANDLE;
		VkQueue                    mGraphicsQueue = VK_NULL_HANDLE;
		uint32_t                   mGraphicsQueueFamily = UINT32_MAX;

		VkSwapchainKHR             mSwapchain = VK_NULL_HANDLE;
		VkFormat                   mSwapchainFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D                 mSwapchainExtent{ 0, 0 };
		std::vector<VkImage>       mSwapchainImages;
		std::vector<VkImageView>   mSwapchainImageViews;
		bool                       mSwapchainTransferSrc = false;  // CreateSwapchain 按 caps 探测
		VkRenderPass               mLegacyRenderPass = VK_NULL_HANDLE;
		VkFormat                   mLegacyRenderPassFormat = VK_FORMAT_UNDEFINED;
		std::vector<VkFramebuffer> mLegacyFramebuffers;

		VmaAllocator               mAllocator = VK_NULL_HANDLE;
	};
} // namespace pvz
