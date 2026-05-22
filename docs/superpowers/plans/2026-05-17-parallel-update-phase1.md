# 并行 Update —— 阶段一（仅 animator 推进）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 Animator 帧推进从 `GameObjectManager::Update` 的串行逐对象循环里拆出来并行执行，帧事件回调延迟到主线程串行回放，目标削减实测 ~6 ms 串行段。

**Architecture:** `Animator::Update()` 拆出两个**新增**方法 `AdvanceFrame()`（并行、对象本地）+ `FireFrameEvents()`（串行、按记录窗口回放）；`Animator::Update()` 本体**不动**（串行 fallback 逐字节一致）。`GameObjectManager::Update` 新增阶段 A（线程池并行跑 `obj->UpdateParallel()`）+ 阶段 B（主线程按 `mGameObjects` 序串行跑 `obj->Update()`）。临时一致性断言做快照→并行→还原→串行→比对。

**Tech Stack:** C++17，VS 2026，现有 `ThreadPool::Dispatch(int,std::function<void(int,int)>)`，`Profiler.h` 粗粒度埋点。

**测试模型（本仓库无自动化测试框架，CLAUDE.md 禁止 assistant 构建）：** 每个代码任务的验证 = (1) 用户在 VS F7 构建；(2) 临时串/并行一致性断言（自动正确性 oracle）；(3) Task 6 的 GATE 实测。提交均由用户驱动（项目既定规范）；plan 内"提交"步骤是用户检查点，assistant 不自动提交。

**原子性约束（照搬既往 perf plan 教训）：** Task 2、3 各自行为逐字节一致、可单独构建验证，是安全检查点。Task 4、5 一起才开启"并行 + 验证"。**Task 5 之前不要跑 GATE 实测**（Task 2/3 不改变性能）。

