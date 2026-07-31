---
name: project-pvz-volk-dynamic-loader
description: 2026-07-31 SDL2 + Volk 动态 Vulkan loader 接入，消除旧 loader 启动期缺少 Vulkan 1.3 导出导致的系统弹窗
metadata:
  node_type: memory
  type: project
  originSessionId: 019fb85c-e2bd-7890-bdf2-6776ffc2f0b1
---

# SDL2 + Volk 动态 Vulkan loader（2026-07-31）

## 背景与结论

Windows 7 玩家启动时在进入 `main()` 前收到“无法定位程序输入点 `vkCmdBeginRendering` 于 `vulkan-1.dll`”系统错误。根因不是 SDL2，也不能据此断定显卡不支持 Vulkan；项目直接链接 `Vulkan::Vulkan`，使 EXE 导入表硬依赖 loader 导出 Vulkan 1.3 核心入口，旧 loader 因缺少导出而被 Windows 装载器提前拒绝。

现已改为 SDL2 与 Volk协作：`SDL_WINDOW_VULKAN` 仍由 SDL2 加载平台 loader，`VulkanContext` 从 `SDL_Vulkan_GetVkGetInstanceProcAddr()` 取得同一 loader 的入口并调用 `volkInitializeCustom()`。创建 instance 后调用 `volkLoadInstance()`，创建唯一 device 后调用 `volkLoadDevice()`；SDL2 继续负责窗口、扩展清单与 `VkSurfaceKHR`，没有被 Volk替代。

## 接口与构建契约

- 所有渲染头统一包含 `<volk.h>`；目标定义 `VK_NO_PROTOTYPES`，禁止 Vulkan头生成可直接链接的函数原型。
- CMake链接 `volk::volk`，不再链接 `Vulkan::Vulkan`。系统 Vulkan SDK仍用于 Vulkan/VMA 头和 `glslc`，其 include 必须标为 `SYSTEM`，否则 clang 会报告数百条第三方 VMA nullability 警告。
- VMA统一使用 `VMA_STATIC_VULKAN_FUNCTIONS=0`、`VMA_DYNAMIC_VULKAN_FUNCTIONS=1`，创建 allocator 时只传 Volk已加载的 `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`。不要用 `vmaImportVulkanFunctionsFromVolk()` 复制整表：本次环境 Vulkan SDK header=350、vcpkg Volk header=341，二级查询入口能避免两套生成表的版本耦合。
- `VulkanContext::Initialize()` 任一步失败都集中走 `Shutdown()`，释放部分创建的 instance/device/surface 并在 SDL卸载 loader 前 `volkFinalize()`。

## 版本边界

本次只改变函数加载方式，**运行时仍请求并硬要求 Vulkan 1.3**；`VK_API_VERSION_1_3`、dynamic rendering、Synchronization2 与 descriptor indexing 检查均未降低。Shader 的 `--target-env=vulkan1.2` 是既有 SPIR-V 目标环境，不代表运行时降为 Vulkan 1.2。

旧 loader 现在会进入程序并记录明确版本错误，不再出现 Windows 的缺入口弹窗；要让只有 Vulkan 1.2 的设备实际进入游戏，仍需后续实现 dynamic rendering / Synchronization2 的 KHR 路径，并另行判断 bindless descriptor indexing 能力。

## 当前验证证据

- `cmake --preset clang-release` + `cmake --build --preset clang-release`：成功，0 warning / 0 error。
- `dumpbin /imports build/clang-release/PlantsVsZombies.exe`：无 `vulkan-1.dll`、`vkCmdBeginRendering`、`vkCmdPipelineBarrier2`、`vkQueueSubmit2` 导入。
- 当前桌面可见运行 `demo_peashooter.json`：窗口已观察，exit 0；`run.log` 结束于 `script finished OK`，三张同步截图均生成且非空，`state.json` 正常。
- `.agents/skills/` 无 Vulkan/Volk相关技能或 reference，本次无需技能更新。
