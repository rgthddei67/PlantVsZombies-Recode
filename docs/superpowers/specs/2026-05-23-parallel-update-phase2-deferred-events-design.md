# 并行 Update 阶段二 —— Deferred Events 设计

**Date:** 2026-05-23
**Owner:** rgthddei67
**Status:** Design approved (brainstorming 完毕)，待落实施 plan
**Prior art:** [phase-1 design](2026-05-17-parallel-update-phase1-design.md) — 已执行并 REVERT（结论见本文 §1.2）

---

## 1. Context

### 1.1 触发与瓶颈现状

11000 僵尸压测场景 `total/frame` ~15.18ms (65.9 FPS)，主线程瓶颈在 `GameObjectManager::Update` 的 `obj->Update()` 串行循环。

**phase-1 之后（已 REVERT，2026-05-23）现状：**
- Working tree = `097df77` + Task 0 的 6 处 `PROFILE_SCOPE` 取消注释（GameObjectManager.cpp:74/111/127/152/176 + Scene.cpp:68/78），未提交
- 没有 phase-1 的任何遗留代码

### 1.2 phase-1 失败教训（必读）

**phase-1 设计 = 把 `Animator::Update` 拆成 `AdvanceFrame`（并行帧推进） + `FireFrameEvents`（串行回放回调） 两阶段。**

实测结果（11000 僵尸）：

| 维度 | A 强制串行 baseline | B phase-1 并行 | 诊断（B 拆分埋点） |
|---|---|---|---|
| total / frame | 14.54 ms | 15.36 ms（**+0.82 ms 劣化**）| 15.92 |
| 2.GOM_Update | 6.63 ms | 7.50 ms（**+0.87 ms 劣化**）| 7.58 |
| 2c.AdvanceFrame(par+dispatch) | — | — | 0.85 |
| 2c2.dispatch_emptyCal | — | — | **0.05**（dispatch 纯开销几乎免费）|
| 2d.serialPhaseB | — | — | 6.67 ≈ A baseline |

**根因（phase-1 plan §"设计核心"假设错了）：**
- 假设 "Animator 帧推进占 Update 80%" → 实测 AdvanceFrame 并行工作仅 **0.85ms（约 12%）**
- 真正的耗时不在"推帧"，在 `Animator::Update` 内的**其余部分**（帧事件触发 + 子节点递归 + 计时器）
- 拆分本身的开销（额外函数调用 / 两次进入 Animator / 子节点递归遍历两次）= **+0.89ms**，比并行节省还大

### 1.3 phase B 拆段诊断数据（决定 phase-2 方向）

通过 5 个 `constexpr bool` toggle 逐段禁用对比（已 REVERT，结果记此）：

| 段 | 占 `2.GOM_Update` |
|---|---|
| **`mAnimator->Update()` 全段**（帧推进+事件+子节点+计时器+自动销毁判断）| **4.71 ms（66%）** ← #1 大头 |
| **`GameObject::Update`**（组件遍历）| 2.93 ms（41%）|
| `Zombie::Update` 业务逻辑 | 0.18 ms（3%）|
| Plant 业务 / Glow / GOM destroy/add 处理 | 散在 |
| baseline 合计 | 7.09 ms |

（A+B 段和超 baseline 由 toggle 互相重叠 + 60-frame 噪声造成，不影响方向判断。）

**关键发现：** phase-1 拆错了切点——它只并行 `AdvanceFrame`（0.85ms），却把真正的耗时（FireFrameEvents 回调 + 子节点递归 + 计时器，~3.86ms）留在串行。**正确做法 = 把整个 `Animator::Update` 并行**，事件回调 deferred 到主线程回放。

### 1.4 范围

**本次目标：** 把 `Animator::Update` 完整搬到 worker thread；事件 callback 入 per-worker 队列；主线程串行 drain 队列。

**不在本期范围（挂"后续"）：**
- GameObject::Update 组件遍历的优化（2.93ms 是独立瓶颈，单独立项）
- Plant/Zombie 业务逻辑（0.18ms，量级太小不值得）
- 算法层削减工作（zombie 静止跳过 Update 等，另立项）

---

## 2. 设计目标

| 目标 | 指标 |
|---|---|
| Animator 并行段实际加速 | 4.71 ms → ~0.7 ms（理论；GATE 验收 ≥ 1ms total/frame 改善）|
| 数据一致性 | 临时 oracle 跑 30-60s 11000 zombie + 交互，零 `[VERIFY]` 发散行 |
| 行为兼容 | 视觉/音频/碰撞/玩法 11000 压测 + 正常关卡冒烟均无回归 |
| Fallback | 小场景（<200 对象）走原 `Animator::Update()` 串行路径，逐字节一致 |
| 回退便利 | 单一 git restore 即可回 Task 0 起点 |

---

## 3. 架构

