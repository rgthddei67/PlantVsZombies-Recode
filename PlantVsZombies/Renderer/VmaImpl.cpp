// VMA (Vulkan Memory Allocator) 的实现单元。
// 整个项目里只能有一个 .cpp 定义 VMA_IMPLEMENTATION，否则会出现重复符号。
//
// VulkanSDK 静态链接：VMA 的默认行为就是直接调用 <vulkan/vulkan.h> 的原型，
// 因此不再需要 VMA_STATIC_VULKAN_FUNCTIONS / VMA_DYNAMIC_VULKAN_FUNCTIONS 的开关。

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>