---
name: reference-pvz-batch-instance-zorder
description: PvZ 渲染 sprite=instance缓冲 / 文字=batch缓冲两套独立队列，cross-flush 把 batch 压到 instance 下；叠加层用 DrawTextOnTop
metadata:
  node_type: memory
  type: reference
  originSessionId: d82d330e-766a-4111-b72d-0887c31152b1
---

PvZ 渲染有**两套独立 GPU 队列**：reanim sprite 走 instance 缓冲（`AppendReanimInstance`），
文字/FillRect 等走 batch 顶点缓冲（`m_batchVertices`/`FlushBatch`）。

`AppendReanimInstance`（`Graphics.cpp` 约 943-996）的 cross-flush **有意**让 batch 沉到后续
reanim instance **之下**（注释："否则 plant body 被 lawn 反盖不可见的 Z-order bug"）。

后果：在对象 `Draw()` 循环内部直接 `DrawText`，文字会被压到连自己 sprite 都盖不住的底层 → 隐身。
（并行路径 >200 对象走 deferred-text，在所有几何后统一画，反而正常；串行 <200 才暴露。）

两个文字原语（2026-05 最终设计，DeferredTextCmd 加 `onTop` 标志区分）：
- **`Graphics::DrawText`** = **当前层**（与对象同 z-order）。用于血量显示。
  - 串行：进 DrawText 前先 `FlushInstances()`（对象循环外 UI 调用时队列空=no-op，安全），
    文字落在已画对象之上、后续对象之下。
  - 并行：worker 记 `onTop=false` 的 DeferredText；`ReplayAndEndParallel` 在该 cmd 记录的
    几何位置**就地交错渲染**（emitUpTo 已 emit 本对象 sprite，故文字夹在本对象与后续对象之间）。
    内联 FlushBatch 后须 `boundPipe=None` 强制下次 emit 重绑管线。
- **`Graphics::DrawTextOnTop`** = **绝对顶层**（HUD/调试 overlay，盖住一切游戏对象）。
  - 并行：worker 记 `onTop=true`，回放在所有 slot 几何之后统一渲染。
  - 串行：flush 已排队几何后立即画（无回放阶段，近似顶层）。
- 旧坑：普通 batch 文字若不做上述处理，会被 cross-flush 压到 instance 下层 → 沉底隐身。
- 关键实现点：内联渲染要求把 `fr.vboCursor/ssboCursor` 推到 post-parallel 预留区的动作
  **提前到 slot 循环之前**（原在循环后），否则内联文字顶点与 slot 切片撞车。

颜色范围见 [reference-pvz-color-0-255-convention](reference_pvz_color_0_255_convention.md)。
