#include "VulkanRenderer.h"
#include "VulkanContext.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "../Logger.h"
#include <SDL2/SDL_image.h>
#include <vector>
#include <cstring>

namespace pvz {
	namespace {
#define VK_CHECK(expr)                                                      \
    do {                                                                    \
        VkResult _r = (expr);                                               \
        if (_r != VK_SUCCESS) {                                             \
            LOG_ERROR("VulkanRenderer") << #expr " failed (VkResult=" << (int)_r << ")"; \
            return false;                                                   \
        }                                                                   \
    } while (0)

		VkSemaphore MakeSemaphore(VkDevice dev) {
			VkSemaphoreCreateInfo ci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VkSemaphore s = VK_NULL_HANDLE;
			vkCreateSemaphore(dev, &ci, nullptr, &s);
			return s;
		}

		VkFence MakeFenceSignaled(VkDevice dev) {
			VkFenceCreateInfo ci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 第一帧能立刻通过 wait
			VkFence f = VK_NULL_HANDLE;
			vkCreateFence(dev, &ci, nullptr, &f);
			return f;
		}

		// VK_KHR_synchronization2 风格的 image barrier helper
		void ImageBarrier2(VkCommandBuffer cb,
			VkImage image,
			VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
			VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
			VkImageLayout oldLayout, VkImageLayout newLayout) {
			VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
			b.srcStageMask = srcStage;
			b.srcAccessMask = srcAccess;
			b.dstStageMask = dstStage;
			b.dstAccessMask = dstAccess;
			b.oldLayout = oldLayout;
			b.newLayout = newLayout;
			b.image = image;
			b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dep.imageMemoryBarrierCount = 1;
			dep.pImageMemoryBarriers = &b;
			vkCmdPipelineBarrier2(cb, &dep);
		}
	} // anonymous

	VulkanRenderer::VulkanRenderer() = default;
	VulkanRenderer::~VulkanRenderer() { Shutdown(); }

	bool VulkanRenderer::Initialize(VulkanContext* ctx) {
		mCtx = ctx;
		return CreateFrameResources();
	}

	bool VulkanRenderer::CreateFrameResources() {
		VkDevice dev = mCtx->Device();

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
			PerFrame& pf = mFrames[i];

			VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			pci.queueFamilyIndex = mCtx->GraphicsQueueFamily();
			pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			VK_CHECK(vkCreateCommandPool(dev, &pci, nullptr, &pf.cmdPool));

			VkCommandBufferAllocateInfo aci{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			aci.commandPool = pf.cmdPool;
			aci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			aci.commandBufferCount = 1;
			VK_CHECK(vkAllocateCommandBuffers(dev, &aci, &pf.cmdBuffer));

			pf.imageAvailable = MakeSemaphore(dev);
			pf.inFlight = MakeFenceSignaled(dev);
		}

		// 每张 swapchain image 一个 renderFinished 信号量
		mRenderFinished.resize(mCtx->SwapchainImages().size());
		for (auto& s : mRenderFinished) s = MakeSemaphore(dev);

		return true;
	}

	void VulkanRenderer::DestroyFrameResources() {
		if (!mCtx) return;
		VkDevice dev = mCtx->Device();
		if (!dev) return;

		for (auto s : mRenderFinished) if (s) vkDestroySemaphore(dev, s, nullptr);
		mRenderFinished.clear();

		for (auto& pf : mFrames) {
			if (pf.inFlight) { vkDestroyFence(dev, pf.inFlight, nullptr);       pf.inFlight = VK_NULL_HANDLE; }
			if (pf.imageAvailable) { vkDestroySemaphore(dev, pf.imageAvailable, nullptr); pf.imageAvailable = VK_NULL_HANDLE; }
			if (pf.cmdPool) { vkDestroyCommandPool(dev, pf.cmdPool, nullptr);  pf.cmdPool = VK_NULL_HANDLE; pf.cmdBuffer = VK_NULL_HANDLE; }
		}
	}

	void VulkanRenderer::Shutdown() {
		if (mCtx && mCtx->Device()) {
			vkDeviceWaitIdle(mCtx->Device());
		}
		DestroyFrameResources();
		mCtx = nullptr;
	}

	bool VulkanRenderer::OnSwapchainRecreated() {
		if (!mCtx || !mCtx->Device()) return false;
		VkDevice dev = mCtx->Device();

		// image 数可能变化（例如 MAILBOX 通常返回 3，FIFO 可能 2/3）。
		// 销毁旧的 per-image renderFinished 信号量，按新的 image 数重建。
		for (auto s : mRenderFinished) if (s) vkDestroySemaphore(dev, s, nullptr);
		mRenderFinished.clear();

		mRenderFinished.resize(mCtx->SwapchainImages().size());
		for (auto& s : mRenderFinished) s = MakeSemaphore(dev);

		// 帧索引复位，避免新一轮 BeginFrame 用错 PerFrame 槽位。
		mFrameIdx = 0;
		mAcquiredImageIdx = UINT32_MAX;
		mFrameActive = false;
		return true;
	}

