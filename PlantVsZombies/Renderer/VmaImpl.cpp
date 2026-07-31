// VMA (Vulkan Memory Allocator) 的实现单元。
// 整个项目里只能有一个 .cpp 定义 VMA_IMPLEMENTATION，否则会出现重复符号。
//
// 项目通过 Volk 动态取得 Vulkan 入口；VMA_STATIC_VULKAN_FUNCTIONS=0 禁止重新引入
// vulkan-1.dll 导入，VMA_DYNAMIC_VULKAN_FUNCTIONS=1 则让 allocator 经传入的
// vkGetInstanceProcAddr / vkGetDeviceProcAddr 自行补齐所需函数。

#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
