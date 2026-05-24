#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace pvz {
	class VulkanContext;

	// 一个 bindless 纹理槽位。bindlessIndex 直接对应 fragment shader 中 textures[index] 的下标。
	struct VulkanTexture {
		VkImage         image = VK_NULL_HANDLE;
		VmaAllocation   alloc = VK_NULL_HANDLE;
		VkImageView     view = VK_NULL_HANDLE;
		int             width = 0;
		int             height = 0;
		uint32_t        bindlessIndex = 0;
		// 后续 phase 会补 atlasPage + a{U,V}{0,1}
	};

	// Phase 2b — bindless 纹理系统：
	//   - 单个 descriptor set (set=0)，binding=1 为 sampler2D[4096] 数组
	//   - PARTIALLY_BOUND + UPDATE_AFTER_BIND + VARIABLE_DESCRIPTOR_COUNT
	//   - 共享一个 immutable LINEAR/CLAMP 采样器
	//   - 通过 free-list 管理 bindless 索引
	//   - CreateTextureRGBA8 走 staging buffer → vkCmdCopyBufferToImage → 屏障到 SHADER_READ_ONLY
	class VulkanTexturePool {
	public:
		static constexpr uint32_t MAX_TEXTURES = 4096;

		VulkanTexturePool();
		~VulkanTexturePool();

		VulkanTexturePool(const VulkanTexturePool&) = delete;
		VulkanTexturePool& operator=(const VulkanTexturePool&) = delete;

		bool Initialize(VulkanContext* ctx);
		void Shutdown();

		// 上传 RGBA8 数据创建一张纹理。返回的指针由 pool 拥有；调用 DestroyTexture 释放。
		VulkanTexture* CreateTextureRGBA8(int width, int height, const void* pixels);
		void DestroyTexture(VulkanTexture* tex);

		VkDescriptorSetLayout DescriptorSetLayout() const { return mLayout; }
		VkDescriptorSet       DescriptorSet()       const { return mSet; }

	private:
		bool CreateSampler();
		bool CreateLayout();
		bool CreatePoolAndSet();
		bool CreateUploadCommandPool();

		uint32_t AllocBindlessIndex();
		void     FreeBindlessIndex(uint32_t idx);

		bool UploadPixels(VulkanTexture& tex, const void* pixels, VkDeviceSize byteSize);
		void WriteDescriptor(uint32_t bindlessIndex, VkImageView view);

		VulkanContext* mCtx = nullptr;

		VkSampler             mSampler = VK_NULL_HANDLE;
		VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
		VkDescriptorPool      mPool = VK_NULL_HANDLE;
		VkDescriptorSet       mSet = VK_NULL_HANDLE;
		VkCommandPool         mUploadPool = VK_NULL_HANDLE;
		VkFence               mUploadFence = VK_NULL_HANDLE;

		std::vector<std::unique_ptr<VulkanTexture>> mTextures;  // 由 bindlessIndex 索引
		std::vector<uint32_t>                       mFreeList;
		uint32_t                                    mNextIndex = 0;
	};
} // namespace pvz
