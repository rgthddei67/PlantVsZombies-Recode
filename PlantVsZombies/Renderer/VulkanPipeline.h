#pragma once

#include <volk.h>

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
        // 可选：descriptor set layout（nullptr = 不绑定 set）
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        // 可选：push constant 字节数（0 = 无）。范围始终从 offset 0 开始。
        uint32_t              pushConstantSize    = 0;
        VkShaderStageFlags    pushConstantStages  = 0;
        // 是否启用标准 alpha 混合（SRC_ALPHA / ONE_MINUS_SRC_ALPHA）
        bool                  alphaBlend          = false;
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