**前置事实（已在规划期核实，写给零上下文工程师）：**
- 源码根目录是嵌套的 `D:\PVZ\PlantsVsZombies\PlantVsZombies\PlantVsZombies\`；文档在 `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\`。
- `Animator::Update()` 当前实现于 `PlantVsZombies/Reanimation/Animator.cpp:124-224`。
- `AdvanceFrame` 外部调用面：`PlayTrack`(Animator.cpp:63) → `GetTrackRange`(只读不可变 `mReanim->GetTrack`，错误路径 `std::cout`) / `SetFrameRange`(只写 `this`) / `SetSpeed`(只写 `this`+自有子节点)；`RestoreSpeed`→`SetSpeed`。全部对象本地。

---

### Task 1: 核实并行安全前置（只读代码，产出书面结论，GATE）

**Files:**
- Read: `PlantVsZombies/Reanimation/Animator.cpp:124-224`（`Update`）
- Read: `PlantVsZombies/Reanimation/Animator.cpp:63-96`（`PlayTrack`）
- Read: `PlantVsZombies/Reanimation/Animator.cpp:341-352`（`SetSpeed`）、`496-544`（`GetTrackRange`/`SetFrameRange`）
- Read: `PlantVsZombies/Reanimation/Reanimation.*` 中 `GetTrack(const std::string&)` 的实现

- [ ] **Step 1: 逐函数确认写入面**

读上述代码，对 `AdvanceFrame` 将包含的逻辑（Update 第 124-162 行 + 递归子节点 `AdvanceFrame`）逐一确认其调用的每个函数：
- `DeltaTime::GetDeltaTime()` —— 必须只读（每帧 `Scene::Update` 前由 `GameApp` 设置一次）。
- `PlayTrack` / `GetTrackRange` / `SetFrameRange` / `SetSpeed` / `RestoreSpeed` —— 必须只读不可变 `mReanim`、只写 `this` 或自有子 Animator，不写任何全局/静态/共享容器，不碰 GL/Vulkan/音频/对象增删。
- `Reanimation::GetTrack` —— 必须 `const`/只读，且 `Reanimation` 资源加载后游戏期不被改写。

- [ ] **Step 2: 写下结论**

在本文件 Task 1 末尾追加一行 `结论：PASS` 或 `结论：FAIL —— <原因>`。
- **PASS 判据：** 上述全部成立（仅 `GetTrackRange` 缺轨道错误路径有 `std::cout`，属非稳态、可接受）。
- **FAIL 处理：** 立即停止本 plan，回 brainstorming 修订 spec（需把相关写操作改为延迟到阶段 B）。

- [ ] **Step 3: 用户检查点**

把结论贴给用户确认后再进入 Task 2。无代码改动，无需构建/提交。

**结论：PASS**（2026-05-17，独立 Explore subagent 审计 + 规划期核实，双重确认）
- Update 124-162 调用面：`DeltaTime::GetDeltaTime()`(只读每帧静态值, DeltaTime.h:59)、`PlayTrack`(只写 this+子节点)、`RestoreSpeed`→`SetSpeed`、`std::clamp`。
- `PlayTrack`(Animator.cpp:63-96) 只写 this 成员 + 调 `SetSpeed`/`GetTrackRange`/`SetFrameRange`。
- `GetTrackRange`(496-539) 只读不可变 `mReanim->GetTrack`；错误路径 `std::cout`@498/504/519/532（非稳态诊断，可接受）。
- `SetFrameRange`(540-544) 只写 this 三个成员。`SetSpeed`(341-352) 只写 this.mSpeed + 递归自有子节点。
- `Reanimation::GetTrack`(Reanimation.cpp:193-207) const 只读，`mTracks` 资源加载期填充、游戏期不改。
- 无 static 局部 / 全局 / 单例写 / GL·Vulkan·音频·对象增删（除上述 4 行 `std::cout` 错误路径）。
- 已知次要警示：多 worker 并发命中错误路径的 `std::cout` 会交错输出（非状态损坏，仅控制台文字乱）；稳态不触发，必要时可 guard。

---

### Task 2: Animator 拆出 `AdvanceFrame()` + `FireFrameEvents()`，`Update()` 不变

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（新增 4 个成员 + 2 个方法声明）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（新增 2 个方法实现，`Update()` 保持原样）

- [ ] **Step 1: Animator.h 新增成员**

在 `Animator.h` 第 68 行 `std::unordered_multimap<int, FrameEvent> mFrameEvents;` 之后、第 70 行 `public:` 之前插入：

```cpp
    // ---- 阶段一并行：AdvanceFrame 记录帧事件窗口，FireFrameEvents 串行回放 ----
    int  mEvtOldInt  = 0;
    int  mEvtNewInt  = 0;
    bool mEvtWrapped = false;
    bool mEvtPending = false;
```

- [ ] **Step 2: Animator.h 新增方法声明**

在 `Animator.h` 第 325 行 `void Update();` 之后插入：

```cpp
    /**
     * @brief 阶段一并行段：仅推进帧索引/末尾循环/裁剪 + 记录帧事件窗口 + 递归子节点 AdvanceFrame。
     *        对象本地、worker 线程安全；不触发回调、不碰 mFrameEvents。
     */
    void AdvanceFrame();

    /**
     * @brief 阶段一串行段：按 AdvanceFrame 记录的窗口触发帧事件 + 混合计时 + 递归子节点 FireFrameEvents。
     *        必须在主线程、对应对象 AdvanceFrame 之后调用。
     */
    void FireFrameEvents();
