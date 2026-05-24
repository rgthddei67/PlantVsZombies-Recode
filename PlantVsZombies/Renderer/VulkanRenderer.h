#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace pvz {
	class VulkanContext;
	class VulkanPipeline;

	// Phase 1.5 — 持有命令池、命令缓冲、同步原语，跑 acquire→record→submit→present 循环。
	// Phase 2a — 可选挂一个 pipeline，每帧画 3 个顶点的三角形。
	class VulkanRenderer {
	public:
		static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

		VulkanRenderer();
		~VulkanRenderer();

		VulkanRenderer(const VulkanRenderer&) = delete;
		VulkanRenderer& operator=(const VulkanRenderer&) = delete;

		bool Initialize(VulkanContext* ctx);
		void Shutdown();

		// swapchain 热重建后调用：销毁旧的 per-image renderFinished 信号量、按新的 image 数重建。
		// 必须在 vkDeviceWaitIdle 后、下次 BeginFrame 之前调用。
		bool OnSwapchainRecreated();

		// Phase 3b — 帧生命周期拆分。
		// BeginFrame: wait fence + acquire image + reset pool + begin cb + barrier + beginRendering + setViewport/Scissor。
		//             成功后 CurrentCmdBuffer() 返回本帧 cb，调用方可往里录命令；失败返回 false 时调用方不应再调用 EndFrame。
		// EndFrame:   endRendering + barrier → PRESENT_SRC + endCb + submit2 + present。失败返回 false。
		bool BeginFrame(float r, float g, float b, float a);
		bool EndFrame();

		VkCommandBuffer CurrentCmdBuffer() const;
		uint32_t        CurrentFrameIdx() const { return mFrameIdx; }

		// 当 acquire / present 报 OUT_OF_DATE / SUBOPTIMAL 时被置 true。
		// 上层应在帧外调用 VulkanContext::RecreateSwapchain + OnSwapchainRecreated 后调 Clear。
		bool NeedsSwapchainRebuild() const { return mSwapchainNeedsRebuild; }
		void ClearSwapchainRebuildFlag() { mSwapchainNeedsRebuild = false; }

	private:
		struct PerFrame {
			VkCommandPool   cmdPool = VK_NULL_HANDLE;
			VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
			VkSemaphore     imageAvailable = VK_NULL_HANDLE;  // signaled by acquire
			VkFence         inFlight = VK_NULL_HANDLE;  // CPU↔GPU
		};

		bool CreateFrameResources();
		void DestroyFrameResources();

		// 当前帧 acquire 到的 swapchain image 索引；仅在 BeginFrame..EndFrame 之间有效。
		uint32_t mAcquiredImageIdx = UINT32_MAX;
		bool     mFrameActive = false;

		VulkanContext* mCtx = nullptr;

		std::array<PerFrame, FRAMES_IN_FLIGHT> mFrames;
		std::vector<VkSemaphore> mRenderFinished;  // 每张 swapchain image 一个（用于 present 时等待）

		uint32_t mFrameIdx = 0;
		bool     mSwapchainNeedsRebuild = false;
	};
} // namespace pvz