	bool VulkanRenderer::BeginFrame(float r, float g, float b, float a) {
		if (!mCtx || !mCtx->IsInitialized()) return false;
		if (mFrameActive) {
			LOG_WARN("VulkanRenderer") << "BeginFrame called twice without EndFrame";
			return false;
		}
		VkDevice dev = mCtx->Device();
		PerFrame& frame = mFrames[mFrameIdx];

		// 1) 等本帧上一次 GPU 提交结束
		vkWaitForFences(dev, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

		// 2) 拿下一张 swapchain image
		VkResult acquireResult = vkAcquireNextImageKHR(
			dev, mCtx->Swapchain(), UINT64_MAX,
			frame.imageAvailable, VK_NULL_HANDLE, &mAcquiredImageIdx);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			// swapchain 失效：标记需要重建，本帧跳过。fence 没 reset，下次照样能 wait。
			mSwapchainNeedsRebuild = true;
			return false;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			LOG_ERROR("VulkanRenderer") << "vkAcquireNextImageKHR failed (" << (int)acquireResult << ")";
			return false;
		}
		if (acquireResult == VK_SUBOPTIMAL_KHR) {
			// 还能用，本帧继续，但帧末触发重建。
			mSwapchainNeedsRebuild = true;
		}

		// 拿到 image 后才能 reset fence（避免死锁）
		vkResetFences(dev, 1, &frame.inFlight);

		// 3) 重置命令池并 begin
		vkResetCommandPool(dev, frame.cmdPool, 0);

		VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(frame.cmdBuffer, &bi));

