# 并行 Update 阶段二 —— Deferred Events Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `Animator::Update` 完整搬到 worker thread；事件 callback 入 per-worker 队列；主线程串行 drain 队列后跑剩余业务。预期总帧时 15.18 → ~11 ms（~90 FPS），削减 ~4ms。

**Architecture:** Animator 新增 `UpdateParallelDeferred(outBuf)`（与 `Update()` 同行为，但 callback 入队、不调；子节点递归同样走 deferred）。`GameObject::UpdateParallel(outBuf)` 默认空。`AnimatedObject::UpdateParallel` 调 Animator 的 deferred 版。`GameObjectManager::Update` 两阶段：A `mThreadPool->Dispatch` 并行 `obj->UpdateParallel`（事件 push 到 per-slot buffer），B 主线程 drain 所有 buffer + 跑原 `Update()`。临时一致性断言做快照→并行→还原→串行→比对（复用 phase-1 同款 oracle 减字段+加 frameEventCount）。

**Tech Stack:** C++17，VS 2026，现有 `ThreadPool::Dispatch(int,std::function<void(int,int)>)`，`Profiler.h` 粗粒度埋点。

**测试模型（本仓库无自动化测试框架，CLAUDE.md 禁止 assistant 构建）：** 每个代码任务的验证 = (1) 用户在 VS F7 构建；(2) 临时串/并行一致性断言（自动正确性 oracle）；(3) Task 6 的 GATE 实测。提交均由用户驱动；plan 内"提交"步骤是用户检查点，assistant 不自动提交。

**原子性约束（照搬 phase-1 教训）：** Task 2、3、4 各自行为逐字节一致、可单独构建验证，是安全检查点。Task 5 接通并行，是首次行为变化点。**Task 5 之前不要跑 GATE 实测**（Task 2/3/4 不改变性能）。

