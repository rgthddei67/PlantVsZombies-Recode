---
name: project_pvz_trig_lut_gpu_suggestion_verdict
description: "2026-07-01 否决无测量的GPU trig与粗量化LUT；2026-08-27在2万可见对象压力下改采2048点线性插值LUT，GPU搬运仍否"
metadata:
  node_type: memory
  type: project
  originSessionId: 5d1440f7-e821-4df9-88a8-b37f2be4e916
---

2026-07-01：又一个"读代码猜瓶颈、没跑 profiler"的性能建议——某 AI 提议优化 `Animator` 里的 sin/cos,①搬到 reanim 专用 GPU/compute 着色器 ②sin/cos 查表(LUT)。当时两条都不建议；以下是当时裁决，**LUT 部分已被 2026-08-27 的更高规模实测条件修订，GPU/compute 搬运仍否。**

**2026-08-27 修订：** `level46_data.json` 的 20000 可见僵尸让 Animator 记录再次成为可量热点；当前实现采用 2048 点 `sin/cos` 表并对连续浮点角线性插值，表大小约 16 KiB，理论最大误差约 `1.2e-6`，异常大角度仍用 `fmodf` 归一化。它不是旧建议里会产生可见跳变的粗角度量化；模仿者、冰晶/glow、睡莲阴影以及默认/`-NoInstance` 可见回归均通过。不要把旧的“任何 LUT 都否”继续当当前源码契约；仍须坚持同场 A/B，未来若实体规模或 CPU 架构变化可以重新撤回。

**关键事实(代码里已写死)**:`Animator.cpp:368` GPU-instanced 快路径上方注释记录了过去一次测量(GATE A):trig ≈ **6ms CPU-sum across 165k tracks/frame**(=11000僵尸×~15track;是跨并行worker的CPU时间总和,非墙钟,摊进 par-record 2.37ms 墙钟只占一小块),且明确 "the GPU instancing win comes from removing mat4 construction + 6-vertex inflation + write traffic (7×), **not from moving trig itself**"。sin/cos 只在两处、做同一件事:把 track 的 `kx/ky` 斜切角构造成仿射矩阵 `tA..tD`(`372-377` 快路径压测占~100% / `473-478` 带子动画<1%慢回退),结果打进 `InstanceRecord` 走 SSBO 早已 GPU-bound。

**逐条**:
- **①GPU/compute**:与 [project_pvz_dalao_geometry_compute_suggestion](project_pvz_dalao_geometry_compute_suggestion.md) "矩阵进compute"是同一建议,同判否。GPU 非瓶颈(present 0.14/replay 0.03,整体 CPU-bound);顶点着色器算 trig 是**每顶点**(4~6×/实例)反而更多;compute 预处理则新增 dispatch+compute→vertex 屏障同步点;而 CPU 侧折在本来就要跑的 record pass 里近乎白算,矩阵结果已在 GPU。净收益≈0 还多同步点。
- **②LUT**:`kx/ky` 是逐帧插值+blend 的**连续浮点角**→量化必致斜切抖动(精度);16.5万次/帧**随机角**查表冲刷 L1/L2,而这套渲染对写带宽敏感(instancing 靠减写流量提速),加大表随机读是反方向;现代 sinf/cosf 配 clang -march=native 本就快甚至向量化,拿计算换可能 miss 的访存常更慢。
- **上层**:主人已 STOP([project_pvz_perf_optimization](project_pvz_perf_optimization.md) "够了停在这");7e2ae95 每波刷怪上限压到250,现实到不了165k track;真重启 ROI 序=par-record视口剔除→2d串行地板→collision,均排 trig 前;精算per-frame trig 与 [project_pvz_precomputed_animation](project_pvz_precomputed_animation.md) 已放弃路径同类(插值语义)。

同模式:[project_pvz_gemini_perf_report_verdict](project_pvz_gemini_perf_report_verdict.md) / [project_pvz_gemini_vulkan_review](project_pvz_gemini_vulkan_review.md)。教训重申:瓶颈靠 profiler 非读代码。再提 trig 时先区分“CPU 线性插值高精度 LUT”（当前已采用）与“把 trig 搬 GPU/compute”（仍否）。
