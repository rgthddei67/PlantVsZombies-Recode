#pragma once

#include <vulkan/vulkan.h>
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

		// hostVisible=true 时 + MAPPED 持久映射：
		//   hostReadback=false（默认）→ SEQUENTIAL_WRITE，适合 CPU→GPU staging/顶点上传；
		//   hostReadback=true         → HOST_ACCESS_RANDOM，适合 GPU→CPU 回读（截图等）；
		//   两种路径均通过 MappedPtr() 访问映射内存；回读路径在读取前须调用 InvalidateMapped()。
		bool Create(VulkanContext* ctx,
			VkDeviceSize size,
			VkBufferUsageFlags usage,
			bool hostVisible,
			bool hostReadback = false);
		void Destroy();

		// GPU 写完、CPU 读前调用；对 HOST_COHERENT 内存是 no-op，永远安全。
		void InvalidateMapped();

		VkBuffer        Handle()    const { return mBuffer; }
		VkDeviceSize    Size()      const { return mSize; }
		void* MappedPtr() const { return mMappedPtr; }

	private:
		VulkanContext* mCtx = nullptr;
		VkBuffer        mBuffer = VK_NULL_HANDLE;
		VmaAllocation   mAlloc = VK_NULL_HANDLE;
		VkDeviceSize    mSize = 0;
		void* mMappedPtr = nullptr;
	};
} // namespace pvz
