#include "VulkanPipeline.h"
#include "VulkanContext.h"

#include <cstdio>
#include <fstream>

namespace pvz {

namespace {

#define VK_CHECK(expr)                                                       \
    do {                                                                     \
        VkResult _r = (expr);                                                \
        if (_r != VK_SUCCESS) {                                              \
            std::fprintf(stderr, "[VulkanPipeline] %s failed (VkResult=%d)\n", \
                         #expr, (int)_r);                                    \
            return false;                                                    \
        }                                                                    \
    } while (0)

} // anonymous

VulkanPipeline::VulkanPipeline() = default;
VulkanPipeline::~VulkanPipeline() { Shutdown(); }

bool VulkanPipeline::LoadFile(const char* path, std::vector<char>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "[VulkanPipeline] Failed to open shader file: %s\n", path);
        return false;
    }
    const std::streamsize size = f.tellg();
    if (size <= 0 || (size % 4) != 0) {
        std::fprintf(stderr, "[VulkanPipeline] Bad SPIR-V file size (%lld) for %s\n",
                     (long long)size, path);
        return false;
    }
    out.resize((size_t)size);
    f.seekg(0);
    f.read(out.data(), size);
    return true;
}

bool VulkanPipeline::LoadShaderModule(const char* path, VkShaderModule* outModule) {
    std::vector<char> bytes;
    if (!LoadFile(path, bytes)) return false;

    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = bytes.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(bytes.data());
    VK_CHECK(vkCreateShaderModule(mCtx->Device(), &ci, nullptr, outModule));
    return true;
}

bool VulkanPipeline::Initialize(VulkanContext* ctx, const Desc& desc) {
    mCtx = ctx;
    VkDevice dev = mCtx->Device();

    VkShaderModule vert = VK_NULL_HANDLE;
    VkShaderModule frag = VK_NULL_HANDLE;
    if (!LoadShaderModule(desc.vertSpvPath, &vert)) return false;
    if (!LoadShaderModule(desc.fragSpvPath, &frag)) {
        vkDestroyShaderModule(dev, vert, nullptr);
        return false;
    }

    // pipeline layout：单 set 或多 set（后者优先）+ 可选 push constant
    VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    if (desc.setLayoutCount > 0 && desc.setLayouts) {
        plci.setLayoutCount = desc.setLayoutCount;
        plci.pSetLayouts    = desc.setLayouts;
    } else if (desc.descriptorSetLayout != VK_NULL_HANDLE) {
        plci.setLayoutCount = 1;
        plci.pSetLayouts    = &desc.descriptorSetLayout;
    }
    VkPushConstantRange pcr{};
    if (desc.pushConstantSize > 0) {
        pcr.stageFlags = desc.pushConstantStages;
        pcr.offset     = 0;
        pcr.size       = desc.pushConstantSize;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges    = &pcr;
    }
    VK_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &mLayout));

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vis{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    if (desc.vertexBindingCount > 0 && desc.vertexBindings) {
        vis.vertexBindingDescriptionCount = desc.vertexBindingCount;
        vis.pVertexBindingDescriptions    = desc.vertexBindings;
    }
    if (desc.vertexAttributeCount > 0 && desc.vertexAttributes) {
        vis.vertexAttributeDescriptionCount = desc.vertexAttributeCount;
        vis.pVertexAttributeDescriptions    = desc.vertexAttributes;
    }

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (desc.alphaBlend) {
        blendAtt.blendEnable         = VK_TRUE;
        blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAtt.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
    } else if (desc.additiveBlend) {
        blendAtt.blendEnable         = VK_TRUE;
        blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAtt.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments    = &blendAtt;

    const VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    ds.dynamicStateCount = 2;
    ds.pDynamicStates    = dyn;

    VkFormat colorFmt = desc.colorFormat;
    VkPipelineRenderingCreateInfo prci{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    prci.colorAttachmentCount    = 1;
    prci.pColorAttachmentFormats = &colorFmt;

    VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gpci.pNext               = &prci;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;
    gpci.pVertexInputState   = &vis;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState      = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState   = &ms;
    gpci.pColorBlendState    = &cb;
    gpci.pDynamicState       = &ds;
    gpci.layout              = mLayout;

    VkResult r = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, nullptr, &mPipeline);

    vkDestroyShaderModule(dev, vert, nullptr);
    vkDestroyShaderModule(dev, frag, nullptr);

    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanPipeline] vkCreateGraphicsPipelines failed (%d)\n", (int)r);
        return false;
    }
    return true;
}

void VulkanPipeline::Shutdown() {
    if (!mCtx) return;
    VkDevice dev = mCtx->Device();
    if (!dev) { mCtx = nullptr; return; }

    if (mPipeline) { vkDestroyPipeline(dev, mPipeline, nullptr);     mPipeline = VK_NULL_HANDLE; }
    if (mLayout)   { vkDestroyPipelineLayout(dev, mLayout, nullptr); mLayout = VK_NULL_HANDLE; }
    mCtx = nullptr;
}

} // namespace pvz