```

- [ ] **Step 3: Animator.cpp 新增 `AdvanceFrame()` 实现**

在 `Animator.cpp` 第 224 行（`Update()` 的结束 `}`）之后插入：

```cpp
void Animator::AdvanceFrame() {
    if (!mIsPlaying || !mReanim) { mEvtPending = false; return; }

    float deltaTime = DeltaTime::GetDeltaTime();
    float oldFrame = mFrameIndexNow;

    float frameAdvance = deltaTime * mFPS * mSpeed * mExtraSpeedMultiplier;
    mFrameIndexNow += frameAdvance;

    bool reachedEnd = mFrameIndexNow >= mFrameIndexEnd;
    if (reachedEnd) {
        switch (mPlayingState) {
        case PlayState::PLAY_REPEAT:
            mFrameIndexNow = mFrameIndexBegin;
            break;
        case PlayState::PLAY_ONCE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        case PlayState::PLAY_ONCE_TO:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            if (!mTargetTrack.empty()) {
                PlayTrack(mTargetTrack, 0.0f, 0.5f);
                RestoreSpeed();
                mTargetTrack = "";
            }
            break;
        case PlayState::PLAY_NONE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        }
    }

    mFrameIndexNow = std::clamp(mFrameIndexNow, mFrameIndexBegin, mFrameIndexEnd);

    // 记录帧事件窗口（取点与原 Update 的 oldFrame@128 / newInt@166 完全一致）
    mEvtOldInt  = static_cast<int>(oldFrame);
    mEvtNewInt  = static_cast<int>(mFrameIndexNow);
    mEvtWrapped = (mEvtNewInt < mEvtOldInt);   // 原条件 if (newInt >= oldInt) 的反面
    mEvtPending = true;

    // 递归子节点推进（与原 Update 第 216-223 的遍历结构一致，仅 child->Update 换 child->AdvanceFrame）
    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->AdvanceFrame();
            }
        }
    }
}

void Animator::FireFrameEvents() {
    if (!mEvtPending) return;
    mEvtPending = false;

    int oldInt = mEvtOldInt;
    int newInt = mEvtNewInt;

    if (!mEvtWrapped) {
        // 正常前进或不变（原 Update 第 169-181 行，原条件 newInt >= oldInt）
        for (int f = oldInt + 1; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }
    else {
        // 发生了回绕（循环播放）（原 Update 第 183-208 行）
        int endInt = static_cast<int>(mFrameIndexEnd);
        for (int f = oldInt + 1; f <= endInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
        int beginInt = static_cast<int>(mFrameIndexBegin);
        for (int f = beginInt; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }

    // 更新混合计时器（原 Update 第 210-214 行；deltaTime 每帧恒定，重取等价）
    float deltaTime = DeltaTime::GetDeltaTime();
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        if (mReanimBlendCounter < 0) mReanimBlendCounter = 0;
    }

    // 递归子节点回放
    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->FireFrameEvents();
            }
        }
    }
}
```

**注意：** 不要修改 `Animator::Update()`（124-224）。它保持原样，作为串行 fallback 的逐字节一致路径。新方法此时还没有调用方。

- [ ] **Step 4: 用户构建验证**

请用户在 VS F7 构建。预期：编译通过，无新增警告/错误。运行游戏（正常关卡，非压测）：行为与改动前完全一致（新方法尚无调用方，纯增量）。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段一 step1：Animator 拆出 AdvanceFrame/FireFrameEvents（Update 不变）`。assistant 不自动提交。

---

### Task 3: GameObject/AnimatedObject 两阶段 API（并行路径尚未接通）

**Files:**
- Modify: `PlantVsZombies/Game/GameObject.h:114`（新增虚函数默认空实现）
- Modify: `PlantVsZombies/Game/AnimatedObject.h`（新增成员 + override 声明）
- Modify: `PlantVsZombies/Game/AnimatedObject.cpp`（新增 `UpdateParallel`，改 `Update` 内一行）

- [ ] **Step 1: GameObject.h 新增虚函数**

在 `GameObject.h` 第 114 行 `virtual void Update();` 之后插入：

```cpp
    // 阶段一并行：默认空。约定——只做对象本地工作，worker 线程安全。
    virtual void UpdateParallel() {}
```

- [ ] **Step 2: AnimatedObject.h 新增成员与 override**

在 `AnimatedObject.h` 第 28 行 `bool mAutoDestroy;` 之后插入：

```cpp
    bool mAdvancedInParallel = false;   // 阶段一：本帧 animator 是否已在并行段推进过
```

