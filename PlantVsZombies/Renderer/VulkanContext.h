#pragma once

// VulkanSDK 静态链接路径：函数原型由 <vulkan/vulkan.h> 提供，
// 实现来自 vulkan-1.lib（项目链接器额外依赖里加入）。
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <cstdint>
#include <vector>

struct SDL_Window;

namespace pvz {

// 拥有从 instance 到 swapchain 的全部 Vulkan 设备级对象。
// 不负责具体的帧渲染——那是 VulkanRenderer 的职责。
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool Initialize(SDL_Window* window, bool enableValidation, bool vsync);
    void Shutdown();

    // 热切换 swapchain：销毁旧 swapchain/image views，按新 vsync 选 present mode 重建。
    // 调用者负责在调用前后处理 renderer 端 per-swapchain-image 资源（VulkanRenderer::OnSwapchainRecreated）。
    bool RecreateSwapchain(bool vsync);

    bool IsInitialized() const { return mInitialized; }

    // 给后续 phase 的渲染层用的访问器
    VkInstance       Instance()        const { return mInstance; }
    VkPhysicalDevice PhysicalDevice()  const { return mPhysicalDevice; }
    VkDevice         Device()          const { return mDevice; }
    VkQueue          GraphicsQueue()   const { return mGraphicsQueue; }
    uint32_t         GraphicsQueueFamily() const { return mGraphicsQueueFamily; }
    VkSurfaceKHR     Surface()         const { return mSurface; }
    VmaAllocator     Allocator()       const { return mAllocator; }

    VkSwapchainKHR   Swapchain()       const { return mSwapchain; }
    VkFormat         SwapchainFormat() const { return mSwapchainFormat; }
    VkExtent2D       SwapchainExtent() const { return mSwapchainExtent; }
    const std::vector<VkImage>&      SwapchainImages()     const { return mSwapchainImages; }
    const std::vector<VkImageView>&  SwapchainImageViews() const { return mSwapchainImageViews; }

private:
    bool CreateInstance(SDL_Window* window, bool enableValidation);
    bool CreateDebugMessenger();
    bool CreateSurface(SDL_Window* window);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(SDL_Window* window, bool vsync);
    bool CreateAllocator();

    void DestroySwapchain();

    bool                       mInitialized = false;
    bool                       mValidationEnabled = false;

    SDL_Window*                mWindow          = nullptr;  // Recreate 时复用

    VkInstance                 mInstance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT   mDebugMessenger  = VK_NULL_HANDLE;
    VkSurfaceKHR               mSurface         = VK_NULL_HANDLE;
    VkPhysicalDevice           mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice                   mDevice          = VK_NULL_HANDLE;
    VkQueue                    mGraphicsQueue   = VK_NULL_HANDLE;
    uint32_t                   mGraphicsQueueFamily = UINT32_MAX;

    VkSwapchainKHR             mSwapchain       = VK_NULL_HANDLE;
    VkFormat                   mSwapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D                 mSwapchainExtent { 0, 0 };
    std::vector<VkImage>       mSwapchainImages;
    std::vector<VkImageView>   mSwapchainImageViews;

    VmaAllocator               mAllocator       = VK_NULL_HANDLE;
};

} // namespace pvz
