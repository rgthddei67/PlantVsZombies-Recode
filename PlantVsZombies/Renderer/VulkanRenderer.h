#pragma once

#include <volk.h>

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

    // 设置当前帧要用的 pipeline。nullptr 表示只 clear。
    void SetTrianglePipeline(VulkanPipeline* p) { mTrianglePipeline = p; }

    // Phase 2b — 设置一个 bindless 贴图 quad（在三角形之后绘制）。
    // pipeline 必须用 quad shader 创建（带 descriptor set layout 和 push constant）。
    void SetTexturedQuad(VulkanPipeline* pipeline,
                         VkDescriptorSet bindlessSet,
                         float ndcX, float ndcY, float ndcW, float ndcH,
                         uint32_t texIndex) {
        mQuadPipeline = pipeline;
        mQuadDescSet  = bindlessSet;
        mQuadPC       = { ndcX, ndcY, ndcW, ndcH, texIndex };
    }
    void ClearTexturedQuad() { mQuadPipeline = nullptr; mQuadDescSet = VK_NULL_HANDLE; }

    // 渲染一帧。clear color + （如果有 pipeline）画三角形 + （如果有 quad）画贴图 quad。
    bool DrawFrame(float r, float g, float b, float a);

private:
    struct PerFrame {
        VkCommandPool   cmdPool       = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer     = VK_NULL_HANDLE;
        VkSemaphore     imageAvailable= VK_NULL_HANDLE;  // signaled by acquire
        VkFence         inFlight      = VK_NULL_HANDLE;  // CPU↔GPU
    };

    bool CreateFrameResources();
    void DestroyFrameResources();

    struct QuadPushConsts {
        float    x, y, w, h;  // 16 字节
        uint32_t texIndex;    // 4 字节，total 20
    };

    VulkanContext*  mCtx              = nullptr;
    VulkanPipeline* mTrianglePipeline = nullptr;

    VulkanPipeline* mQuadPipeline     = nullptr;
    VkDescriptorSet mQuadDescSet      = VK_NULL_HANDLE;
    QuadPushConsts  mQuadPC{};

    std::array<PerFrame, FRAMES_IN_FLIGHT> mFrames;
    std::vector<VkSemaphore> mRenderFinished;  // 每张 swapchain image 一个（用于 present 时等待）

    uint32_t mFrameIdx = 0;
};

} // namespace pvz
