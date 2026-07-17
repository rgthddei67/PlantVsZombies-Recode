---
name: feedback-dual-queue-order-preservation
description: 在 PvZ Graphics 这类 dual-queue 渲染设计里加新路径前，必须显式审查 serial fallback 的跨队列保序——worker 模式可能有 replay 兜底，但 serial 没有。
metadata:
  node_type: memory
  type: feedback
  originSessionId: 56364cb4-cdfa-4e81-95e6-98462baabf7e
---

# Dual-queue 渲染设计的保序 foot-gun

**规则**：在 PvZ Graphics（或任何类似的「多队列累积 + 周期性 flush」渲染系统）里加新的 draw 队列时，**显式审查 serial fallback 路径的跨队列保序**，不能依赖 "worker replay 路径正确所以 serial 也对"。

**Why**：commit `388a845`（GPU instancing for reanim）加了第二个累积队列 `m_batchInstances`（与原 `m_batchVertices` 并列）。worker 模式下 `ReplayAndEndParallel::emitUpTo` 按 SetBlend 段交错 emit 两个队列，保序正确；serial fallback 路径**没有等价机制**——两个队列独立 mid-frame flush，cmd buffer 顺序 ≠ 应用 append 顺序。ship 后 2 天 user 报严重渲染 bug（GlowingTimer>0 时 plant body 全消失、Z-order 倒挂），根因就是 serial fallback 漏审。修复用了 cross-flush（`AddVertices` 前 flush instances；`AppendReanimInstance` 前 flush batch）。详见 [pvz-gpu-instancing-reanim](project_pvz_gpu_instancing_reanim.md) postscript。

**How to apply**：
- 看到 Graphics.cpp 里 mid-frame 触发的 `FlushXxx` 调用（如 blend 切换、capacity 满）—— 立警觉：**这个 flush 时机和"另一队列在它之前 append 的内容"** 的相对顺序对吗？
- 看到 worker 路径有 `emitUpTo` / `RecordCmd` / per-slot 交错 replay 但 serial 路径直接 push to global queue —— 这就是不对称结构，serial 必须 cross-flush 才能补齐。
- 设计新「累积+flush」队列前问自己：「这个队列的 flush 时机和 m_batchVertices 的 flush 时机会不会错位？」如果错位会有 Z-order bug —— 加 cross-flush 在双方的入口点。

**相关变量重载 lesson**：同一 fix 还揭示 `m_currentBlendMode` 被过载承担两个不变量（实例队列状态 + per-vert bm 默认值）。**single variable 一个不变量**原则在 Graphics 这种状态共享重的子系统里特别重要——加新功能时如果想"复用"已有 state 变量，先确认新 use 与旧 use **不会在时间上同时活跃**。如果会，分离为两个变量（如 `m_queuedInstanceBlend`）。
