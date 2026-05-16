#pragma once

#include <volk.h>
#include <utility>
#include <vma/vk_mem_alloc.h>

namespace pvz {

class VulkanContext;

// RAII 封装 VMA 分配的 VkBuffer。后续 phase 会用持久映射的 host-visible 缓冲承载逐帧顶点数据；
// Phase 2b 只用一次性 staging buffer 来上传纹理像素。
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    ~VulkanBuffer() { Destroy(); }

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept { *this = std::move(other); }
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    // hostVisible=true 时强制 SEQUENTIAL_WRITE + MAPPED；mappedPtr 可直接写。
    bool Create(VulkanContext* ctx,
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                bool hostVisible);
    void Destroy();

    VkBuffer        Handle()    const { return mBuffer; }
    VkDeviceSize    Size()      const { return mSize; }
    void*           MappedPtr() const { return mMappedPtr; }

private:
    VulkanContext*  mCtx       = nullptr;
    VkBuffer        mBuffer    = VK_NULL_HANDLE;
    VmaAllocation   mAlloc     = VK_NULL_HANDLE;
    VkDeviceSize    mSize      = 0;
    void*           mMappedPtr = nullptr;
};

} // namespace pvz
