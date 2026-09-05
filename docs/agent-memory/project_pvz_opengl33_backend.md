---
name: pvz-opengl33-backend
description: 同一 EXE 的 Vulkan 默认快路与 OpenGL 3.3 Core Win7 兼容回退；含选择、资源、CPU Batch、线程、Pool、验证和真机边界
metadata:
  node_type: memory
  type: project
  updated: 2026-08-30
---

# OpenGL 3.3 Core 兼容渲染后端（2026-08-11）

## 当前契约

- `PlantsVsZombies.exe` 接受 `-Renderer=auto|vulkan|opengl`，缺省为 `auto`。auto 优先 Vulkan；失败后按 Renderer/TexturePool/Context/window 顺序清理，再以不同 SDL flags 创建 OpenGL 窗口。强制 Vulkan 不回退；强制 OpenGL 不调用任何 SDL Vulkan API，适合绕过损坏的 Win7 Vulkan Runtime。
- `RenderTexture`、`TextureBackend` 与 `CaptureBackend` 是资源/截图的后端边界。`ResourceManager::Texture`、`CachedText`、`GlyphAtlas` 不再持有 Vulkan 专属指针或解释 binding ID；Vulkan 仍把 ID 用作 bindless slot，OpenGL 把它用作 texture name。
- Vulkan 快路保持 bindless descriptor、GPU `InstanceRecord`、mapped-buffer worker record/replay。OpenGL 仍禁用 Bindless、instanced draw、persistent mapping、texture array 和动态 sampler 数组，但 2026-08-30 起先请求 4.3 Core Context：成功则用 GLSL 430 矩阵 SSBO 快路，失败则重建独立纯 3.3 Core Context。`-OpenGL33` 可强制基线路径。
- OpenGL 4.3 快路一次上传原 `BatchVertex` 与当前 batch 的 `mat4[]` SSBO，顶点 shader 按 `matrixIndex` 完成变换；仍在 CPU 按原顺序切分连续同纹理、同 shader、同 blend 段，不重排绘制。纯 3.3 路径保留 CPU 矩阵展开、动态 VBO/IBO 与 `glDrawElements`。两路的 clip rect 都在 fragment shader 按 top-left 语义与 `gl_FragCoord` 比较。
- OpenGL Reanimation 强制走既有 CPU slow path；`GameObjectManager` 的并行 Draw record 在 GL 后端关闭，所有 GL API 只由 Context 主线程调用。多线程 `Update` 没有改动。`-NoInstance` 在 GL 下被接受并记录为 CPU Batch 路径不变。
- OpenGL 纹理统一上传预乘 RGBA8，支持运行时子区更新、线性/三线性过滤、CLAMP、完整 mip 链、销毁和截图回读。Reanimation 资源在 GL 下按稳定顺序打入带 2px 边缘外扩的图集；Vulkan 仍使用原 bindless 纹理，不为接口统一增加图集成本。
- OpenGL Pool 使用独立 `pool.vert/frag.glsl`，波形、三层颜色与焦散算法参考 `D:\PVZ\OpenGL-PoolEffect-only`，再适配当前 1100×600、对象矩阵、逐顶点 clip 和预乘 Alpha 契约。
- 全屏/窗口切换必须在重算 letterbox 前用 `SDL_GL_GetDrawableSize` 刷新高 DPI framebuffer；截图从 back buffer 回读并翻转 Y；VSync 走 `SDL_GL_SetSwapInterval`。
- 2026-08-23 为模仿者增加 `WashedOut` / `LessWashedOut` 两个 GLSL 330 fragment program；CPU 展开和原提交顺序不变，只按 blend/filter 边界切换 program。两条滤镜与 Vulkan 使用相同 HSL 参数，并遵守预乘 alpha 的“还原 RGB→滤色→重新预乘”契约，不能以纯白叠层近似。

## Android 首版补充（2026-09-05）

Android 固定 GLES 3.0 CPU Batch，不执行桌面 4.3/3.3 Core 选择或 Vulkan 初始化。OpenGLApi 使用 GLES 头，基线 shader 转为 GLSL ES 300 并声明 highp；算法与纹理/clip/Pool 约定保持。编译与运行验收必须区分，主人自行验机；入口及已知 Context 丢失限制见 [Android README](../../android/README.md)。

## 诊断与验证证据