在 `AnimatedObject.h` 第 99 行 `void Update() override;` 之后插入：

```cpp
    void UpdateParallel() override;
```

- [ ] **Step 3: AnimatedObject.cpp 新增 `UpdateParallel` + 改 `Update` 一行**

`AnimatedObject.cpp` 当前 `Update()`：

```cpp
void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		mAnimator->Update();

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
```

将其替换为（仅 `mAnimator->Update();` 那一行变条件，并在函数前新增 `UpdateParallel`）：

```cpp
void AnimatedObject::UpdateParallel() {
	if (mAnimator) mAnimator->AdvanceFrame();
	mAdvancedInParallel = true;
}

void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		if (mAdvancedInParallel) { mAnimator->FireFrameEvents(); mAdvancedInParallel = false; }
		else                     { mAnimator->Update(); }

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
```

（`Update()` 其余部分一字不动。`AnimatedObject.cpp` 已 `#include "GameObjectManager.h"`，无需新增 include。）

- [ ] **Step 4: 用户构建验证**

请用户在 VS F7 构建。预期：编译通过。运行正常关卡：行为与改动前完全一致 —— 因为还没有任何代码调用 `UpdateParallel()`，`mAdvancedInParallel` 恒为 `false`，`Update()` 仍走 `mAnimator->Update()`（与原路径逐字节一致）。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段一 step2：新增 UpdateParallel 两阶段 API（并行路径未接通）`。

---

### Task 4: 临时一致性断言脚手架（Animator 快照/还原/比对）

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（新增临时 struct + 3 个方法声明）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（新增 3 个方法实现）

> 本任务全部为**临时**代码，Task 7 在 GATE KEEP 后整体删除。

- [ ] **Step 1: Animator.h 新增临时快照结构与方法声明**

在 `Animator.h` 第 426 行（类结束 `};`）之前插入：

```cpp
public:
    // ===== 临时（阶段一验证脚手架，GATE KEEP 后随 Task 7 删除）=====
    struct AdvanceSnapshot {
        float fNow, fBegin, fEnd, speed, origSpeed, blendCtr, blendCtrMax;
        int   blendBuf, evtOld, evtNew;
        bool  playing, evtWrapped, evtPending;
        PlayState pstate;
        std::string targetTrack, curTrack;
        std::vector<AdvanceSnapshot> children;
    };
    void SnapshotAdvanceState(AdvanceSnapshot& s) const;
    void RestoreAdvanceState(const AdvanceSnapshot& s);
    static bool EqualsAdvanceState(const AdvanceSnapshot& a,
                                   const AdvanceSnapshot& b,
                                   std::string& diffOut);
    // ===== 临时结束 =====
```

- [ ] **Step 2: Animator.cpp 新增三方法实现**

在 `Animator.cpp` 文件末尾（最后一个 `}` 之后）插入：

```cpp
// ===== 临时（阶段一验证脚手架，GATE KEEP 后随 Task 7 删除）=====
void Animator::SnapshotAdvanceState(AdvanceSnapshot& s) const {
    s.fNow = mFrameIndexNow;   s.fBegin = mFrameIndexBegin;   s.fEnd = mFrameIndexEnd;
    s.speed = mSpeed;          s.origSpeed = mOriginalSpeed;
    s.blendCtr = mReanimBlendCounter; s.blendCtrMax = mReanimBlendCounterMax;
    s.blendBuf = mFrameIndexBlendBuffer;
    s.evtOld = mEvtOldInt; s.evtNew = mEvtNewInt;
    s.playing = mIsPlaying; s.evtWrapped = mEvtWrapped; s.evtPending = mEvtPending;
    s.pstate = mPlayingState;
    s.targetTrack = mTargetTrack; s.curTrack = mCurrentTrackName;
    s.children.clear();
    for (size_t i = 0; i < mExtraInfos.size(); i++)
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) { s.children.emplace_back(); child->SnapshotAdvanceState(s.children.back()); }
        }
}