**前置事实（已在 brainstorming 期核实，写给零上下文工程师）：**
- 源码根目录是嵌套的 `D:\PVZ\PlantsVsZombies\PlantVsZombies\PlantVsZombies\`；文档/spec 在 `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\`。
- 当前 working tree = `097df77 修复影子显示问题` + Task 0 的 6 处 PROFILE_SCOPE 取消注释（GameObjectManager.cpp:74/111/127/152/176 + Scene.cpp:68/78），未提交。
- `Animator::Update()` 当前实现于 `PlantVsZombies/Reanimation/Animator.cpp:124-224`。
- `Animator.h` 类结束 `};` 在 line 444；`std::unordered_multimap<int, FrameEvent> mFrameEvents;` 在 line 68；`void Update();` 在 line 325。
- `GameObject.h` `virtual void Update();` 在 line 114。
- `AnimatedObject.h` `bool mAutoDestroy;` 在 line 28；`void Update() override;` 在 line 99。
- `AnimatedObject.cpp` Update() 在 line 60-78。
- `GameObjectManager.cpp` Update() 在 line 73-107；line 100-106 是裸 for `obj->Update()` 循环（外层是新加的 `PROFILE_SCOPE("2.GOM_Update(serial)")` 包整段）。

**Spec：** `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\superpowers\specs\2026-05-23-parallel-update-phase2-deferred-events-design.md`

---

### Task 1: 核实 child Animator 不跨父共享（GATE，只读代码）

**Files:**
- Read: `PlantVsZombies/Reanimation/Animator.h`（找 `AttachAnimator` 声明 + `mAttachedReanims` 容器类型）
- Read: `PlantVsZombies/Reanimation/Animator.cpp`（`AttachAnimator` 实现 + `DetachAnimator`、`DetachAllAnimators`）
- Read: 所有调用 `Animator::AttachAnimator` / `AttachAnimatorToTrack` 的位置（用 grep 找：`Animator->Attach`、`->AttachAnimator`）

- [ ] **Step 1: 列举所有 AttachAnimator caller**

用 Grep 工具搜 `AttachAnimator(` 与 `AttachAnimatorToTrack(`，列出所有 caller 文件:行。

- [ ] **Step 2: 逐 caller 核实 child 是否独有**

对每个 caller，回答：
- 传入的 child `std::shared_ptr<Animator>` 是 caller 自己 new 出来的（独有），还是从其他对象拿来的（可能共享）？
- 如果是从 `ResourceManager` / 全局缓存 / 单例拿，是 `Reanimation` 资源（OK，只读）还是 `Animator` 实例（FAIL，多父共享）？

**典型 PASS 形态（期望）：** caller 是 `Plant`/`Zombie`/子弹 等，每个对象 new 自己的 child Animator 用于附加部件（如僵尸的头/手）。

**典型 FAIL 形态（不期望）：** 多个父对象同一 `shared_ptr<Animator>` attach（同一 child 出现在两个 `mExtraInfos[].mAttachedReanims[]` 容器中）。

- [ ] **Step 3: 写下结论**

在本文件 Task 1 末尾追加：`结论：PASS`（child Animator 独有，没有跨父共享）或 `结论：FAIL —— <每处 cross-parent share 的位置>`。

- **PASS 处理：** 进 Task 2。
- **FAIL 处理：** 立即停止本 plan，回 brainstorming 修订 spec（需要 `mFrameEvents` 加 atomic 或 mutex，或彻底放弃 deferred 设计）。

- [ ] **Step 4: 用户检查点**

把结论贴给用户确认后再进 Task 2。无代码改动，无需构建/提交。

**结论：** _（执行 Task 1 时填写）_

---

### Task 2: 新增 DeferredEvent header + Animator::UpdateParallelDeferred（Update 不动）

**Files:**
- Create: `PlantVsZombies/Game/DeferredEvent.h`
- Modify: `PlantVsZombies/Reanimation/Animator.h`（include + 1 方法声明）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（1 方法实现）

- [ ] **Step 1: 创建 DeferredEvent.h**

新建文件 `PlantVsZombies/Game/DeferredEvent.h`：

```cpp
#pragma once
#ifndef _DEFERRED_EVENT_H
#define _DEFERRED_EVENT_H

#include <functional>

// 阶段二并行：worker 内推进 Animator 时遇到帧事件，把 callback 拷贝入此结构；
// 主线程串行 drain 时依次 invoke cb。
struct DeferredEvent {
    std::function<void()> cb;
};

#endif
```

- [ ] **Step 2: Animator.h 新增 include + 方法声明**

`Animator.h` 顶部 include 区（在 `#include <unordered_map>` 之后或随便相邻位置）加：

```cpp
#include "../Game/DeferredEvent.h"
#include <vector>
```

（若 `<vector>` 已被间接 include，保留无害。）

在 `Animator.h` 第 325 行 `void Update();` 之后插入：

```cpp
    /**
     * @brief 阶段二并行段：完整推进帧 + 计时器 + 子节点递归；遇到帧事件 = 拷贝 callback 入 outBuf。
     *        对象本地、worker 线程安全；不调用任何 callback。
     *        前置：本 Animator 与其子 Animator 树不被其他线程并发访问（Task 1 audit PASS）。
     */
    void UpdateParallelDeferred(std::vector<DeferredEvent>& outBuf);
```

- [ ] **Step 3: Animator.cpp 新增 UpdateParallelDeferred 实现**

在 `Animator.cpp` 第 224 行（`Update()` 的结束 `}`）之后插入：

```cpp
void Animator::UpdateParallelDeferred(std::vector<DeferredEvent>& outBuf) {
    if (!mIsPlaying || !mReanim) return;

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

    int oldInt = static_cast<int>(oldFrame);
    int newInt = static_cast<int>(mFrameIndexNow);

    if (newInt >= oldInt) {
        for (int f = oldInt + 1; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                outBuf.push_back({ it->second.callback });
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }
    else {
        int endInt = static_cast<int>(mFrameIndexEnd);
        for (int f = oldInt + 1; f <= endInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                outBuf.push_back({ it->second.callback });
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
                outBuf.push_back({ it->second.callback });
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }

    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        if (mReanimBlendCounter < 0) mReanimBlendCounter = 0;
    }

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->UpdateParallelDeferred(outBuf);
            }
        }
    }
}
```

**注意：** 不要修改 `Animator::Update()`（124-224）。它保持原样，作为串行 fallback 的逐字节一致路径。新方法此时还没有调用方。

- [ ] **Step 4: 用户构建验证**

用户在 VS F7 构建。预期：编译通过，无新增警告/错误。运行正常关卡：行为与改前完全一致（新方法尚无调用方，纯增量）。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段二 step1：新增 DeferredEvent + Animator::UpdateParallelDeferred（Update 不变）`。assistant 不自动提交。

---

### Task 3: GameObject::UpdateParallel virtual + AnimatedObject 改造（并行路径尚未接通）

**Files:**
- Modify: `PlantVsZombies/Game/GameObject.h:114`（include + 新增虚函数默认空实现）
- Modify: `PlantVsZombies/Game/AnimatedObject.h`（新增成员 + override 声明）
- Modify: `PlantVsZombies/Game/AnimatedObject.cpp`（新增 `UpdateParallel`，改 `Update` 内一行）

- [ ] **Step 1: GameObject.h 加 include + 新增虚函数**

在 `GameObject.h` 顶部 include 区（紧贴 `#include <algorithm>` 之后）加：

```cpp
#include "DeferredEvent.h"
#include <vector>
```

在 `GameObject.h` 第 114 行 `virtual void Update();` 之后插入：

```cpp
    // 阶段二并行：默认空。约定——只做对象本地工作，worker 线程安全；
    //               遇到 deferred 操作 push 到 outBuf，主线程串行回放。
    virtual void UpdateParallel(std::vector<DeferredEvent>& outBuf) {}
```

- [ ] **Step 2: AnimatedObject.h 新增成员与 override**

在 `AnimatedObject.h` 第 28 行 `bool mAutoDestroy;` 之后插入：

```cpp
    bool mAdvancedInParallel = false;   // 阶段二：本帧 animator 是否已在并行段推进过
```

在 `AnimatedObject.h` 第 99 行 `void Update() override;` 之后插入：

```cpp
    void UpdateParallel(std::vector<DeferredEvent>& outBuf) override;
```

- [ ] **Step 3: AnimatedObject.cpp 新增 UpdateParallel + 改 Update 一行**

`AnimatedObject.cpp` 当前 `Update()`（line 60-78）：

```cpp
void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		mAnimator->Update();

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
			if (mAutoDestroy) {
				GameObjectManager::GetInstance().DestroyGameObject(this);
			}
			else {
				mIsPlaying = false;
			}
		}
	}

	UpdateGlowingEffect();
}
```

替换为（仅 `mAnimator->Update();` 那一行变条件，并在函数前新增 `UpdateParallel`）：

```cpp
void AnimatedObject::UpdateParallel(std::vector<DeferredEvent>& outBuf) {
	if (mAnimator) mAnimator->UpdateParallelDeferred(outBuf);
	mAdvancedInParallel = true;
}

void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		if (mAdvancedInParallel) { mAdvancedInParallel = false; /* events 在 phase B drain 已处理；Animator 状态已就位 */ }
		else                     { mAnimator->Update(); }

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
			if (mAutoDestroy) {
				GameObjectManager::GetInstance().DestroyGameObject(this);
			}
			else {
				mIsPlaying = false;
			}
		}
	}

	UpdateGlowingEffect();
}
```

（`AnimatedObject.cpp` 已 `#include` 链路含 `Animator.h`，DeferredEvent 通过 Animator.h 的 include 链可见，无需新增 include。）

- [ ] **Step 4: 用户构建验证**

用户 VS F7 构建。预期：编译通过。运行正常关卡：行为与改前完全一致——`mAdvancedInParallel` 恒为 `false`（无人调 `UpdateParallel`），`Update()` 走原 `mAnimator->Update()` 路径。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段二 step2：新增 UpdateParallel 两阶段 API（并行路径未接通）`。

---

### Task 4: 临时一致性断言脚手架（Animator 快照/还原/比对）

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（新增临时 struct + 3 个方法声明）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（新增 3 个方法实现 + `#include <cmath>`）

> 本任务全部为**临时**代码，Task 7 在 GATE KEEP 后整体删除。

- [ ] **Step 1: Animator.h 新增临时快照结构与方法声明**

在 `Animator.h` 第 444 行（类结束 `};`）之前插入：

```cpp
public:
    // ===== 临时（阶段二验证脚手架，GATE KEEP 后随 Task 7 删除）=====
    struct AdvanceSnapshot {
        float fNow, fBegin, fEnd, speed, origSpeed, blendCtr, blendCtrMax;
        int   blendBuf;
        bool  playing;
        PlayState pstate;
        std::string targetTrack, curTrack;
        std::unordered_multimap<int, FrameEvent> frameEventsCopy;   // 深拷贝，用于 Restore；不参与 Equals
        std::vector<AdvanceSnapshot> children;
    };
    void SnapshotAdvanceState(AdvanceSnapshot& s) const;
    void RestoreAdvanceState(const AdvanceSnapshot& s);
    static bool EqualsAdvanceState(const AdvanceSnapshot& a,
                                   const AdvanceSnapshot& b,
                                   std::string& diffOut);
    // ===== 临时结束 =====
```

- [ ] **Step 2: Animator.cpp 顶部加 cmath include**

在 `Animator.cpp` 顶部 include 区（line 1-6）加：

```cpp
#include <cmath>
```

（若 `<cmath>` 已被间接 include 不影响。）

- [ ] **Step 3: Animator.cpp 新增三方法实现**

在 `Animator.cpp` 文件末尾（最后一个 `}` 之后）插入：

```cpp
// ===== 临时（阶段二验证脚手架，GATE KEEP 后随 Task 7 删除）=====
void Animator::SnapshotAdvanceState(AdvanceSnapshot& s) const {
    s.fNow = mFrameIndexNow;   s.fBegin = mFrameIndexBegin;   s.fEnd = mFrameIndexEnd;
    s.speed = mSpeed;          s.origSpeed = mOriginalSpeed;
    s.blendCtr = mReanimBlendCounter; s.blendCtrMax = mReanimBlendCounterMax;
    s.blendBuf = mFrameIndexBlendBuffer;
    s.playing = mIsPlaying;
    s.pstate = mPlayingState;
    s.targetTrack = mTargetTrack; s.curTrack = mCurrentTrackName;
    s.frameEventsCopy = mFrameEvents;   // 深拷贝 multimap（含 std::function），用于 Restore 完整还原
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
    mIsPlaying = s.playing;
    mPlayingState = s.pstate;
    mTargetTrack = s.targetTrack; mCurrentTrackName = s.curTrack;
    mFrameEvents = s.frameEventsCopy;   // 整个还原（含已被并行 erase 的 events）
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
        a.blendBuf!=b.blendBuf ||
        a.playing!=b.playing ||
        a.pstate!=b.pstate || a.targetTrack!=b.targetTrack || a.curTrack!=b.curTrack ||
        a.children.size()!=b.children.size()) {
        // 注意：frameEventsCopy 不参与 Equals —— 它是辅助还原字段，
        //       multimap 顺序不稳定且 std::function 无 operator==。
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

**关键 vs phase-1**：减 4 字段（`evtOld/evtNew/evtWrapped/evtPending`，本次无延后回放窗口），加 1 字段 `frameEventsCopy`（深拷贝 mFrameEvents 用于 Restore；与 phase-1 不同，本次 `UpdateParallelDeferred` 会 erase 非 persistent events，oracle 跑两次必须能完整还原 mFrameEvents 才能保证第二次跑产生一致行为）。

- [ ] **Step 4: 用户构建验证**

用户 VS F7 构建。预期：编译通过。运行正常关卡：行为不变（这些方法仍无调用方）。

- [ ] **Step 5: 用户提交检查点（用户驱动）**

提交建议信息：`阶段二 step3：临时一致性断言脚手架（Snapshot/Restore/Equals）`。

---

### Task 5: 接通 GameObjectManager 阶段 A/B + 阈值 + 验证模式

**Files:**
- Modify: `PlantVsZombies/Game/GameObjectManager.h`（新增成员 + include）
- Modify: `PlantVsZombies/Game/GameObjectManager.cpp:100-106`（裸 for 替换为阶段 A/B）

- [ ] **Step 1: GameObjectManager.h 新增成员 + include**

在 `GameObjectManager.h` 顶部 include 区加：

```cpp
#include "DeferredEvent.h"
```

在 `GameObjectManager` 类内（与现有 `mThreadPool` 同区域）加：

```cpp
private:
    std::vector<std::vector<DeferredEvent>> mDeferredEventBuffers;  // size = numWorkers，跨帧 capacity 复用
```

- [ ] **Step 2: GameObjectManager.cpp 顶部加 include**

在 `GameObjectManager.cpp` 顶部 `#include "../Profiler.h"` 之后加：

```cpp
#include "AnimatedObject.h"
#include <cstdio>
```

（`AnimatedObject.h` 已 `#include "../Reanimation/Animator.h"`，`Animator::AdvanceSnapshot` 随之可见。）

- [ ] **Step 3: 替换 Update() 内的 obj->Update() 循环**

`GameObjectManager.cpp` 当前 Update() 内（line 100-106）：

```cpp
	// 现有对象
	for (size_t i = 0; i < mGameObjects.size(); i++) {
		auto obj = mGameObjects[i].get();
		if (obj->IsActive()) {
			obj->Update();
		}
	}
```

替换为：

```cpp
	// 现有对象
	{
		PROFILE_SCOPE("2b.GOM_objUpdateLoop(serial)");
		const int total = static_cast<int>(mGameObjects.size());
		constexpr int kParallelUpdateThreshold = 200;            // 与并行 Draw 阈值同量级；GATE 可调
		static constexpr bool kVerifyParallelAdvance = false;    // 临时：串/并行一致性断言开关，GATE 用时本地置 true

		if (mThreadPool && total >= kParallelUpdateThreshold) {
			int numWorkers = static_cast<int>(std::thread::hardware_concurrency());
			if (numWorkers < 1) numWorkers = 1;
			if (numWorkers > total) numWorkers = total;

			if (static_cast<int>(mDeferredEventBuffers.size()) < numWorkers)
				mDeferredEventBuffers.resize(numWorkers);
			for (auto& buf : mDeferredEventBuffers) buf.clear();   // 保 capacity

			if (kVerifyParallelAdvance) {
				// —— 验证模式：snapshot → 并行(outBuf_par) → snapshot par → restore → 串行(outBuf_ser) → snapshot ser → 比对；生效态=串行结果 ——
				static std::vector<Animator::AdvanceSnapshot> snaps;
				static std::vector<Animator::AdvanceSnapshot> parSig;
				static std::vector<std::vector<DeferredEvent>> outBufsSer;
				snaps.clear();   snaps.resize(total);
				parSig.clear();  parSig.resize(total);
				outBufsSer.clear(); outBufsSer.resize(numWorkers);
				for (auto& buf : outBufsSer) buf.clear();

				auto animOf = [this](int i) -> Animator* {
					auto* ao = dynamic_cast<AnimatedObject*>(mGameObjects[i].get());
					return ao ? ao->GetAnimatorInternal().get() : nullptr;
				};

				// 1) snapshot 起点
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->SnapshotAdvanceState(snaps[i]); }
				}
				// 2) 并行跑
				mThreadPool->Dispatch(total, [this, total, numWorkers](int s, int e) {
					const int chunkSize = (total + numWorkers - 1) / numWorkers;
					const int slot = (chunkSize > 0) ? (s / chunkSize) : 0;
					auto& outBuf = mDeferredEventBuffers[slot];
					for (int i = s; i < e; ++i) {
						auto* o = mGameObjects[i].get();
						if (o->IsActive()) o->UpdateParallel(outBuf);
					}
				});
				// 3) snapshot 并行结果
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->SnapshotAdvanceState(parSig[i]); }
				}
				// 4) restore 起点
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) { if (auto* a = animOf(i)) a->RestoreAdvanceState(snaps[i]); }
				}
				// 5) 串行参考（生效态）—— 主线程单线程跑同款 UpdateParallel
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->UpdateParallel(outBufsSer[0]);   // 串行参考都 push 到 slot 0
				}
				// 6) 比对每个对象状态
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
				// 7) 比对 event 总数（slot 间顺序无所谓，只比总和）
				size_t totalPar = 0, totalSer = 0;
				for (auto& buf : mDeferredEventBuffers) totalPar += buf.size();
				for (auto& buf : outBufsSer) totalSer += buf.size();
				if (totalPar != totalSer) {
					std::printf("[VERIFY] event total mismatch par=%zu ser=%zu\n", totalPar, totalSer);
				}
				// 验证模式下：用串行的 outBufsSer[0] 替换 mDeferredEventBuffers，避免事件被回放两次
				mDeferredEventBuffers.swap(outBufsSer);
			} else {
				// —— 阶段 A：并行推进（仅 animator 帧推进 + 事件入队，对象本地）——
				mThreadPool->Dispatch(total, [this, total, numWorkers](int start, int end) {
					const int chunkSize = (total + numWorkers - 1) / numWorkers;
					const int slot = (chunkSize > 0) ? (start / chunkSize) : 0;
					auto& outBuf = mDeferredEventBuffers[slot];
					for (int i = start; i < end; ++i) {
						auto* obj = mGameObjects[i].get();
						if (obj->IsActive()) obj->UpdateParallel(outBuf);
					}
				});
			}

			// —— 阶段 B-1：主线程 drain deferred event buffers（slot 顺序）——
			{
				PROFILE_SCOPE("2c.PhaseB_DrainEvents");
				for (auto& buf : mDeferredEventBuffers) {
					for (auto& evt : buf) evt.cb();
				}
			}

			// —— 阶段 B-2：串行（mGameObjects 序），其余 Update 全部在此 ——
			{
				PROFILE_SCOPE("2d.PhaseB_serialUpdate");
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->Update();
				}
			}
		} else {
			// —— 串行 fallback：与原代码逐字节一致 ——
			for (size_t i = 0; i < mGameObjects.size(); i++) {
				auto obj = mGameObjects[i].get();
				if (obj->IsActive()) {
					obj->Update();
				}
			}
		}
	}
```

- [ ] **Step 4: 用户构建验证（验证模式关）**

用户 VS F7 构建（`kVerifyParallelAdvance=false`）。运行：
- **正常关卡**（< 200 对象）走 fallback：行为与改前完全一致
- **11000 僵尸压测**（≥ 200 对象）走并行：动画/掉头掉臂/子弹方向/音效/`-Debug` 碰撞框均正常，无崩溃

⚠️ 不要立刻跑 GATE 实测——先冒烟确认正确性。

- [ ] **Step 5: 用户构建验证（验证模式开）**

用户把 `GameObjectManager.cpp` 内 `kVerifyParallelAdvance` 改为 `true`，F7 构建，跑 **11000 僵尸场景 30–60 秒并做交互**（种植/僵尸啃食/掉臂/CherryBomb 爆炸）。预期：调试控制台**零** `[VERIFY] 串/并行发散` 行 **且**零 `[VERIFY] event total mismatch` 行。若出现：
- 记录 obj 名/diff 内容，停止并按 systematic-debugging 定位
- 多半是 Task 1 audit 漏掉的某个 child Animator 跨父共享 → mFrameEvents 在 worker 间竞争
- 不要继续 GATE

验证通过后把 `kVerifyParallelAdvance` 改回 `false`。

- [ ] **Step 6: 用户提交检查点（用户驱动）**

提交建议信息：`阶段二 step4：接通并行阶段A/B + drain + 临时一致性断言（已验证零发散）`。

---

### Task 6: 测量 GATE（用户实测，决策点）

**Files:** 无代码改动。

- [ ] **Step 1: A 基线（强制串行 fallback）**

`GameObjectManager.cpp` 把 `kParallelUpdateThreshold` 临时改为 `999999`（强制走串行 fallback —— 等价基线，逐字节一致）。F7 构建，预热后稳态 11000 僵尸场景，记录整块 `FRAME PROFILE`（重点 `2.GOM_Update / 2b.GOM_objUpdateLoop / total/frame`）。然后把阈值改回 `200`。

- [ ] **Step 2: B 阶段二（并行开）**

同一 session、同 build 配置，阈值 `200`（并行生效，`kVerifyParallelAdvance=false`）。同样预热后稳态 11000 僵尸场景，记录整块 `FRAME PROFILE`（重点 `2.GOM_Update / 2b / 2c.PhaseB_DrainEvents / 2d.PhaseB_serialUpdate / total/frame`）。同时肉眼 + `-Debug` 复核视觉/音频/碰撞框无回归。

- [ ] **Step 3: 把数据贴给 assistant 决策**

按 spec §7.2 决策规则：
- **KEEP**：`total/frame` 改善 ≥ 1ms **且** Task 5 Step 5 零发散 **且** 无视觉/玩法/音频回归 → 进 Task 7。
- **ITERATE**：0–1ms 改善 / 无回归 → 先加诊断埋点定位"隐藏成本"（drain 时间 / `std::function` copy 触发 heap alloc）后重测 Step 2；不进 Task 7。
- **REVERT**：无改善 / 发散 / 回归 → `git restore` 受影响 5-6 文件，回 Task 1 起点 commit，记录结论到 [[pvz-perf-optimization]] 内存，结束本 plan。

---

### Task 7: GATE KEEP 后清理临时脚手架（条件性）

**前置：** 仅当 Task 6 决策 = KEEP 才执行。

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（删 Task 4 Step 1 的 `AdvanceSnapshot`/3 方法声明块）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（删 Task 4 Step 3 的"临时"实现块；若 `<cmath>` 无他用则删）
- Modify: `PlantVsZombies/Game/GameObjectManager.cpp`（删 `kVerifyParallelAdvance` 整个 `if (kVerifyParallelAdvance) {...} else` 验证分支，只留阶段 A 的 `Dispatch`）

- [ ] **Step 1: 删除 Animator 临时块**

删掉 `Animator.h` 中 `// ===== 临时（阶段二验证脚手架...` 到 `// ===== 临时结束 =====` 整块（含 3 方法声明 + `AdvanceSnapshot`）。删掉 `Animator.cpp` 中对应的 `// ===== 临时（阶段二...` 到 `// ===== 临时结束 =====` 实现块。

检查 `Animator.cpp` 里 `<cmath>` 是否仅被临时 oracle 用：grep `std::abs` / `std::sqrt` 等 cmath 函数；若全部在临时块内，删 `#include <cmath>`；若有其他用途，保留。

- [ ] **Step 2: 精简 GameObjectManager 验证分支**

把 Task 5 Step 3 的整段中 `if (mThreadPool && total >= kParallelUpdateThreshold) { ... }` 内层简化为只剩阶段 A：

```cpp
		if (mThreadPool && total >= kParallelUpdateThreshold) {
			int numWorkers = static_cast<int>(std::thread::hardware_concurrency());
			if (numWorkers < 1) numWorkers = 1;
			if (numWorkers > total) numWorkers = total;

			if (static_cast<int>(mDeferredEventBuffers.size()) < numWorkers)
				mDeferredEventBuffers.resize(numWorkers);
			for (auto& buf : mDeferredEventBuffers) buf.clear();

			// 阶段 A：并行推进（仅 animator 帧推进 + 事件入队，对象本地）
			mThreadPool->Dispatch(total, [this, total, numWorkers](int start, int end) {
				const int chunkSize = (total + numWorkers - 1) / numWorkers;
				const int slot = (chunkSize > 0) ? (start / chunkSize) : 0;
				auto& outBuf = mDeferredEventBuffers[slot];
				for (int i = start; i < end; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->UpdateParallel(outBuf);
				}
			});

			// 阶段 B-1：主线程 drain deferred event buffers
			{
				PROFILE_SCOPE("2c.PhaseB_DrainEvents");
				for (auto& buf : mDeferredEventBuffers) {
					for (auto& evt : buf) evt.cb();
				}
			}

			// 阶段 B-2：串行（mGameObjects 序），其余 Update 全部在此
			{
				PROFILE_SCOPE("2d.PhaseB_serialUpdate");
				for (int i = 0; i < total; ++i) {
					auto* obj = mGameObjects[i].get();
					if (obj->IsActive()) obj->Update();
				}
			}
		} else {
			// 串行 fallback：与原代码逐字节一致
			for (size_t i = 0; i < mGameObjects.size(); i++) {
				auto obj = mGameObjects[i].get();
				if (obj->IsActive()) {
					obj->Update();
				}
			}
		}
```

删掉 `static constexpr bool kVerifyParallelAdvance` 那一行。保留 `#include "AnimatedObject.h"`（虚调用无需它，但保留无害；想最简可在确认无依赖后删）。

> 保留项（永久）：`DeferredEvent` struct、`Animator::UpdateParallelDeferred`、`GameObject::UpdateParallel`、`AnimatedObject::UpdateParallel/mAdvancedInParallel`、`GameObjectManager::mDeferredEventBuffers`、阶段 A/B 派发与阈值 fallback。粗粒度 `Profiler` 埋点 `2b/2c/2d` 保留至整个优化收尾再随 `Profiler.h` 移除（不在本 plan 范围）。

- [ ] **Step 3: 用户构建验证**

用户 F7 构建并重跑 11000 僵尸场景：性能与 Task 6 B 测一致（容差 ±0.2ms），视觉/音频无回归，临时验证代码已无残留（全局搜索 `kVerifyParallelAdvance` / `AdvanceSnapshot` 应为零）。

- [ ] **Step 4: 用户提交检查点（用户驱动）**

提交建议信息：`阶段二完成：deferred-events 并行 Animator（清理验证脚手架，GATE 已 KEEP）`。更新 [[pvz-perf-optimization]] 内存记录本阶段结果与下一阶段指向（GameObject components 2.93ms / 算法削减 / 等）。

---

## Self-Review

**Spec 覆盖核对（对照 2026-05-23-parallel-update-phase2-deferred-events-design.md）：**
- §3 架构（GOM A/B + Animator UpdateParallelDeferred + drain）→ Task 2/3/5。✓
- §4 接口（DeferredEvent struct / UpdateParallelDeferred / UpdateParallel virtual / mDeferredEventBuffers）→ Task 2/3/5。✓
- §5.1 并发安全前提（mFrameEvents per-instance / child 不跨父）→ Task 1 audit GATE。✓
- §5.2 已自审安全点 → Task 1 audit 之前可信。✓
- §6 一致性 oracle（snapshot → 并行 → restore → 串行 → 比对 + outBuf.size 对比）→ Task 4 + Task 5。✓
- §7 GATE 决策规则 → Task 6。✓
- §8 永久 vs 临时 → Task 7 + Task 7 末保留项说明。✓
- §9 风险（cross-parent shared child / function copy heap alloc）→ Task 1 显式 GATE + Task 6 ITERATE 路径预留诊断。✓

**Placeholder 扫描：** 无 TBD/TODO/"类似 TaskN"/"加适当错误处理"；每个代码步给出完整代码。✓

**类型/签名一致性：** `DeferredEvent.cb`/`UpdateParallelDeferred(std::vector<DeferredEvent>&)`/`UpdateParallel(std::vector<DeferredEvent>&)`/`mAdvancedInParallel`/`mDeferredEventBuffers`/`AdvanceSnapshot`/`SnapshotAdvanceState`/`RestoreAdvanceState`/`EqualsAdvanceState`/`GetAnimatorInternal()`（AnimatedObject.h:97 已有）/`GetName()`（GameObject.h:153 已有）/`ThreadPool::Dispatch(int,std::function<void(int,int)>)`（ThreadPool.h:32 已有）—— 全任务一致，均对应已存在或本 plan 定义的符号。✓
