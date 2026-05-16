#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace pvz {

class VulkanContext;

// 通用 pipeline 工厂。Phase 2a 用于无 desc / 无 push const 的测试三角形；
// Phase 2b 起用于带 bindless descriptor 和 push constant 的真实 draw。
class VulkanPipeline {
public:
    struct Desc {
        const char* vertSpvPath = nullptr;
        const char* fragSpvPath = nullptr;
        VkFormat    colorFormat = VK_FORMAT_UNDEFINED;
        // 单 set（保留给老的 quad/triangle 烟雾测试）。若 setLayoutCount > 0，
        // 则使用 setLayouts/setLayoutCount 取代之。
        VkDescriptorSetLayout        descriptorSetLayout = VK_NULL_HANDLE;
        const VkDescriptorSetLayout* setLayouts          = nullptr;
        uint32_t                     setLayoutCount      = 0;
        // 可选：push constant 字节数（0 = 无）。范围始终从 offset 0 开始。
        uint32_t              pushConstantSize    = 0;
        VkShaderStageFlags    pushConstantStages  = 0;
        // 顶点输入（Phase 3b：batch pipeline 用 BatchVertex 描述）。
        const VkVertexInputBindingDescription*   vertexBindings       = nullptr;
        uint32_t                                 vertexBindingCount   = 0;
        const VkVertexInputAttributeDescription* vertexAttributes     = nullptr;
        uint32_t                                 vertexAttributeCount = 0;
        // 混合模式选项（同一时刻只生效一个）：
        //   alphaBlend    — SRC_ALPHA / ONE_MINUS_SRC_ALPHA
        //   additiveBlend — SRC_ALPHA / ONE
        bool                  alphaBlend          = false;
        bool                  additiveBlend       = false;
    };

    VulkanPipeline();
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    bool Initialize(VulkanContext* ctx, const Desc& desc);
    void Shutdown();

    VkPipeline       Handle() const { return mPipeline; }
    VkPipelineLayout Layout() const { return mLayout; }

private:
    static bool LoadFile(const char* path, std::vector<char>& out);
    bool LoadShaderModule(const char* path, VkShaderModule* outModule);

    VulkanContext*   mCtx      = nullptr;
    VkPipelineLayout mLayout   = VK_NULL_HANDLE;
    VkPipeline       mPipeline = VK_NULL_HANDLE;
};

} // namespace pvz