void Animator::RestoreAdvanceState(const AdvanceSnapshot& s) {
    mFrameIndexNow = s.fNow; mFrameIndexBegin = s.fBegin; mFrameIndexEnd = s.fEnd;
    mSpeed = s.speed; mOriginalSpeed = s.origSpeed;
    mReanimBlendCounter = s.blendCtr; mReanimBlendCounterMax = s.blendCtrMax;
    mFrameIndexBlendBuffer = s.blendBuf;
    mEvtOldInt = s.evtOld; mEvtNewInt = s.evtNew;
    mIsPlaying = s.playing; mEvtWrapped = s.evtWrapped; mEvtPending = s.evtPending;
    mPlayingState = s.pstate;
    mTargetTrack = s.targetTrack; mCurrentTrackName = s.curTrack;
    size_t k = 0;
    for (size_t i = 0; i < mExtraInfos.size(); i++)
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child && k < s.children.size()) child->RestoreAdvanceState(s.children[k++]);
        }
}

bool Animator::EqualsAdvanceState(const AdvanceSnapshot& a,
                                  const AdvanceSnapshot& b,
                                  std::string& diffOut) {
    auto fne = [](float x, float y){ return std::abs(x - y) > 1e-4f; };
    if (fne(a.fNow,b.fNow) || fne(a.fBegin,b.fBegin) || fne(a.fEnd,b.fEnd) ||
        fne(a.speed,b.speed) || fne(a.origSpeed,b.origSpeed) ||
        fne(a.blendCtr,b.blendCtr) || fne(a.blendCtrMax,b.blendCtrMax) ||
        a.blendBuf!=b.blendBuf || a.evtOld!=b.evtOld || a.evtNew!=b.evtNew ||
        a.playing!=b.playing || a.evtWrapped!=b.evtWrapped || a.evtPending!=b.evtPending ||
        a.pstate!=b.pstate || a.targetTrack!=b.targetTrack || a.curTrack!=b.curTrack ||
        a.children.size()!=b.children.size()) {
        diffOut = "fNow " + std::to_string(a.fNow) + " vs " + std::to_string(b.fNow) +
                  " | track " + a.curTrack + " vs " + b.curTrack;
        return false;
    }
    for (size_t i = 0; i < a.children.size(); ++i)
        if (!EqualsAdvanceState(a.children[i], b.children[i], diffOut)) return false;
    return true;
}
// ===== 临时结束 =====
```

- [ ] **Step 3: 用户构建验证**

请用户在 VS F7 构建。预期：编译通过（`std::abs`/`std::to_string` 已随 `<algorithm>`/标准库可用；`<cmath>` 若缺则在 `Animator.cpp` 顶部加 `#include <cmath>`）。运行正常关卡：行为不变（这些方法仍无调用方）。

- [ ] **Step 4: 用户提交检查点（用户驱动）**

提交建议信息：`阶段一 step3：临时一致性断言脚手架（Snapshot/Restore/Equals）`。

---

### Task 5: 接通 GameObjectManager 阶段 A/B + 阈值 + 验证模式

**Files:**
- Modify: `PlantVsZombies/Game/GameObjectManager.cpp:73-107`（`Update()` 内 `2b` 段）

- [ ] **Step 1: 替换 `2b.GOM_objUpdateLoop` 段**

`GameObjectManager.cpp` 当前（Task 起点）该段为：

```cpp
	{
		PROFILE_SCOPE("2b.GOM_objUpdateLoop(serial)");
		// 现有对象
		for (size_t i = 0; i < mGameObjects.size(); i++) {
			auto obj = mGameObjects[i].get();
			if (obj->IsActive()) {
				obj->Update();
			}
		}
	}
```

替换为：

