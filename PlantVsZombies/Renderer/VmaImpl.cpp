// VMA (Vulkan Memory Allocator) 的实现单元。
// 整个项目里只能有一个 .cpp 定义 VMA_IMPLEMENTATION，否则会出现重复符号。

#include <volk.h>

// VMA 不再静态调用 vkXxx 原型，而是通过运行时函数指针表
// （在 Phase 1 创建 VmaAllocator 时由 volk 注入）。
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>