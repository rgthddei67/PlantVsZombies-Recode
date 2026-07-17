---
name: project_pvz_dalao_geometry_compute_suggestion
description: "2026-06-17 一位没看过代码的大佬建议\"矩阵进compute shader + 几何着色器1顶点生成6个\"，逐条核实后的裁决"
metadata:
  node_type: memory
  type: project
  originSessionId: 0e561c14-6599-4579-a3ce-fcba6263e252
---

2026-06-17 一位**没看过代码**的大佬给主人提了两条渲染优化建议，主人让我对照实际代码裁决。结论：他点中了正确目标，但给的两个具体手段一个已被更优做法替代、一个会回退。同类先例见 [project_pvz_gemini_vulkan_review](project_pvz_gemini_vulkan_review.md)（外部提建议→逐条验证）。

**① "矩阵运算放进 compute shader" —— 没做，且不建议做。**
- 现状：2×3 仿射在 **CPU** 算（`Reanimation/Animator.cpp:390-395` DrawInternalInstanced，trig+scale+w*Scale 烘进列），但已是**多线程并行**写进 host-mapped SSBO（见下方并行机制），不在串行关键路径上。
- 不值得搬：搬 compute 要先把每 sprite 源数据上传 GPU + 一次 dispatch + compute→vertex 同步屏障；而 reanim **骨骼层级解算是树状串行**、与游戏逻辑在 CPU 交织，不合 compute 的"百万互不相关元素"模型。perf 调查早已 STOP（见 [project_pvz_perf_optimization](project_pvz_perf_optimization.md)）。

**② "几何着色器一个顶点生成 6 个" —— 目标早达成，但用的不是几何着色器，且 GS 会回退。**
- 现状：`vkCmdDraw(cb, 6, instanceCount, 0, baseInst)`（`Graphics.cpp:810`），**不绑顶点缓冲**；6 顶点靠顶点着色器里 `switch(gl_VertexIndex)` 0..5 展开 quad（`Shader/reanim_inst.vert.glsl:46`），每 sprite 只存一份 48B `InstanceRecord`（SSBO set=0 binding=0）。这叫 **vertex pulling / programmatic vertex fetch**，是 GS"1→6"目标的现代标准答案。
- 为什么不能换 GS：桌面 GPU 上几何着色器为支持可变输出必须在驱动层做输出缓冲与原语序列化，NVIDIA/AMD 都建议别用 GS 做 quad 展开——通常**更慢**。本仓库 Phase 6 OpenGL 清理时还**主动删过**一个 geom-batch 死子系统（见 [project_pvz_phase6_opengl_cleanup](project_pvz_phase6_opengl_cleanup.md)）。

**一句话回大佬**：方向认同（别存 6 份顶点，让 GPU 现场生成），但我已用 `gl_VertexIndex` 顶点拉取+实例化实现，不是 GS（GS 在桌面会拖慢）；矩阵在 CPU 但已并行写 SSBO，搬 compute 要新增同步点且骨骼是串行树，先 profile 再说。

底层实例化管线本身见 [project_pvz_gpu_instancing_reanim](project_pvz_gpu_instancing_reanim.md)；并行填 SSBO 的机制（BeginParallelRecord 把 host-mapped instBuf 切成各 worker 不重叠切片→`sl.instPtr[instCount++]=rec` 无锁写→ReplayAndEndParallel 按 slot 顺序拼成有序 vkCmdDraw）散落在 `Graphics.cpp` BeginParallelRecord/AppendReanimInstance/ReplayAndEndParallel + `GameObjectManager.cpp:246-272` dispatch，主人明确表示这部分不入记忆（代码里有）。