```cpp
	{
		PROFILE_SCOPE("2b.GOM_objUpdateLoop(serial)");
		const int total = static_cast<int>(mGameObjects.size());
		constexpr int kParallelUpdateThreshold = 200;            // 与并行 Draw 阈值同量级；GATE 可调
		static constexpr bool kVerifyParallelAdvance = false;    // 临时：串/并行一致性断言开关，GATE 用时本地置 true

		if (mThreadPool && total >= kParallelUpdateThreshold) {
			if (kVerifyParallelAdvance) {
				// —— 验证模式：快照 → 并行推进 → 还原 → 串行推进 → 比对；生效态=串行结果 ——
				static std::vector<Animator::AdvanceSnapshot> snaps;
				static std::vector<Animator::AdvanceSnapshot> parSig;
				snaps.clear();  snaps.resize(total);
				parSig.clear(); parSig.resize(total);

				auto animOf = [this](int i) -> Animator* {
					auto* ao = dynamic_cast<AnimatedObject*>(mGameObjects[i].get());
					return ao ? ao->GetAnimatorInternal().get() : nullptr;
				};

				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->SnapshotAdvanceState(snaps[i]); }
				}
				mThreadPool->Dispatch(total, [this](int s, int e) {
					for (int i = s; i < e; ++i) {
						auto* o = mGameObjects[i].get();
						if (o->IsActive()) o->UpdateParallel();
					}
				});
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->SnapshotAdvanceState(parSig[i]); }
				}
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->RestoreAdvanceState(snaps[i]); }
				}
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->UpdateParallel();   // 串行参考（生效态）
				}
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (!obj->IsActive()) continue;
					auto* a = animOf(i);
					if (!a) continue;
					Animator::AdvanceSnapshot serSig;
					a->SnapshotAdvanceState(serSig);
					std::string diff;
					if (!Animator::EqualsAdvanceState(parSig[i], serSig, diff)) {
						std::printf("[VERIFY] 串/并行发散 obj#%d name=%s : %s\n",
							i, obj->GetName().c_str(), diff.c_str());
					}
				}
			} else {
				// —— 阶段 A：并行推进（仅 animator 帧推进，对象本地）——
				mThreadPool->Dispatch(total, [this](int start, int end) {
					for (int i = start; i < end; ++i) {
						auto* obj = mGameObjects[i].get();
						if (obj->IsActive()) obj->UpdateParallel();
					}
				});
			}

			// —— 阶段 B：串行（mGameObjects 序），其余 Update 全部在此 ——
			for (int i = 0; i < total; ++i) {
				auto* obj = mGameObjects[i].get();
				if (obj->IsActive()) obj->Update();
			}
		} else {
			// —— 串行 fallback：与原代码逐字节一致 ——
			for (int i = 0; i < total; ++i) {
				auto* obj = mGameObjects[i].get();
				if (obj->IsActive()) obj->Update();
			}
		}
	}
```

- [ ] **Step 2: 确认 include**

`GameObjectManager.cpp` 顶部需能见到 `AnimatedObject` 与 `Animator` 定义（用于 `dynamic_cast` 与 `AdvanceSnapshot`）。当前仅 `#include "GameObjectManager.h"` 与 `"../Profiler.h"`。在 `#include "../Profiler.h"` 之后加：

```cpp
#include "AnimatedObject.h"
```

（`AnimatedObject.h` 已 `#include "../Reanimation/Animator.h"`，`Animator::AdvanceSnapshot` 随之可见。）

- [ ] **Step 3: 用户构建验证（验证模式关）**

请用户在 VS F7 构建（`kVerifyParallelAdvance=false`）。运行正常关卡 + 11000 僵尸压测：动画/掉头掉臂/音效/`-Debug` 碰撞框均正常，无崩溃、无明显异常。此时并行路径已生效。

- [ ] **Step 4: 用户构建验证（验证模式开）**

