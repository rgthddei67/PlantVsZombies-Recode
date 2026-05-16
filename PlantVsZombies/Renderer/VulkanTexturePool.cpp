#include "VulkanTexturePool.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"

#include <cstdio>
#include <cstring>

namespace pvz {

namespace {

#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult _r = (expr);                                                   \
        if (_r != VK_SUCCESS) {                                                 \
            std::fprintf(stderr, "[VulkanTexturePool] %s failed (VkResult=%d)\n", \
                         #expr, (int)_r);                                       \
            return false;                                                       \
        }                                                                       \
    } while (0)

} // anonymous

VulkanTexturePool::VulkanTexturePool() = default;
VulkanTexturePool::~VulkanTexturePool() { Shutdown(); }

bool VulkanTexturePool::Initialize(VulkanContext* ctx) {
    mCtx = ctx;
    if (!CreateSampler())            return false;
    if (!CreateLayout())             return false;
    if (!CreatePoolAndSet())         return false;
    if (!CreateUploadCommandPool())  return false;

    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VK_CHECK(vkCreateFence(mCtx->Device(), &fci, nullptr, &mUploadFence));

    mTextures.reserve(64);
    return true;
}

bool VulkanTexturePool::CreateSampler() {
    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = VK_LOD_CLAMP_NONE;
    VK_CHECK(vkCreateSampler(mCtx->Device(), &sci, nullptr, &mSampler));
    return true;
}

bool VulkanTexturePool::CreateLayout() {
    // immutable sampler 数组：descriptorCount 个 entry，全部指向同一个采样器
    std::vector<VkSampler> samplers(MAX_TEXTURES, mSampler);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 1;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = MAX_TEXTURES;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = samplers.data();

    VkDescriptorBindingFlags bindingFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bfci{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    bfci.bindingCount  = 1;
    bfci.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    lci.pNext        = &bfci;
    lci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    lci.bindingCount = 1;
    lci.pBindings    = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(mCtx->Device(), &lci, nullptr, &mLayout));
    return true;
}

bool VulkanTexturePool::CreatePoolAndSet() {
    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = MAX_TEXTURES;

    VkDescriptorPoolCreateInfo pci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pci.maxSets       = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &ps;
    VK_CHECK(vkCreateDescriptorPool(mCtx->Device(), &pci, nullptr, &mPool));

    uint32_t variableCount = MAX_TEXTURES;
    VkDescriptorSetVariableDescriptorCountAllocateInfo vdc{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
    vdc.descriptorSetCount = 1;
    vdc.pDescriptorCounts  = &variableCount;

    VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.pNext              = &vdc;
    ai.descriptorPool     = mPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &mLayout;
    VK_CHECK(vkAllocateDescriptorSets(mCtx->Device(), &ai, &mSet));
    return true;
}

bool VulkanTexturePool::CreateUploadCommandPool() {
    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.queueFamilyIndex = mCtx->GraphicsQueueFamily();
    pci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                           VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(mCtx->Device(), &pci, nullptr, &mUploadPool));
    return true;
}

uint32_t VulkanTexturePool::AllocBindlessIndex() {
    if (!mFreeList.empty()) {
        uint32_t i = mFreeList.back();
        mFreeList.pop_back();
        return i;
    }
    return mNextIndex++;
}

void VulkanTexturePool::FreeBindlessIndex(uint32_t idx) {
    mFreeList.push_back(idx);
}

void VulkanTexturePool::WriteDescriptor(uint32_t bindlessIndex, VkImageView view) {
    VkDescriptorImageInfo dii{};
    dii.imageView   = view;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // sampler 由 immutable sampler 提供，这里忽略

    VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    w.dstSet          = mSet;
    w.dstBinding      = 1;
    w.dstArrayElement = bindlessIndex;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &dii;

    vkUpdateDescriptorSets(mCtx->Device(), 1, &w, 0, nullptr);
}

bool VulkanTexturePool::UploadPixels(VulkanTexture& tex, const void* pixels, VkDeviceSize byteSize) {
    // 1) 创建 staging buffer，把像素 memcpy 进去
    VulkanBuffer staging;
    if (!staging.Create(mCtx, byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, /*hostVisible=*/true)) return false;
    std::memcpy(staging.MappedPtr(), pixels, (size_t)byteSize);

    // 2) 分配一次性命令缓冲
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo aci{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    aci.commandPool        = mUploadPool;
    aci.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    aci.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(mCtx->Device(), &aci, &cb));

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cb, &bi));

    // 3) 屏障：UNDEFINED → TRANSFER_DST_OPTIMAL
    {
        VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        b.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        b.dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.image         = tex.image;
        b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers    = &b;
        vkCmdPipelineBarrier2(cb, &dep);
    }

    // 4) Copy
    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent      = { (uint32_t)tex.width, (uint32_t)tex.height, 1 };
    vkCmdCopyBufferToImage(cb, staging.Handle(), tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // 5) 屏障：TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
    {
        VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        b.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        b.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.image         = tex.image;
        b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers    = &b;
        vkCmdPipelineBarrier2(cb, &dep);
    }

    VK_CHECK(vkEndCommandBuffer(cb));

    // 6) 同步提交（一次性、阻塞）
    VkCommandBufferSubmitInfo cbSi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cbSi.commandBuffer = cb;
    VkSubmitInfo2 si{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos    = &cbSi;

    vkResetFences(mCtx->Device(), 1, &mUploadFence);
    VK_CHECK(vkQueueSubmit2(mCtx->GraphicsQueue(), 1, &si, mUploadFence));
    vkWaitForFences(mCtx->Device(), 1, &mUploadFence, VK_TRUE, UINT64_MAX);

    vkFreeCommandBuffers(mCtx->Device(), mUploadPool, 1, &cb);
    // staging 在作用域结束时自动销毁
    return true;
}

VulkanTexture* VulkanTexturePool::CreateTextureRGBA8(int width, int height, const void* pixels) {
    if (!mCtx || width <= 0 || height <= 0 || !pixels) return nullptr;

    const uint32_t idx = AllocBindlessIndex();
    if (idx >= MAX_TEXTURES) {
        std::fprintf(stderr, "[VulkanTexturePool] bindless slot exhausted (max=%u)\n", MAX_TEXTURES);
        FreeBindlessIndex(idx);
        return nullptr;
    }

    auto t = std::make_unique<VulkanTexture>();
    t->width  = width;
    t->height = height;
    t->bindlessIndex = idx;

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = { (uint32_t)width, (uint32_t)height, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    VkResult r = vmaCreateImage(mCtx->Allocator(), &ici, &aci, &t->image, &t->alloc, nullptr);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanTexturePool] vmaCreateImage failed (%d)\n", (int)r);
        FreeBindlessIndex(idx);
        return nullptr;
    }

    VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image            = t->image;
    vci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vci.format           = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    r = vkCreateImageView(mCtx->Device(), &vci, nullptr, &t->view);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanTexturePool] vkCreateImageView failed (%d)\n", (int)r);
        vmaDestroyImage(mCtx->Allocator(), t->image, t->alloc);
        FreeBindlessIndex(idx);
        return nullptr;
    }

    const VkDeviceSize byteSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;
    if (!UploadPixels(*t, pixels, byteSize)) {
        vkDestroyImageView(mCtx->Device(), t->view, nullptr);
        vmaDestroyImage(mCtx->Allocator(), t->image, t->alloc);
        FreeBindlessIndex(idx);
        return nullptr;
    }

    WriteDescriptor(idx, t->view);

    // 把指针存进槽位（按 bindlessIndex 索引）
    if (mTextures.size() <= idx) mTextures.resize(idx + 1);
    VulkanTexture* raw = t.get();
    mTextures[idx] = std::move(t);
    return raw;
}