### 3.1 双阶段 GOM Update（与 phase-1 同骨架）

```
GameObjectManager::Update():
    [处理 mObjectsToRemove / mObjectsToAdd 不变]
    
    if (mThreadPool && total >= kParallelUpdateThreshold) {
        // === phase A 并行 ===
        clear mDeferredEventBuffers (per-worker)
        mThreadPool->Dispatch(total, [](start, end) {
            slot = start / chunkSize
            for i in [start, end):
                if obj.IsActive(): obj->UpdateParallel(mDeferredEventBuffers[slot])
        })
        
        // === phase B 串行 ===
        drain mDeferredEventBuffers in slot order: invoke callbacks
        for i in [0, total): if obj.IsActive(): obj->Update()
    } else {
        // 串行 fallback（逐字节同原代码）
        for i in [0, total): if obj.IsActive(): obj->Update()
    }
```

### 3.2 phase A 内 worker 跑的工作（per object）

`obj->UpdateParallel(outBuf)` 对 `AnimatedObject` 而言：

```
mAnimator->UpdateParallelDeferred(outBuf)
mAdvancedInParallel = true
```

`Animator::UpdateParallelDeferred(outBuf)` = 原 `Animator::Update()` 改造一行：

| 原 `Animator::Update`（line） | 在 UpdateParallelDeferred 内 |
|---|---|
| 推帧 + 末尾循环 + clamp (124-162) | 一字不动 |
| 帧事件触发 (169-181 / 183-208) | `it->second.callback();` → **`outBuf.push_back({ it->second.callback });`**（callback 复制入队、不调）；persistent / erase 逻辑不变（operate on **per-instance** `mFrameEvents`，无 race）|
| 混合计时器 (210-214) | 一字不动 |
| 子节点递归 (216-223) | `child->Update()` → `child->UpdateParallelDeferred(outBuf)` |

### 3.3 phase B 内主线程串行工作

```cpp
{
    PROFILE_SCOPE("2c.PhaseB_DrainEvents");
    for (auto& buf : mDeferredEventBuffers) {
        for (auto& evt : buf) evt.cb();   // 主线程跑，所有副作用线程安全
    }
}

{
    PROFILE_SCOPE("2d.PhaseB_serialUpdate");
    for (int i = 0; i < total; ++i) {
        auto* obj = mGameObjects[i].get();
        if (obj->IsActive()) obj->Update();
    }
}
```

`AnimatedObject::Update` 内部对 `mAdvancedInParallel` 分支：

```cpp
if (mAnimator) {
    if (mAdvancedInParallel) {
        mAdvancedInParallel = false;   // events 已在 drain 中处理，Animator 状态已就位
    } else {
        mAnimator->Update();           // 串行 fallback 路径
    }
    // 自动销毁判断不变
}
```

注意：自动销毁判断（`IsAnimationFinished()` → `DestroyGameObject(this)`）保留在 `Update()` 中——它在 phase B 才执行，自然安全。

---

## 4. 接口（最小新增）

### 4.1 新增类型

```cpp
// 放 GameObject.h 或独立 ParallelUpdate.h
struct DeferredEvent {
    std::function<void()> cb;
};
```

### 4.2 Animator.h 新增

```cpp
/**
 * @brief 阶段二并行段：完整推进帧 + 计时器 + 子节点递归；遇到帧事件 = 拷贝 callback 入 outBuf。
 *        对象本地、worker 线程安全；不调用任何 callback。
 *        前置：本 Animator 与其子 Animator 树不被其他线程并发访问。
 */
void UpdateParallelDeferred(std::vector<DeferredEvent>& outBuf);
```

`Animator::Update()` 本体**不动**，保留作串行 fallback。

### 4.3 GameObject.h 新增

```cpp
// 默认空。约定——只做对象本地工作，worker 线程安全。
virtual void UpdateParallel(std::vector<DeferredEvent>& outBuf) {}
```

### 4.4 AnimatedObject.h 新增

```cpp
bool mAdvancedInParallel = false;   // 本帧 animator 是否已在并行段推进过

void UpdateParallel(std::vector<DeferredEvent>& outBuf) override;
```

### 4.5 GameObjectManager 新增

```cpp
// header
std::vector<std::vector<DeferredEvent>> mDeferredEventBuffers;  // size = numWorkers，capacity 跨帧复用
```

---

## 5. 并发安全分析

### 5.1 必须 audit PASS 的前提（Task 1 GATE）

- `Animator::mFrameEvents` 是 **per-instance**：同一对象不会跨 worker → erase/insert 无 race ✓
- `mExtraInfos[i].mAttachedReanims[j]` 的 weak_ptr 指向的 child Animator **不在多父之间共享**：必须用 Explore agent 严格核实 `Animator::AttachAnimator` 所有 caller 看 child 是否独有；若 FAIL → 需改 design（mFrameEvents 加 atomic / spinlock 或彻底放弃）
- `Reanimation::GetTrack` const 只读且加载后不变 ✓（phase-1 已 audit PASS）