请用户把 `GameObjectManager.cpp` 内 `kVerifyParallelAdvance` 改为 `true`，F7 构建，跑 11000 僵尸场景约 30–60 秒并做若干交互（种植/僵尸啃食/掉臂）。预期：调试控制台**零** `[VERIFY] 串/并行发散` 行。若出现，记录 obj 名与 diff，停止并按 systematic-debugging 定位（多半是 `AdvanceFrame` 触到了未预期的共享/静态状态），不要继续 GATE。验证通过后把 `kVerifyParallelAdvance` 改回 `false`。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段一 step4：接通并行阶段A/B + 临时一致性断言（已验证零发散）`。

---

### Task 6: 测量 GATE（用户实测，决策点）

**Files:** 无代码改动。

- [ ] **Step 1: A 基线（并行关）**

`GameObjectManager.cpp` 把 `kParallelUpdateThreshold` 临时改为一个大于 11000 的值（例如 `999999`）强制走串行 fallback —— 等价基线（逐字节一致）。F7 构建，预热后稳态 11000 僵尸场景，记录整块 `FRAME PROFILE`（重点 `2b.GOM_objUpdateLoop` 与 `total/frame`）。然后把阈值改回 `200`。

- [ ] **Step 2: B 阶段一（并行开）**

同一 session、同 build 配置，阈值 `200`（并行生效，`kVerifyParallelAdvance=false`）。同样预热后稳态 11000 僵尸场景，记录整块 `FRAME PROFILE`。同时肉眼 + `-Debug` 复核视觉/音频/碰撞框无回归。

- [ ] **Step 2.5: 仅在需要时再加细分埋点（最小化构建轮次）**

把以下两块贴给 assistant 决策（决策规则按 spec §6）：
- A 与 B 的完整 `FRAME PROFILE`；
- 视觉/音频/`-Debug` 复核结论；
- 验证模式（Task 5 Step 4）是否零发散。

- [ ] **Step 3: 决策（assistant + 用户）**

按 spec §6 决策规则：
- **KEEP**：`total/frame` 改善 ≥ ~1 ms **且** 验证模式零发散 **且** 无视觉/玩法/音频回归 → 进 Task 7。
- **ITERATE**：0–1 ms 改善、无回归 → 调 `kParallelUpdateThreshold` / 降 dispatch 开销后重测 Step 2；不进 Task 7。
- **REVERT**：无改善，或出现无法快速解决的发散/回归 → `git checkout` 回 Task 1 起点 commit，记录结论到 [[pvz-perf-optimization]] 内存，结束本 plan。

---

### Task 7: GATE KEEP 后清理临时脚手架（条件性）

**前置：** 仅当 Task 6 决策 = KEEP 才执行。

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（删 Task 4 Step 1 的 `AdvanceSnapshot`/3 方法声明块）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（删 Task 4 Step 2 的"临时"实现块；若加过 `#include <cmath>` 且无他用则删）
- Modify: `PlantVsZombies/Game/GameObjectManager.cpp`（删 `kVerifyParallelAdvance` 整个 `if (kVerifyParallelAdvance) {...} else` 验证分支，只留阶段 A 的 `Dispatch`）

- [ ] **Step 1: 删除 Animator 临时块**

删掉 `Animator.h` 中 `// ===== 临时（阶段一验证脚手架...` 到 `// ===== 临时结束 =====` 整块（含 3 个方法声明与 `AdvanceSnapshot`）。删掉 `Animator.cpp` 中对应的 `// ===== 临时（...` 到 `// ===== 临时结束 =====` 实现块。

- [ ] **Step 2: 精简 GameObjectManager 验证分支**

把 Task 5 Step 1 的整段中 `if (mThreadPool && total >= kParallelUpdateThreshold) { ... }` 内层简化为只剩阶段 A，即：

```cpp
		if (mThreadPool && total >= kParallelUpdateThreshold) {
			// 阶段 A：并行推进（仅 animator 帧推进，对象本地）
			mThreadPool->Dispatch(total, [this](int start, int end) {
				for (int i = start; i < end; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->UpdateParallel();
				}
			});
			// 阶段 B：串行（mGameObjects 序），其余 Update 全部在此
			for (int i = 0; i < total; ++i) {
				auto* obj = mGameObjects[i].get();
				if (obj->IsActive()) obj->Update();
			}
		} else {
			// 串行 fallback：与原代码逐字节一致
			for (int i = 0; i < total; ++i) {
				auto* obj = mGameObjects[i].get();
				if (obj->IsActive()) obj->Update();
			}
		}
```