void VulkanTexturePool::DestroyTexture(VulkanTexture* tex) {
    if (!tex || !mCtx) return;
    const uint32_t idx = tex->bindlessIndex;
    if (idx >= mTextures.size() || mTextures[idx].get() != tex) return;

    // Phase 2b 简化处理：同步等设备空闲后销毁。后续 phase 走 deletion queue。
    vkDeviceWaitIdle(mCtx->Device());

    if (tex->view)  vkDestroyImageView(mCtx->Device(), tex->view, nullptr);
    if (tex->image) vmaDestroyImage(mCtx->Allocator(), tex->image, tex->alloc);

    mTextures[idx].reset();
    FreeBindlessIndex(idx);
}

void VulkanTexturePool::Shutdown() {
    if (!mCtx) return;
    VkDevice dev = mCtx->Device();
    if (dev) vkDeviceWaitIdle(dev);

    for (auto& up : mTextures) {
        if (!up) continue;
        if (up->view)  vkDestroyImageView(dev, up->view, nullptr);
        if (up->image) vmaDestroyImage(mCtx->Allocator(), up->image, up->alloc);
    }
    mTextures.clear();
    mFreeList.clear();
    mNextIndex = 0;

    if (mUploadFence) { vkDestroyFence(dev, mUploadFence, nullptr); mUploadFence = VK_NULL_HANDLE; }
    if (mUploadPool)  { vkDestroyCommandPool(dev, mUploadPool, nullptr); mUploadPool = VK_NULL_HANDLE; }
    if (mPool)        { vkDestroyDescriptorPool(dev, mPool, nullptr);    mPool = VK_NULL_HANDLE; mSet = VK_NULL_HANDLE; }
    if (mLayout)      { vkDestroyDescriptorSetLayout(dev, mLayout, nullptr); mLayout = VK_NULL_HANDLE; }
    if (mSampler)     { vkDestroySampler(dev, mSampler, nullptr); mSampler = VK_NULL_HANDLE; }
    mCtx = nullptr;
}

} // namespace pvz
