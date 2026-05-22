# 设计：并行 Update —— 阶段一（仅 animator 推进）

日期：2026-05-17
状态：已批准（brainstorming），待写实现 plan
关联：[[pvz-perf-optimization]] 内存；前置并行 Draw record/replay（commit `629f973`）

## 1. 问题与根因（已实测确认）

11000+ 僵尸时 GPU ~20%、单核 ~100%，典型串行 CPU-bound。
经分阶段埋点测量，根因锁定：

- `GameObjectManager::Update()` 的逐对象循环（`GameObjectManager.cpp` 现有
  `2b.GOM_objUpdateLoop` 埋点段）= **~6.03 ms，单核串行，占 ~14 ms 帧的 ~43%**
  （build 1 干净测量，粗粒度埋点开销可忽略 —— 可信基线）。
- 该 6 ms 内部 ≈ **40% animator 推进 / 60% 组件循环 + AI**（细粒度探针绝对值被
  `steady_clock::now()` ×4.6 万/帧污染，但两次独立 build 的比值稳定 ≈ 42:58，且
  等量探针偏置只会把比值压向 50:50 —— 故组件侧实际占比只多不少）。
- `Animator::Update()` 内联触发用户帧事件回调（`Animator.cpp:173/188/200`），
  僵尸里这些回调会播音效/生成对象/掉头掉臂/改 board —— 共享状态危害穿在最热路径上。

结论：没有"直接 parallel_for"的便宜解；纯动画 LOD 只能覆盖较小的 ~40% 那半，
不够。需结构性并行化。用户已选"分阶段并行"，本 spec 只覆盖**阶段一**。

非目标（后续独立 plan）：组件循环并行化、Zombie AI/move 并行化、动画 LOD、
Collision/Clickable 串行段优化。

## 2. 阶段一范围（窄）

只把 **Animator 帧推进** 移到并行：

- 并行（worker 线程）：Animator 帧索引推进 / 裁剪 / 末尾循环 / `PLAY_ONCE_TO`
  轨道切换 / 混合计时 + 递归子 reanim 的同样推进。
- 串行（主线程，`mGameObjects` 顺序）：帧事件回调触发、组件循环、Zombie
  AI/move、auto-destroy、UpdateGlowingEffect —— 与现状顺序完全一致。

预计粗略收益 ~1.5–2 ms（最终以 GATE 实测为准）。目标同时一次性搭好可复用的
"并行推进 + 串行提交"脚手架供后续阶段直接复用。

## 3. 关键代码事实（已核实）

- `PLAY_ONCE_TO` 在 `Update()` 内调用的 `PlayTrack`（`Animator.cpp:149`）是
  **对象本地**的：`GetTrackRange`/`SetFrameRange` 只读不可变的共享 `mReanim`
  数据、只写 `this`。故**不需要延迟轨道切换**；唯一需延迟的是用户帧事件回调。
- 子 reanim 由各 Animator 自己持有（`mExtraInfos` 内 weak_ptr），不跨游戏对象
  共享 —— 对子节点递归套用同样拆分即安全。
- `Animator::AdvanceFrame` 不触碰 `mFrameEvents`（erase 只在 `FireFrameEvents`
  串行段发生）—— 并行段无并发 multimap 改动。
- `DeltaTime::GetDeltaTime()` 每帧在 `GameApp` 主循环 `Scene::Update` 前设置一次，
  update 期间只读 —— 并发读安全。
- 复用 `GameObjectManager::mThreadPool` 的现有 `Dispatch(total, [](start,end))`
  按下标等分 `[start,end)`（与并行 Draw 同一原语）—— 不改 ThreadPool。

## 4. 设计

### 4.1 两阶段 API（最小、可复用）

- `GameObject::UpdateParallel()` —— 新虚函数，默认 `{}`（空）。契约：仅做对象
  本地工作，worker 线程安全。
- `AnimatedObject::UpdateParallel()` —— 覆写：
  `if (mAnimator) mAnimator->AdvanceFrame(); mAdvancedInParallel = true;`，无其它。
- `AnimatedObject` 新增 `bool mAdvancedInParallel = false;`（每对象一个 bool）。
- `AnimatedObject::Update()` —— 结构不变，仅把原 `mAnimator->Update();` 一行改为：
  ```cpp
  if (mAdvancedInParallel) { mAnimator->FireFrameEvents(); mAdvancedInParallel = false; }
  else                     { mAnimator->Update(); }   // 串行 fallback：与现状逐字节一致
  ```

### 4.2 GameObjectManager::Update 控制流

在逐对象循环段（`2b`），镜像现有并行 Draw 的阈值/fallback 模式：

```
if (parallelEnabled && total >= kParallelUpdateThreshold):
    mThreadPool->Dispatch(total, [start,end]:                  // 阶段A — 并行
        for i in [start,end): if active: mGameObjects[i]->UpdateParallel())
    for i in 0..total: if active: mGameObjects[i]->Update()     // 阶段B — 串行，mGameObjects 序
else:
    for i in 0..total: if active: mGameObjects[i]->Update()     // 不变的串行 fallback
```

阶段 B 即今天的串行循环，仅 animator 的"推进"被提到阶段 A；组件/AI/move/回调/
auto-destroy 仍在阶段 B 按相同顺序串行。`kParallelUpdateThreshold` 取值与并行
Draw 阈值同量级（实现 plan 内定，GATE 可调）。

**串行 fallback 可证明与现状行为逐字节一致**（`mAdvancedInParallel` 恒 false →
`mAnimator->Update()` 与今天完全相同）。

### 4.3 Animator 拆分

在现有缝（`Animator.cpp` 第 163 行，clamp 之后、事件触发之前）拆：