### 5.2 已自审通过的安全点

- callback `std::function` 拷贝入队时是值传递，闭包内的 `this` 等捕获只是指针拷贝，不解引用
- callback 在主线程 drain 时才真正调用——所有过去"必须串行"的子调用（AudioSystem / BulletPool / CreateBullet / GameRandom / EmitEffect / DestroyGameObject）都跑在主线程，无需改造
- 自动销毁判断在 phase B 末尾跑——drain 时 obj 仍存活，闭包 `this` 不会悬空
- worker 间不共享 `mDeferredEventBuffers`（per-slot）→ push 无 race
- `DeltaTime::GetDeltaTime()` 每帧由 GameApp 更新一次，只读 ✓（phase-1 已 audit）

### 5.3 已知次要警示

- `Animator::GetTrackRange` 错误路径有 `std::cout` —— 多 worker 并发命中会交错输出，非状态损坏；稳态不触发，必要时 guard

---

## 6. 一致性 oracle（临时，KEEP 后随清理删除）

### 6.1 流程

```
foreach obj in mGameObjects (if IsActive and has Animator):
    snapshot(state_initial)
    outBuf_par.clear(); UpdateParallelDeferred(outBuf_par)
    snapshot(state_parallel)
    restore(state_initial)
    outBuf_ser.clear(); UpdateParallelDeferred(outBuf_ser)
    snapshot(state_serial)
    if !Equals(state_parallel, state_serial, diff):
        printf("[VERIFY] divergence obj#N name=X : %s", diff)
    if outBuf_par.size() != outBuf_ser.size():
        printf("[VERIFY] event count mismatch obj#N: par=%d ser=%d", par_size, ser_size)
```

### 6.2 复用 phase-1 设计（精确 diff）

复用 phase-1 的 `Animator::AdvanceSnapshot` struct + 3 方法（Snapshot / Restore / Equals），但做以下精确调整：

**字段减项**（本次不存事件窗口供延后回放，4 字段无意义）：
- 删 `int evtOld, evtNew`
- 删 `bool evtWrapped, evtPending`

**字段加项（关键，phase-1 无此需求）：**
- 加 `std::unordered_multimap<int, FrameEvent> frameEventsCopy`

**为什么 frameEventsCopy 是必须的：** 与 phase-1 不同，本次 `UpdateParallelDeferred` **会 erase 非 persistent events**（与原 `Update()` 同行为）。oracle 跑两次（并行一次、串行一次）时：
- 第一次跑：erase k 个 events，push k 个 callbacks
- 若 Restore 不恢复 mFrameEvents → 第二次跑：mFrameEvents 已少 k 个，erase 0、push 0
- → outBuf 数量 par=k vs ser=0 必然发散，oracle 假警报

修正：`SnapshotAdvanceState` 深拷贝 `mFrameEvents` 到 `frameEventsCopy`；`RestoreAdvanceState` 用 `frameEventsCopy` 整个 reassign `mFrameEvents`。`EqualsAdvanceState` **不** 比较 `frameEventsCopy`（它是辅助还原字段，非被测状态）。

复制 std::function 容器有成本（10000 对象 × ~10 events = 100000 拷贝/帧），但 oracle 只在 `kVerifyParallelAdvance=true` 时生效，本来就接受性能下降。

最终 struct：

```cpp
struct AdvanceSnapshot {
    float fNow, fBegin, fEnd, speed, origSpeed, blendCtr, blendCtrMax;
    int   blendBuf;
    bool  playing;
    PlayState pstate;
    std::string targetTrack, curTrack;
    std::unordered_multimap<int, FrameEvent> frameEventsCopy;   // 用于 Restore，不参与 Equals
    std::vector<AdvanceSnapshot> children;
};
```

oracle 流程的 outBuf 数量比对依然有意义（在正确 Restore 前提下，并行/串行的 push 总数相等是必要条件）。

三方法签名与 phase-1 一致；KEEP 后整体删除（§8.2）。

### 6.3 toggle

`GameObjectManager.cpp` 内 `static constexpr bool kVerifyParallelAdvance = false;` —— GATE 阶段本地改 true 验证后改回 false。

---

## 7. 测量 GATE（决策点）

### 7.1 测量流程

1. **A 基线**：把 `kParallelUpdateThreshold` 改 999999 强制走串行 fallback，记 11000 zombie 预热稳态 `FRAME PROFILE`
2. **B 并行**：阈值 200（默认），同 session 同场景同预热时长，记 `FRAME PROFILE`
3. **可选 B-detail**：在 B 模式下加 `2c.PhaseB_DrainEvents` 与 `2d.PhaseB_serialUpdate` scope，看 drain 实际耗时是否符合预期

