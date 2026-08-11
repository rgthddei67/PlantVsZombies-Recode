---
name: pvz-opengl33-backend
description: 同一 EXE 的 Vulkan 默认快路与 OpenGL 3.3 Core Win7 兼容回退；含选择、资源、CPU Batch、线程、Pool、验证和真机边界
metadata:
  node_type: memory
  type: project
  updated: 2026-08-11
---

# OpenGL 3.3 Core 兼容渲染后端（2026-08-11）

## 当前契约

- `PlantsVsZombies.exe` 接受 `-Renderer=auto|vulkan|opengl`，缺省为 `auto`。auto 优先 Vulkan；失败后按 Renderer/TexturePool/Context/window 顺序清理，再以不同 SDL flags 创建 OpenGL 窗口。强制 Vulkan 不回退；强制 OpenGL 不调用任何 SDL Vulkan API，适合绕过损坏的 Win7 Vulkan Runtime。
- `RenderTexture`、`TextureBackend` 与 `CaptureBackend` 是资源/截图的后端边界。`ResourceManager::Texture`、`CachedText`、`GlyphAtlas` 不再持有 Vulkan 专属指针或解释 binding ID；Vulkan 仍把 ID 用作 bindless slot，OpenGL 把它用作 texture name。
- Vulkan 快路保持 bindless descriptor、GPU `InstanceRecord`、mapped-buffer worker record/replay。OpenGL 严格为 3.3 Core + GLSL 330：不使用扩展、SSBO、Bindless、instanced draw、persistent mapping、texture array 或动态 sampler 数组。
- OpenGL 普通绘制先由 CPU 应用对象/轨道矩阵，再按原提交序列把连续同纹理、同 shader、同 blend 的三角形合成单 sampler Batch；动态 VBO/IBO 使用 orphan + `glBufferSubData` + `glDrawElements`。clip rect 作为每顶点 framebuffer 像素数据，由 fragment shader 按 top-left 语义与 `gl_FragCoord` 比较。
- OpenGL Reanimation 强制走既有 CPU slow path；`GameObjectManager` 的并行 Draw record 在 GL 后端关闭，所有 GL API 只由 Context 主线程调用。多线程 `Update` 没有改动。`-NoInstance` 在 GL 下被接受并记录为 CPU Batch 路径不变。
- OpenGL 纹理统一上传预乘 RGBA8，支持运行时子区更新、线性/三线性过滤、CLAMP、完整 mip 链、销毁和截图回读。Reanimation 资源在 GL 下按稳定顺序打入带 2px 边缘外扩的图集；Vulkan 仍使用原 bindless 纹理，不为接口统一增加图集成本。
- OpenGL Pool 使用独立 `pool.vert/frag.glsl`，波形、三层颜色与焦散算法参考 `D:\PVZ\OpenGL-PoolEffect-only`，再适配当前 1100×600、对象矩阵、逐顶点 clip 和预乘 Alpha 契约。
- 全屏/窗口切换必须在重算 letterbox 前用 `SDL_GL_GetDrawableSize` 刷新高 DPI framebuffer；截图从 back buffer 回读并翻转 Y；VSync 走 `SDL_GL_SetSwapInterval`。

## 诊断与验证证据

- AutoTest 首帧 `run.log` 记录 requested/selected、`-NoInstance`、测试故障注入、SDL driver；Vulkan 记录 API/dynamic rendering/sync，OpenGL 记录 Vendor/Renderer/Version/GLSL/framebuffer/VSync，并记录进程是否已存在 `vulkan-1.dll`。真实 Vulkan 初始化错误由 `VulkanContext::LastError()` 保留 stage、SDL 原始文本或 `VkResult`。
- 2026-08-11 当前 Windows 桌面（Intel Arc B370，OpenGL 3.3 / GLSL 3.30）可见通过：强制 Vulkan、Vulkan `-NoInstance`、强制 OpenGL、OpenGL `-NoInstance`、auto 选 Vulkan、`-TestVulkanInitFailure` auto 回退 OpenGL；另通过主菜单/全屏回窗/实际关卡/Reanimation/Glyph/粒子/Alpha+Additive/Pool 白天夜间/水线 clip/截图/VSync。强制 GL 的 `run.log` 为 `opengl vulkanLoaderLoaded=no`。
- 相同 Seed 场景的 Vulkan/OpenGL 截图没有对象缺失、层级颠倒、明显颜色/Alpha、文字、粒子或矩阵偏移。普通关卡 GL 取证帧为 148 quads、34 batches/draws、33 次纹理边界、0 次状态边界、VBO 7488 B、IBO 624 B、约 0.68 ms；兼容路径不以追平 Vulkan 性能为目标。
- `clang-release` 与 `clang-release-noavx2` 均构建通过且 Win7 import audit 为 378 imports；两个 preset 的 save-migration/save-schema CTest 均通过。no-AVX2 build rules 无 `/arch:AVX2` 且 `PVZ_ENABLE_AVX2=OFF`。OpenGL 静态扫描无禁用 API/Shader 构造，四个 Shader 均为 `#version 330 core`。

## 尚未证明

- 上述运行证据来自当前 Windows 桌面，不是 Windows 7 真机。Win7 SP1 x64 仍需用 `build\clang-release-noavx2` 测试 `-Renderer=opengl` 和 `-Renderer=auto`，回收 `run.log`、OpenGL Vendor/Renderer/Version、GLSL Version、启动失败文本和截图/录屏。
- auto 已实际尝试 Vulkan 时，Windows loader/ICD 可能仍让 `GetModuleHandle(vulkan-1.dll)` 为 yes；这不等价于 Vulkan 对象或 SDL Vulkan window 未清理。强制 `-Renderer=opengl` 才是“完全不加载/调用 Vulkan”的玩家绕过路径，并已有当前机器 `no` 证据。
