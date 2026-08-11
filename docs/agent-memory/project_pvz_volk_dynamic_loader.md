---
name: project-pvz-volk-dynamic-loader
description: SDL2 + Volk 动态 Vulkan loader，以及 Vulkan 1.3 核心、1.2 KHR 与传统回退的运行时能力矩阵
metadata:
  node_type: memory
  type: project
  originSessionId: 019fb85c-e2bd-7890-bdf2-6776ffc2f0b1
---

# SDL2 + Volk 动态 Vulkan loader（2026-07-31）

## 背景与结论

Windows 7 玩家启动时在进入 `main()` 前收到“无法定位程序输入点 `vkCmdBeginRendering` 于 `vulkan-1.dll`”系统错误。根因不是 SDL2，也不能据此断定显卡不支持 Vulkan；项目直接链接 `Vulkan::Vulkan`，使 EXE 导入表硬依赖 loader 导出 Vulkan 1.3 核心入口，旧 loader 因缺少导出而被 Windows 装载器提前拒绝。

2026-08-11 又补齐了 Vulkan 之外的 Windows 系统 API 装载层；YY-Thunks、subsystem 6.01 与全 EXE 导入门禁的独立契约见 [Windows 7 x64 系统 API 兼容层](project_pvz_win7_yy_thunks.md)。两层必须同时保留：Volk 解决 Vulkan loader 导出差异，YY-Thunks 解决 KERNEL32 等系统 DLL 的新入口。

现已改为 SDL2 与 Volk协作：`SDL_WINDOW_VULKAN` 仍由 SDL2 加载平台 loader，`VulkanContext` 从 `SDL_Vulkan_GetVkGetInstanceProcAddr()` 取得同一 loader 的入口并调用 `volkInitializeCustom()`。创建 instance 后调用 `volkLoadInstance()`，创建唯一 device 后调用 `volkLoadDevice()`；SDL2 继续负责窗口、扩展清单与 `VkSurfaceKHR`，没有被 Volk替代。

## 接口与构建契约

- 所有渲染头统一包含 `<volk.h>`；目标定义 `VK_NO_PROTOTYPES`，禁止 Vulkan头生成可直接链接的函数原型。
- CMake链接 `volk::volk`，不再链接 `Vulkan::Vulkan`。系统 Vulkan SDK仍用于 Vulkan/VMA 头和 `glslc`，其 include 必须标为 `SYSTEM`，否则 clang 会报告数百条第三方 VMA nullability 警告。
- VMA统一使用 `VMA_STATIC_VULKAN_FUNCTIONS=0`、`VMA_DYNAMIC_VULKAN_FUNCTIONS=1`，创建 allocator 时只传 Volk已加载的 `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`。不要用 `vmaImportVulkanFunctionsFromVolk()` 复制整表：本次环境 Vulkan SDK header=350、vcpkg Volk header=341，二级查询入口能避免两套生成表的版本耦合。
- `VulkanContext::Initialize()` 任一步失败都集中走 `Shutdown()`，释放部分创建的 instance/device/surface 并在 SDL卸载 loader 前 `volkFinalize()`。

## 版本边界与能力选路（2026-08-05 更新）

运行时最低版本现为 Vulkan 1.2；instance 请求 `min(loader, 1.3)`，设备使用 instance 与物理设备共同支持的版本。默认 `clang-release` 的 CPU 门槛是 x64 + AVX2；Win7 非法指令回退版 `clang-release-noavx2` 只降低项目源码的指令集，不改变任何 Vulkan 能力要求。设备必须提供 `VK_KHR_swapchain`、项目现用的 Vulkan 1.2 bindless descriptor indexing features，并允许至少 8192 个 update-after-bind combined image samplers；不满足这些条件仍会在初始化阶段给出明确错误。

dynamic rendering 与 synchronization2 独立选路，不能假定驱动同时提供二者：

- Vulkan 1.3 feature 可用时走核心 `vkCmdBeginRendering` / `vkCmdPipelineBarrier2` / `vkQueueSubmit2`。
- Vulkan 1.2 分别探测并启用 `VK_KHR_dynamic_rendering` 与 `VK_KHR_synchronization2`；只有一个扩展时只让对应子系统走 KHR。
- 缺 dynamic rendering 时创建单颜色附件 RenderPass 和每 swapchain image 一个 framebuffer，graphics pipeline 绑定该 RenderPass。
- 缺 synchronization2 时把项目实际使用的 stage/access2 标志保守映射到传统标志，并走 `vkCmdPipelineBarrier` / `vkQueueSubmit`。
- Shader 继续以 `--target-env=vulkan1.2` 编译；bindless descriptor 路径和批处理/实例化 shader 不分叉。

测试开关：`-Vulkan12` 把协商限制到 1.2；`-VulkanLegacyRendering` / `-VulkanLegacySync` 分别屏蔽两项新接口；`-Vulkan12Fallback` 是三者同时启用的最低路径别名。AutoTest 首帧把实际 API 和两条路径写入 `run.log`，状态 JSON 的 `graphics` 也导出相同字段。

## 当前验证证据

- 2026-08-11 Win7 玩家用 `clang-release-noavx2` 已越过系统 API 与非法指令阶段，但 SDL 创建 Vulkan 窗口时，系统 loader 的首次 `vkEnumerateInstanceExtensionProperties(NULL, &count, NULL)` 返回 `VK_ERROR_OUT_OF_HOST_MEMORY (-1)`；程序尚未进入项目的 `VulkanContext`/Volk 能力选路。须先用同机 `vulkaninfo`、实际加载的 `vulkan-1.dll` 路径及驱动/隐式 layer 状态诊断，不能靠 `-Vulkan12Fallback` 绕过。
- `cmake --preset clang-release` + `cmake --build --preset clang-release`：成功；增量复查 `ninja: no work to do`。
- 当前桌面可见、只读加载 30,333,941 字节的 `level18_data.json`（约 20,000 僵尸）运行 `repro_vulkan_level18_pressure.json`；默认 1.3 核心路径与 Vulkan 1.2 的 KHR/KHR、RenderPass/KHR、KHR/传统同步、RenderPass/传统同步共五组均 exit 0、`script finished OK`，每组两张截图存在且非空。
- 五组起始截图逐字节一致；8 秒高压碰撞/死亡/断肢/粒子阶段相对核心基线最多仅 20/660,000 像素不同，单通道最大差 12，肉眼截图一致。压力存档测试前后 SHA-256 均为 `D89B27258F5D821E62841C939AE76E321B45C688F372926BBBD14D10127A08F3`。
- 旧验证仍确认 EXE 无 `vulkan-1.dll` 及 Vulkan 1.3 核心函数静态导入；1.2 loader 不会在进入 `main()` 前因缺导出失败。
- `.agents/skills/` 当前没有 Vulkan/Volk/Graphics 兼容技能或 reference；本次审计未发现需同步的技能契约，因此不更新技能。