- `AdvanceFrame()` = 现 124–162 行（推进、末尾/循环、`PLAY_ONCE_TO`、clamp）
  + 混合计时 210–214 + 递归子节点 `AdvanceFrame()`。把事件窗口记进新 POD 成员
  `int mEvtOldInt; int mEvtNewInt; bool mEvtWrapped; bool mEvtPending;`
  （`oldInt` 在推进前取、`newInt` 在 clamp 后取 —— 与今天 128/166 行同点）。
- `FireFrameEvents()` = 现 164–208 行，由记录的窗口驱动 + 递归子节点
  `FireFrameEvents()`。唯一读/erase `mFrameEvents` 之处。
- `Animator::Update()` 变为 `AdvanceFrame(); FireFrameEvents();` —— 同样的
  `(oldInt,newInt,wrapped)` 产生同样输出。今天事件逻辑里的任何潜在怪癖被**原样
  复现**，不修（单变量改动）。

### 4.4 并发安全论证（阶段 A）

- 只读：`DeltaTime::GetDeltaTime()`（每帧前置设置、期间只读）、`mReanim`
  不可变轨道数据经 `GetTrackRange`（加载后不变）。
- 只写 `this` + 自己的子节点：所有 `mFrameIndex*`、`mPlayingState`、
  `mIsPlaying`、`mReanimBlendCounter`、`mSpeed`、`mCurrentTrackName`、
  `mTargetTrack`、新 `mEvt*`。子节点各 Animator 独占，不跨对象 → 无两个 worker
  触同一 Animator。
- 阶段 A 不碰 `mFrameEvents`、无 GL/Vulkan、无音频、无对象增删、无 Board。
  唯一非本地触点是缺轨道错误路径的 `std::cerr`（非稳态、可接受、必要时可加 guard）。

## 5. 验证：串/并行一致性断言（临时）

GameObjectManager 内编译期 `static constexpr bool kVerifyParallelAdvance`
（默认 `false`；本地开启验证，GATE 通过后随标志一并删除）。开启时阶段 A 变为
**快照 → 并行 → 还原 → 串行 → 比对**：

1. 快照（主线程，阶段 A 前）：每个活动动画对象的推进所改标量态拷进每对象
   POD（`mFrameIndexNow/Begin/End`、`mPlayingState`、`mIsPlaying`、
   `mReanimBlendCounter`、`mSpeed`、`mCurrentTrackName`、`mTargetTrack`、
   `mEvt*`），递归含子 Animator。无大容器（阶段 A 不碰 `mFrameEvents`）。
2. 并行运行 → 记每对象推进后签名 `O_par`（同一标量集，逐字段比对）。
3. 从快照还原每个 Animator（memcpy 回标量）。
4. 串行运行同样推进，`mGameObjects` 序 → 签名 `O_ser`。此结果成为**实际生效
   态**（验证模式下游戏跑串行结果 → 即使在验证也始终正确）。
5. 逐对象比对 `O_par == O_ser`，不等则 assert + 打印对象 id / 轨道名 / 两侧
   状态，然后继续。

因 `AdvanceFrame` 是 `(this 态, 帧 dt)` 的纯函数，除非真有数据竞争否则
`O_par` 必等于 `O_ser`。直接命中我们唯一在意的危害类（串/并行发散），零生产
路径改动、无高风险纯函数重构。快照/还原仅验证模式编译，随标志删除。

## 6. 测量 GATE

同 build / 同 session A/B，预热稳态 11000 僵尸场景（沿用既有 measure-first
范式；数字是决策输入，不是设计的一部分）：

- **A 基线**：当前干净 build，仅粗粒度埋点。记 `2b.GOM_objUpdateLoop`
  （预计 ~6 ms）与 `total/frame`（~14 ms）。
- **B 阶段一（并行 ON）**：同 session。记新阶段 A 并行推进耗时、变小后的 `2b`
  （阶段 B = FireFrameEvents + 组件 + AI）、`total/frame`。一致性断言全程
  **零 mismatch**；视觉/音频/`-Debug` 碰撞框检查干净。

**决策规则：**
- **KEEP**：`total/frame` 改善 ≥ ~1 ms **且** 零断言 mismatch **且** 无
  视觉/玩法/音频回归。
- **ITERATE**（调 `kParallelUpdateThreshold` / 降 dispatch 开销）：0–1 ms
  改善、无回归。
- **REVERT**：无改善，或出现无法快速解决的断言 mismatch / 回归。

## 7. 生命周期

- 永久：两阶段 API（`UpdateParallel` 默认空 + `AnimatedObject` 覆写）、
  `Animator::AdvanceFrame`/`FireFrameEvents` 拆分、`GameObjectManager`
  阶段 A/B 派发 + 阈值/标志 fallback。
- 临时（GATE KEEP 后删）：`kVerifyParallelAdvance`、快照/还原、一致性断言。
- 不动：粗粒度 Profiler 埋点保留至整个优化收尾，届时随 `Profiler.h` 一并移除。

## 8. 风险与缓解

- **遗漏的共享触点**：缺轨道 `std::cerr` 错误路径（非稳态，可接受/可 guard）；
  若 `GetTrackRange`/`SetFrameRange` 命中可变缓存则不安全 —— 实现期需核实其只读
  不可变 `mReanim` + 只写 `this`（列为实现 plan 的前置核实项）。
- **行为发散**：由一致性断言（§5）兜底，KEEP 要求零 mismatch。
- **收益不足**：窄范围预计仅 ~1.5–2 ms；GATE 的 ITERATE/REVERT 分支处理；
  后续阶段在独立 plan 扩大范围。
- **fallback 安全**：阈值以下/标志关闭时走原串行路径，行为逐字节不变。