### 7.2 决策规则

| 结果 | 条件 | 动作 |
|---|---|---|
| **KEEP** | `total/frame` 改善 ≥ 1ms **且** 验证模式零发散 **且** 无视觉/音频/玩法回归 | 进清理 Task |
| **ITERATE** | 0–1ms 改善 + 无回归 | 调阈值 / 加诊断埋点定位"隐藏成本"（`std::function` heap alloc / drain 跨核读取）后重测 |
| **REVERT** | 无改善 / 发散 / 回归 | `git restore` 受影响文件，记内存 |

### 7.3 预期收益（基于 §1.3 拆段数据）

| 维度 | baseline | 预测 B | 改善 |
|---|---|---|---|
| Animator 段 | 4.71 ms | ~0.7 ms（4.71/8 + dispatch 0.05 + drain 0.1）| -4 ms |
| 其他 GOM_Update | 2.38 ms | 2.38 ms | 0 |
| **2.GOM_Update** | **7.09 ms** | **~3 ms** | **-4 ms** |
| **total/frame** | **15.18 ms** | **~11 ms** | **-4 ms（~90 FPS）** |

⚠️ 实测远低于预期时**不立即 REVERT**——先加 drain/function-copy 诊断埋点定位隐藏成本（phase-1 教训：dispatch 不是问题，但其他东西可能是）。

---

## 8. 永久 vs 临时

### 8.1 永久保留（KEEP 后）

- `DeferredEvent` struct
- `Animator::UpdateParallelDeferred`
- `GameObject::UpdateParallel(buf)` virtual + AnimatedObject override
- `AnimatedObject::mAdvancedInParallel` + Update 内分支
- `GameObjectManager::mDeferredEventBuffers` + 阶段 A/B 派发 + 阈值 fallback

### 8.2 KEEP 后删除（清理 Task）

- `Animator::AdvanceSnapshot` struct + 3 方法（声明+实现）
- `kVerifyParallelAdvance` toggle + 整个验证分支
- 验证模式相关 PROFILE_SCOPE（保留 `2c.PhaseB_DrainEvents` / `2d.PhaseB_serialUpdate` 直到整个 Profiler.h 移除阶段，不在本 plan 范围）

---

## 9. 风险与缓解

| 风险 | 概率 | 缓解 |
|---|---|---|
| cross-parent shared child Animator → mFrameEvents race | 低 | Task 1 audit GATE 显式核实 |
| `std::function<void()>` copy 触发 heap alloc | 中 | drain 子段单独 scope 测；若是瓶颈，改 small-buffer / 预 reserve |
| cache 抖动（10000 Animator 跨核读写）| 低 | phase-1 已证 dispatch 0.05ms 几乎免费 |
| 回放顺序变化导致音效/玩法可感知差异 | 低 | 用户已接受 slot 间顺序变化；-Debug 跑 30-60s 交互复核 |
| callback 闭包持有 raw pointer 在 drain 时悬空 | 极低 | 帧事件 callback 通常 `[this]` 捕获 self；drain 在 destroy 之前发生 |
| 并发 UpdateParallelDeferred 的 child weak_ptr.lock() 间冲突 | 极低 | lock() 是 thread-safe 操作；前提是 child 不跨父共享（§5.1 第二点）|

### 9.1 回退路径

`git restore` 受影响 5-6 个文件 → working tree 回到 Task 0 起点（PROFILE_SCOPE only）。无须任何额外工作。

---

## 10. Brainstorming 决策记录（追溯）

| 决策点 | 选择 | 理由 |
|---|---|---|
| deferred 粒度 | A: 整个回调入队（vs B: 细粒 deferred 不安全子调用）| 零调用点改造；callback 内"对象本地工作"占比小（基于 phase-1 + 拆段诊断推断）|
| 回放顺序 | slot 0 → slot 1 → ...，slot 内对象按物理序 | 用户接受；多数 callback 是音频/spawn，slot 间无依赖 |
| child Animator 共享假设 | 假设独有，Task 1 audit 显式验证 | 若失败可降级设计 |
| 串行 fallback | `Animator::Update()` 本体不动 | 与 phase-1 同；小场景与原行为逐字节一致 |
| oracle 设计 | 复用 phase-1 Snapshot/Restore/Equals + 加 outBuf.size() 对比 | 最小新增 |

---

## Self-Review Checklist

- [ ] Placeholder scan: 无 TBD / TODO / "类似 X" / vague requirements
- [ ] 内部一致性: §3 架构 与 §4 接口 与 §6 oracle 同步
- [ ] Scope: 焦点单一（仅 Animator 并行化 + deferred callback），未触 GameObject components 或算法削减
- [ ] Ambiguity: child Animator 共享假设明确；drain 顺序明确