删掉 `static constexpr bool kVerifyParallelAdvance` 那一行。保留 `#include "AnimatedObject.h"`（阶段 A 的 `obj->UpdateParallel()` 虚调用无需它，但保留无害；若想最简可在确认无其他依赖后删）。

> 保留项（永久）：`Animator::AdvanceFrame`/`FireFrameEvents`、`GameObject::UpdateParallel`、`AnimatedObject::UpdateParallel`/`mAdvancedInParallel`、阶段 A/B 派发与阈值 fallback。粗粒度 `Profiler` 埋点按既有计划保留至整个优化收尾再随 `Profiler.h` 移除（不在本 plan 范围）。

- [ ] **Step 3: 用户构建验证**

请用户 F7 构建并重跑 11000 僵尸场景：性能与 Task 6 B 测一致，视觉/音频无回归，临时验证代码已无残留（全局搜索 `kVerifyParallelAdvance` / `AdvanceSnapshot` 应为零）。

- [ ] **Step 4: 用户提交检查点（用户驱动）**

提交建议信息：`阶段一完成：并行 animator 推进（清理验证脚手架，GATE 已 KEEP）`。更新 [[pvz-perf-optimization]] 内存记录本阶段结果与下一阶段指向。

---

## Self-Review

**Spec 覆盖核对（对照 2026-05-17-parallel-update-phase1-design.md）：**
- §2 窄范围（仅 animator 推进）→ Task 2/3/5。✓
- §4.1 两阶段 API（`UpdateParallel` 默认空 + `AnimatedObject` 覆写 + `mAdvancedInParallel` + `Update` 条件）→ Task 3。✓
- §4.2 GOM 阶段 A/B + 阈值/fallback → Task 5。✓
- §4.3 Animator 拆分 + `mEvt*` 记录窗口 → Task 2。✓ **细化：** spec §4.1/§4.3 "`Update()`=`AdvanceFrame();FireFrameEvents()` 逐字节一致" 收紧为——**串行 fallback 的 `Update()` 本体不改、真正逐字节一致**；`AdvanceFrame`/`FireFrameEvents` 为并行路径专用新方法；并行路径存在"对象内 父事件↔子推进 / 跨对象 事件↔推进"重排，属任何并行推进方案固有、已记录、由一致性断言覆盖真实竞争。已在本 plan 头与 Task 2 注明，需用户知晓。
- §4.4 并发安全 → Task 1 核实 + Task 2 设计。✓
- §5 一致性断言（快照→并行→还原→串行→比对）→ Task 4 + Task 5 验证模式。✓
- §6 GATE 决策规则 → Task 6。✓
- §7 生命周期（永久 vs 临时）→ Task 7 + Task 7 末保留项说明。✓
- §8 风险（`GetTrackRange`/`SetFrameRange` 只读核实）→ Task 1 显式 GATE。✓

**Placeholder 扫描：** 无 TBD/TODO/"类似 TaskN"/"加适当错误处理"；每个代码步给出完整代码。✓

**类型/签名一致性：** `AdvanceFrame()`/`FireFrameEvents()`/`UpdateParallel()`/`mAdvancedInParallel`/`mEvt{Old,New}Int`/`mEvtWrapped`/`mEvtPending`/`AdvanceSnapshot`/`SnapshotAdvanceState`/`RestoreAdvanceState`/`EqualsAdvanceState`/`GetAnimatorInternal()`（AnimatedObject.h:97 已有）/`GetName()`（GameObject.h:153 已有）/`ThreadPool::Dispatch(int,std::function<void(int,int)>)`（ThreadPool.h:32 已有）—— 全任务一致，均对应已存在或本 plan 定义的符号。✓
