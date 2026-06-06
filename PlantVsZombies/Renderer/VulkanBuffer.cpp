#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "../Logger.h"

#include <utility>

namespace pvz {
	VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
		if (this != &other) {
			Destroy();
			mCtx = other.mCtx;
			mBuffer = other.mBuffer;
			mAlloc = other.mAlloc;
			mSize = other.mSize;
			mMappedPtr = other.mMappedPtr;
			other.mCtx = nullptr;
			other.mBuffer = VK_NULL_HANDLE;
			other.mAlloc = VK_NULL_HANDLE;
			other.mSize = 0;
			other.mMappedPtr = nullptr;
		}
		return *this;
	}

	bool VulkanBuffer::Create(VulkanContext* ctx,
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		bool hostVisible) {
		mCtx = ctx;
		mSize = size;

		VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size = size;
		bci.usage = usage;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo aci{};
		aci.usage = VMA_MEMORY_USAGE_AUTO;
		if (hostVisible) {
			aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		VmaAllocationInfo info{};
		VkResult r = vmaCreateBuffer(mCtx->Allocator(), &bci, &aci, &mBuffer, &mAlloc, &info);
		if (r != VK_SUCCESS) {
			LOG_ERROR("VulkanBuffer") << "vmaCreateBuffer failed (" << (int)r << ")";
			return false;
		}
		mMappedPtr = info.pMappedData;
		return true;
	}

	void VulkanBuffer::Destroy() {
		if (mBuffer && mCtx && mCtx->Allocator()) {
			vmaDestroyBuffer(mCtx->Allocator(), mBuffer, mAlloc);
		}
		mBuffer = VK_NULL_HANDLE;
		mAlloc = VK_NULL_HANDLE;
		mSize = 0;
		mMappedPtr = nullptr;
		mCtx = nullptr;
	}
} // namespace pvz