		VkImage swapImage = mCtx->SwapchainImages()[mAcquiredImageIdx];
		ImageBarrier2(frame.cmdBuffer, swapImage,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		VkRenderingAttachmentInfo color{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		color.imageView = mCtx->SwapchainImageViews()[mAcquiredImageIdx];
		color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.clearValue.color = { { r, g, b, a } };

		VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		ri.renderArea = { {0, 0}, mCtx->SwapchainExtent() };
		ri.layerCount = 1;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &color;
		vkCmdBeginRendering(frame.cmdBuffer, &ri);

		// 默认 viewport / scissor 固定整帧；对象级 ClipRect 随顶点/实例进入 fragment shader。
		// 负高度 viewport：Vulkan 1.1+ 支持，效果是把 clip-space Y 翻转，让 PVZ 沿用
		// 的 GL 风格正交矩阵 (top=0, bottom=h) 渲染到 framebuffer 时方向正确。
		// y 偏到 height，height 取负，等价于绕屏幕水平线翻转。
		VkViewport vp{};
		vp.x = 0.0f;
		vp.y = (float)mCtx->SwapchainExtent().height;
		vp.width = (float)mCtx->SwapchainExtent().width;
		vp.height = -(float)mCtx->SwapchainExtent().height;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		VkRect2D scissorRect{ {0, 0}, mCtx->SwapchainExtent() };
		vkCmdSetViewport(frame.cmdBuffer, 0, 1, &vp);
		vkCmdSetScissor(frame.cmdBuffer, 0, 1, &scissorRect);

		mFrameActive = true;
		return true;
	}

	bool VulkanRenderer::EndFrame() {
		if (!mFrameActive) {
			LOG_WARN("VulkanRenderer") << "EndFrame called without BeginFrame";
			return false;
		}
		PerFrame& frame = mFrames[mFrameIdx];
		VkImage swapImage = mCtx->SwapchainImages()[mAcquiredImageIdx];

		vkCmdEndRendering(frame.cmdBuffer);

		// AutoTest 截图：present 之后图像归显示引擎所有，回读必须发生在 present 之前。
		VulkanBuffer captureBuf;   // 函数内 RAII：submit 后等 fence 再读，函数尾析构
		if (!mCapturePath.empty() && !mCtx->SwapchainSupportsTransferSrc()) {
			LOG_ERROR("VulkanRenderer") << "交换链不支持 TRANSFER_SRC，截图不可用";
			mCapturePath.clear();
		}
		const bool capturing = !mCapturePath.empty();
		const VkExtent2D captureExt = mCtx->SwapchainExtent();
		if (capturing) {
			ImageBarrier2(frame.cmdBuffer, swapImage,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			if (captureBuf.Create(mCtx,
				VkDeviceSize(captureExt.width) * captureExt.height * 4,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT, /*hostVisible=*/true, /*hostReadback=*/true)) {
				VkBufferImageCopy region{};   // bufferRowLength=0 → 行紧密排列
				region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
				region.imageExtent = { captureExt.width, captureExt.height, 1 };
				vkCmdCopyImageToBuffer(frame.cmdBuffer, swapImage,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, captureBuf.Handle(), 1, &region);
			}
			else {
				LOG_ERROR("VulkanRenderer") << "截图 buffer 创建失败，本次截图跳过";
			}
			ImageBarrier2(frame.cmdBuffer, swapImage,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}
		else {
			ImageBarrier2(frame.cmdBuffer, swapImage,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		VK_CHECK(vkEndCommandBuffer(frame.cmdBuffer));

		VkSemaphoreSubmitInfo waitSem{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		waitSem.semaphore = frame.imageAvailable;
		waitSem.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signalSem{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signalSem.semaphore = mRenderFinished[mAcquiredImageIdx];
		signalSem.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

		VkCommandBufferSubmitInfo cbSi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cbSi.commandBuffer = frame.cmdBuffer;

		VkSubmitInfo2 si2{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		si2.waitSemaphoreInfoCount = 1; si2.pWaitSemaphoreInfos = &waitSem;
		si2.commandBufferInfoCount = 1; si2.pCommandBufferInfos = &cbSi;
		si2.signalSemaphoreInfoCount = 1; si2.pSignalSemaphoreInfos = &signalSem;

		VK_CHECK(vkQueueSubmit2(mCtx->GraphicsQueue(), 1, &si2, frame.inFlight));

		VkSwapchainKHR sc = mCtx->Swapchain();
		VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		pi.waitSemaphoreCount = 1;
		pi.pWaitSemaphores = &mRenderFinished[mAcquiredImageIdx];
		pi.swapchainCount = 1;
		pi.pSwapchains = &sc;
		pi.pImageIndices = &mAcquiredImageIdx;
		VkResult presentResult = vkQueuePresentKHR(mCtx->GraphicsQueue(), &pi);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
			// swapchain 已 stale：本帧已经送出去，无需视为失败；标记由上层在帧外重建。
			mSwapchainNeedsRebuild = true;
		}
		else if (presentResult != VK_SUCCESS) {
			LOG_ERROR("VulkanRenderer") << "vkQueuePresentKHR failed (" << (int)presentResult << ")";
			mFrameActive = false;
			return false;
		}

		if (capturing) {
			// 等本帧 GPU 完成（fence 不 reset —— BeginFrame 自己管理 wait+reset）
			vkWaitForFences(mCtx->Device(), 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
			if (captureBuf.MappedPtr()) {
				// GPU availability 已由 fence 保证；显式 invalidate 确保 CPU 可见性
				// （对 HOST_COHERENT 内存是 no-op，对 non-coherent 刷新 CPU cache line）。
				captureBuf.InvalidateMapped();
				WriteCapturePng(captureBuf.MappedPtr(), captureExt.width, captureExt.height);
			}
			mCapturePath.clear();
		}

		mFrameActive = false;
		mAcquiredImageIdx = UINT32_MAX;
		mFrameIdx = (mFrameIdx + 1) % FRAMES_IN_FLIGHT;
		return true;
	}

	VkCommandBuffer VulkanRenderer::CurrentCmdBuffer() const {
		if (!mFrameActive) return VK_NULL_HANDLE;
		return mFrames[mFrameIdx].cmdBuffer;
	}

	void VulkanRenderer::WriteCapturePng(const void* bgraPixels, uint32_t w, uint32_t h) {
		// swapchain alpha 不参与合成、值不可靠：强制 255，避免 PNG 透明
		std::vector<uint8_t> pixels(size_t(w) * h * 4);
		std::memcpy(pixels.data(), bgraPixels, pixels.size());
		for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 0xFF;

		// VK_FORMAT_B8G8R8A8_UNORM 内存序 = 小端 ARGB8888
		SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
			pixels.data(), (int)w, (int)h, 32, (int)w * 4, SDL_PIXELFORMAT_ARGB8888);
		if (!surf) {
			LOG_ERROR("VulkanRenderer") << "截图 surface 创建失败: " << SDL_GetError();
			return;
		}
		if (IMG_SavePNG(surf, mCapturePath.c_str()) != 0) {
			LOG_ERROR("VulkanRenderer") << "IMG_SavePNG 失败: " << IMG_GetError();
		}
		else {
			LOG_INFO("VulkanRenderer") << "截图已保存: " << mCapturePath;
		}
		SDL_FreeSurface(surf);
	}
} // namespace pvz