- AutoTest 首帧 `run.log` 记录 requested/selected、`-NoInstance`、测试故障注入、SDL driver；Vulkan 记录 API/dynamic rendering/sync，OpenGL 记录 Vendor/Renderer/Version/GLSL/framebuffer/VSync，并记录进程是否已存在 `vulkan-1.dll`。真实 Vulkan 初始化错误由 `VulkanContext::LastError()` 保留 stage、SDL 原始文本或 `VkResult`。
- 2026-08-11 当前 Windows 桌面（Intel Arc B370，OpenGL 3.3 / GLSL 3.30）可见通过：强制 Vulkan、Vulkan `-NoInstance`、强制 OpenGL、OpenGL `-NoInstance`、auto 选 Vulkan、`-TestVulkanInitFailure` auto 回退 OpenGL；另通过主菜单/全屏回窗/实际关卡/Reanimation/Glyph/粒子/Alpha+Additive/Pool 白天夜间/水线 clip/截图/VSync。强制 GL 的 `run.log` 为 `opengl vulkanLoaderLoaded=no`。
- 相同 Seed 场景的 Vulkan/OpenGL 截图没有对象缺失、层级颠倒、明显颜色/Alpha、文字、粒子或矩阵偏移。普通关卡 GL 取证帧为 148 quads、34 batches/draws、33 次纹理边界、0 次状态边界、VBO 7488 B、IBO 624 B、约 0.68 ms；兼容路径不以追平 Vulkan 性能为目标。
- `clang-release` 与 `clang-release-noavx2` 均构建通过且 Win7 import audit 为 378 imports；两个 preset 的 save-migration/save-schema CTest 均通过。no-AVX2 build rules 无 `/arch:AVX2` 且 `PVZ_ENABLE_AVX2=OFF`。原 batch/pool 基线 shader 保持 `#version 330 core`，只有 `batch_ssbo.vert.glsl` 使用 `#version 430 core`。
- 2026-08-30 Intel Arc B370 当前桌面实测：自动创建 4.3/GLSL 430 并命中 `batchPath=ssbo`；`-OpenGL33` 创建 3.3/GLSL 330 并命中 `batchPath=cpu`。同 Seed 主菜单、全屏、关卡 Reanimation+字形和粒子四张 PNG 逐字节 SHA-256 相同，两路均为 149 quads / 35 batches / 35 draws；SSBO 路径矩阵峰值 9536 B。OpenGL clip、Vulkan 默认、Vulkan `-NoInstance` 及 Vulkan 故障注入自动回落 SSBO 均可见通过。小场景单帧约 1 ms，不足以声称稳定 FPS 改善。

## 2026-09-05：CPU Batch 整组上传

主人报告骁龙 835 在满页选卡和迷雾出现时卡顿。旧 CPU 路径按纹理分段后，每段都
orphan 并上传 VBO/IBO；修改为 `UploadCpuBatch` 一次上传完整展开顶点，再由
`SubmitCpuBatchSegment` 按原纹理/混合顺序 DrawArrays。旧索引为连续序列，故该路径
不再生成或上传索引；Pool 专用上传和桌面 SSBO 路径保持原有实现。上传与分段绘制间
不能插入 Pool 或其他 VBO 写入，Graphics::FlushBatch 负责保证连续提交。

新增 `graphics.openGLCpuBatchUploadCount` 状态观测，以及 `smoke_mobile_draw_load`
满页选卡/6-9 迷雾夹具。改前 Windows 强制 CPU 基线 passed，选卡 163 批、迷雾 127 批，
各段分别上传；截图帧的耗时包含捕获开销，不可作为正常帧率结论。改后 Android 与
Windows clang-release 编译通过；主人要求自行测试，因此不执行改后运行回归，上传次数
下降和真机性能改善仍需实测，不能把编译通过视为完成性能验收。

技能/reference 审计：既有动画、资源与后端验证流程仍适用，未改变资产、层次、玩法或
存档合同，无需更新技能；此次依主人要求省略改后可见运行。

## 尚未证明（硬件兼容）

- 上述运行证据来自当前 Windows 桌面，不是 Windows 7 真机。Win7 SP1 x64 仍需用 `build\clang-release-noavx2` 测试 `-Renderer=opengl` 和 `-Renderer=auto`，回收 `run.log`、OpenGL Vendor/Renderer/Version、GLSL Version、启动失败文本和截图/录屏。
- auto 已实际尝试 Vulkan 时，Windows loader/ICD 可能仍让 `GetModuleHandle(vulkan-1.dll)` 为 yes；这不等价于 Vulkan 对象或 SDL Vulkan window 未清理。强制 `-Renderer=opengl` 才是“完全不加载/调用 Vulkan”的玩家绕过路径，并已有当前机器 `no` 证据。
- `-OpenGL33` 只证明当前新驱动上的强制 3.3 路径；仍需 Win7/旧 GPU 真机证明 4.3 Context 请求失败后能重建 3.3 窗口，且无启动闪烁或驱动残留状态。
