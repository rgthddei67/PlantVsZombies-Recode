# Animator 速度系统重构（clip 层替代 mOriginalSpeed）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用一个"绝对覆盖、0=回落 base"的 `mClipSpeed` 层取代 `mOriginalSpeed` 的存档-还原机制，删除全局 `RestoreSpeed` 两步舞，保持游戏手感完全不变。

**Architecture:** Animator 的有效播放速度从原来的 `mSpeed * mExtraSpeedMultiplier`（其中 `mSpeed` 被 PlayTrack 反复覆盖+靠 `mOriginalSpeed` 找回）改为：

```
effective = (mClipSpeed != 0 ? mClipSpeed : mSpeed) * mExtraSpeedMultiplier
```

- `mSpeed`（base）：每实例随机的基础走速。**PlayTrack 永不再写它**；只有 `SetSpeed`/`SetAnimationSpeed` 写（init、Polevaulter 跳完重设）。
- `mClipSpeed`（片段绝对覆盖）：每次 `PlayTrack` 覆盖；传 `speed>0` → `mClipSpeed=speed`（绝对）；传 `speed==0`（默认）→ `mClipSpeed=0`（回落 base）。随轨道作用域，切轨道自动失效。**会递归到附加子动画**，复刻旧 `SetSpeed` 递归。
- `mExtraSpeedMultiplier`（状态层）：减速/冻结，跨 PlayTrack 存活。**本次重构完全不动它**。

**Tech Stack:** C++17 / Visual Studio 2026 / 自研 Reanimation 动画系统 / nlohmann::json 存档。

**重要约束（来自 CLAUDE.md 与会话）：**
- 本仓库**无自动化测试框架**。验证 = 主人在 VS 里 F7 编译 + F5 运行，按下文场景矩阵肉眼确认。
- **禁止**用 MSVC-Debug-MCP 的 `build_*` 工具编译；编译由主人手动在 VS 完成。
- 本次改动是**原子重构**：删除 Animator/AnimatedObject 的 `RestoreSpeed` 会让所有调用点编译失败，所以 Task 1–8 必须**全部落地后**才能编译。计划在 Task 9 一次性编译+验证，Task 10 一次性提交（不提交中间不可编译态，保护 bisect）。
- ⚠️ **PaperZombie 主人正在手动改**：Task 8 不锁定行号，执行前**必须重新读取**并就地协调。

---

## File Structure

| 文件 | 职责 | 本次改动 |
|---|---|---|
| `PlantVsZombies/Reanimation/Animator.h` | 动画器声明 | 删 `mOriginalSpeed`/`SetOriginalSpeed`/`GetOriginalSpeed`/`RestoreSpeed`；加 `mClipSpeed`/`GetClipSpeed`/`SetClipSpeed`/内联 `EffectiveSpeed` |
| `PlantVsZombies/Reanimation/Animator.cpp` | 动画器实现 | 重写 `PlayTrack`；两处 `frameAdvance` 用 `EffectiveSpeed`；两处 `PLAY_ONCE_TO` 删 `RestoreSpeed`；`GetTrackVelocity` 用 `EffectiveSpeed`；实现 `SetClipSpeed`（递归） |
| `PlantVsZombies/Game/AnimatedObject.h` | 包装层声明 | 删 `SetOriginalSpeed`/`GetOriginalSpeed`/`RestoreSpeed`；加 `GetClipSpeed`/`SetClipSpeed` |
| `PlantVsZombies/Game/AnimatedObject.cpp` | 包装层实现 | 同上对应实现替换 |
| `PlantVsZombies/Game/Zombie/Zombie.cpp` | 普通僵尸 | 删 2 处 `RestoreSpeed`（StopEat、ValidateEatingState） |
| `PlantVsZombies/Game/Zombie/Polevaulter.cpp` | 撑杆僵尸 | 删 2 处 `RestoreSpeed`；`EndJump` 重设 base 改用 `SetAnimationSpeed` |
| `PlantVsZombies/Game/Plant/WallNut.cpp` | 坚果墙 | 啃食暂停从 `SetOriginalSpeed/SetSpeed(0)/RestoreSpeed` 改为 `SetExtraSpeedMultiplier(0)/(1)` |
| `PlantVsZombies/GameInfoSaver.cpp` | 存档 | `animOriginalSpeed` 字段改为 `animClipSpeed`（存+读） |
| `PlantVsZombies/Game/Zombie/PaperZombie.cpp` | 读报僵尸（**主人在改**） | 删其中的 `RestoreSpeed`（行号执行时再定位） |

---

## Task 1: Animator.h — 改三层速度声明

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h:41-42`（加 `mClipSpeed`）
- Modify: `PlantVsZombies/Reanimation/Animator.h:64`（删 `mOriginalSpeed`）
- Modify: `PlantVsZombies/Reanimation/Animator.h:147-160`（删原始速度三件套，加 clip 访问器）
- Modify: `PlantVsZombies/Reanimation/Animator.h:247-252`（`SetSpeed`/`GetSpeed` 附近加 `SetClipSpeed`/`GetClipSpeed`/`EffectiveSpeed`）

- [ ] **Step 1: 在成员变量区加 `mClipSpeed`**

把（`Animator.h:41-42`）：

```cpp
	float mSpeed = 1.0f;                      ///< 播放速度倍率
	float mExtraSpeedMultiplier = 1.0f;       ///< 额外速度倍率 (独立于 mSpeed，与 PlayTrack/SetSpeed 正交，用于减速等状态效果)
```

改成：

```cpp
	float mSpeed = 1.0f;                      ///< 基础播放速度 (每实例随机的 base，PlayTrack 不再覆盖它)
	float mClipSpeed = 0.0f;                  ///< 当前轨道的绝对速度覆盖；0 = 回落到 mSpeed。每次 PlayTrack 重设，随轨道作用域
	float mExtraSpeedMultiplier = 1.0f;       ///< 额外速度倍率 (正交状态层，减速/冻结，跨 PlayTrack 存活)
```

- [ ] **Step 2: 删除 `mOriginalSpeed` 成员**

删除（`Animator.h:62-64` 附近，注意保留上方的 `mTargetTrack`）：

```cpp
	// 过渡目标
	std::string mTargetTrack = "";              ///< 播放一次后要切换到的轨道名
	float mOriginalSpeed = 1.0f;                ///< 原始速度 (用于恢复)
```

改成：

```cpp
	// 过渡目标
	std::string mTargetTrack = "";              ///< 播放一次后要切换到的轨道名
```

- [ ] **Step 3: 删除原始速度三件套（`SetOriginalSpeed`/`GetOriginalSpeed`/`RestoreSpeed`）**

删除（`Animator.h:147-161` 整段）：

```cpp
	/**
	 * @brief 设置原始速度 (用于恢复)
	 */
	void SetOriginalSpeed(float speed) { this->mOriginalSpeed = speed; }

	/**
	 * @brief 获取原始速度
	 */
	float GetOriginalSpeed() { return this->mOriginalSpeed; }

	/**
	 * @brief 恢复到原始速度
	 */
	void RestoreSpeed() { SetSpeed(mOriginalSpeed); }

```

（整段删掉，不留空壳。）

- [ ] **Step 4: 在 `SetSpeed`/`GetSpeed` 后加 clip 访问器与 `EffectiveSpeed`**

在（`Animator.h:252` 的 `float GetSpeed() const { return mSpeed; }` 之后）插入：

```cpp
	/**
	 * @brief 设置当前轨道的绝对速度覆盖 (0 = 回落到基础速度 mSpeed)。
	 *        会递归到所有附加子动画，复刻旧 SetSpeed 的递归语义。
	 */
	void SetClipSpeed(float clipSpeed);

	/**
	 * @brief 获取当前轨道速度覆盖值 (0 表示正使用基础速度)
	 */
	float GetClipSpeed() const { return mClipSpeed; }

	/**
	 * @brief 实际生效的播放速度 = (clip 覆盖优先, 否则 base) * 状态倍率
	 */
	float EffectiveSpeed() const {
		return (mClipSpeed != 0.0f ? mClipSpeed : mSpeed) * mExtraSpeedMultiplier;
	}
```

- [ ] **Step 5: 更新 `PlayTrack`/`PlayTrackOnce` 的 speed 参数文档注释（可选但建议）**

把 `Animator.h:128` 和 `:138` 两处注释里的：

```
	 * @param speed 播放速度倍率，0.0表示保持当前速度不变，>0时自动保存原始速度
```

改成：

```
	 * @param speed 轨道绝对播放速度，0.0=回落到基础速度(base)，>0=本轨道固定用该速度
```

---

## Task 2: Animator.cpp — 重写速度逻辑

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:88-91`（PlayTrack 写 clip 而非 mSpeed）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:133`（Update frameAdvance）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:151-152`（PLAY_ONCE_TO 删 RestoreSpeed）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:237`（UpdateParallelDeferred frameAdvance）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:254-255`（并行版 PLAY_ONCE_TO 删 RestoreSpeed）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:557-568`（SetSpeed 后新增 SetClipSpeed 实现）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:795`（GetTrackVelocity）

- [ ] **Step 1: PlayTrack 改为写 clip，不再碰 mSpeed/mOriginalSpeed**

把（`Animator.cpp:88-91`）：

```cpp
	if (speed > 0.0f) {
		mOriginalSpeed = mSpeed;
		SetSpeed(speed);
	}
```

改成：

```cpp
	// speed>0 → 本轨道绝对速度覆盖；speed==0 → 清除覆盖，回落到基础速度 mSpeed。
	// 递归到子动画，确保附加配件(铁桶/路障/报纸/头部)跟随同样的轨道速度。
	SetClipSpeed(speed > 0.0f ? speed : 0.0f);
```

- [ ] **Step 2: Update 的 frameAdvance 用 EffectiveSpeed**

把（`Animator.cpp:132-133`）：

```cpp
	// 帧索引前进 (mExtraSpeedMultiplier 让状态效果如减速可独立于 mSpeed)
	float frameAdvance = deltaTime * mFPS * mSpeed * mExtraSpeedMultiplier;
```

改成：

```cpp
	// 帧索引前进：clip 覆盖优先于 base，再乘状态层(减速)
	float frameAdvance = deltaTime * mFPS * EffectiveSpeed();
```

- [ ] **Step 3: PLAY_ONCE_TO 删 RestoreSpeed（Update 版）**

把（`Animator.cpp:150-154`）：

```cpp
			if (!mTargetTrack.empty()) {
				PlayTrack(mTargetTrack, 0.0f, 0.5f);
				RestoreSpeed();
				mTargetTrack = "";
			}
```

改成：

```cpp
			if (!mTargetTrack.empty()) {
				PlayTrack(mTargetTrack, 0.0f, 0.5f);   // speed=0 → clip 清零，回落 base
				mTargetTrack = "";
			}
```

- [ ] **Step 4: UpdateParallelDeferred 的 frameAdvance 用 EffectiveSpeed**

把（`Animator.cpp:237`）：

```cpp
	float frameAdvance = deltaTime * mFPS * mSpeed * mExtraSpeedMultiplier;
```

改成：

```cpp
	float frameAdvance = deltaTime * mFPS * EffectiveSpeed();
```

- [ ] **Step 5: PLAY_ONCE_TO 删 RestoreSpeed（并行版）**

把（`Animator.cpp:253-257`）：

```cpp
			if (!mTargetTrack.empty()) {
				PlayTrack(mTargetTrack, 0.0f, 0.5f);
				RestoreSpeed();
				mTargetTrack = "";
			}
```

改成：

```cpp
			if (!mTargetTrack.empty()) {
				PlayTrack(mTargetTrack, 0.0f, 0.5f);   // speed=0 → clip 清零，回落 base
				mTargetTrack = "";
			}
```

> ⚠️ 注意：`PlayTrack` 在 `UpdateParallelDeferred`（worker 线程）里被调用——但这是**既有行为**（旧代码此处也调 `PlayTrack`+`RestoreSpeed`）。`SetClipSpeed` 只写本对象树成员、不触发 callback，与并行段约束一致（见 Animator.h 对该方法的注释）。本次未引入新的线程风险。

- [ ] **Step 6: 在 SetSpeed 之后实现 SetClipSpeed（递归）**

在（`Animator.cpp:568` 的 `SetSpeed` 函数结束 `}` 之后）插入：

```cpp
void Animator::SetClipSpeed(float clipSpeed) {
	this->mClipSpeed = clipSpeed;

	// 递归到附加子动画，复刻旧 SetSpeed 的传播语义：
	// 父轨道切到 eat(2.1)/walk(回落) 时，附加配件同步同样的速度
	for (size_t i = 0; i < mExtraInfos.size(); i++) {
		for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
			auto child = mExtraInfos[i].mAttachedReanims[j].lock();
			if (child) {
				child->SetClipSpeed(clipSpeed);
			}
		}
	}
}
```

- [ ] **Step 7: GetTrackVelocity 用 EffectiveSpeed**

把（`Animator.cpp:795`）：

```cpp
	float velocity = dx * mSpeed * mExtraSpeedMultiplier;
```

改成：

```cpp
	float velocity = dx * EffectiveSpeed();
```

---

## Task 3: AnimatedObject 包装层

**Files:**
- Modify: `PlantVsZombies/Game/AnimatedObject.h:61-62`（删原始速度声明）
- Modify: `PlantVsZombies/Game/AnimatedObject.h:82`（删 RestoreSpeed 声明）
- Modify: `PlantVsZombies/Game/AnimatedObject.cpp:244-267`（删三实现，加 clip 包装）

- [ ] **Step 1: 头文件删原始速度声明，加 clip 包装声明**

把（`AnimatedObject.h:61-62`）：

```cpp
	void SetOriginalSpeed(float speed);
	float GetOriginalSpeed();
```

改成：

```cpp
	void SetClipSpeed(float speed);
	float GetClipSpeed() const;
```

- [ ] **Step 2: 头文件删 RestoreSpeed 声明**

删除（`AnimatedObject.h:82`）：

```cpp
	void RestoreSpeed();
```

（连同上一行 `PlayTrackOnce(...)` 之后的这一行删掉即可。）

- [ ] **Step 3: cpp 替换三个实现**

把（`AnimatedObject.cpp:244-267` 整段，从 `GetOriginalSpeed` 到 `RestoreSpeed` 结束）：

```cpp
float AnimatedObject::GetOriginalSpeed()
{
	if (mAnimator)
	{
		return mAnimator->GetOriginalSpeed();
	}
	return 0.0f;
}

void AnimatedObject::SetOriginalSpeed(float speed)
{
	if (mAnimator)
	{
		mAnimator->SetOriginalSpeed(speed);
	}
}

void AnimatedObject::RestoreSpeed()
{
	if (mAnimator)
	{
		mAnimator->RestoreSpeed();
	}
}
```

改成：

```cpp
float AnimatedObject::GetClipSpeed() const
{
	return mAnimator ? mAnimator->GetClipSpeed() : 0.0f;
}

void AnimatedObject::SetClipSpeed(float speed)
{
	if (mAnimator)
	{
		mAnimator->SetClipSpeed(speed);
	}
}
```

---

## Task 4: Zombie.cpp — 删 2 处 RestoreSpeed

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp:491-495`（StopEat）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp:553-554`（ValidateEatingState）

- [ ] **Step 1: StopEat 删 RestoreSpeed**

把（`Zombie.cpp:491-495`）：

```cpp
			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);
				this->RestoreSpeed();
				plant->mEaterCount--;
			}
```

改成：

```cpp
			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);   // clip 清零，自动回落走速
				plant->mEaterCount--;
			}
```

- [ ] **Step 2: ValidateEatingState 删 RestoreSpeed**

把（`Zombie.cpp:551-555`）：

```cpp
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk2", 0.0f, 0.3f);
			RestoreSpeed();
```

改成：

```cpp
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk2", 0.0f, 0.3f);   // clip 清零，自动回落走速
```

---

## Task 5: Polevaulter.cpp — 删 2 处 RestoreSpeed + 跳跃后重设 base

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp:117-118`（EndJump 重设 base）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp:156-157`（ValidateEatingState）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp:200-201`（StopEat）

> 关键点：撑杆僵尸跳跃后从"快跑"永久降为"普通走"，这是**改 base** 不是临时 clip。旧代码靠 `PlayTrack("anim_walk", 0.9~1.7)` 写 `mSpeed`（base）实现；新模型 PlayTrack 不再写 base，所以必须显式 `SetAnimationSpeed` 重设 base，否则吃完植物 clip 清零会回落到"快跑"base 而非走速。
> （注意：第 119 行 `mSpeed = GameRandom::Range(7.0f, 13.0f)` 是**僵尸的移动速度(像素/秒)**，与动画无关，**不要动**。）

- [ ] **Step 1: EndJump 用 SetAnimationSpeed 重设动画 base**

把（`Polevaulter.cpp:117-118`）：

```cpp
	// 切换为走路动画和普通速度
	PlayTrack("anim_walk", GameRandom::Range(0.9f, 1.7f));
```

改成：

```cpp
	// 切换为走路动画和普通速度：跳跃后永久降速，写入动画 base（而非临时 clip）
	SetAnimationSpeed(GameRandom::Range(0.9f, 1.7f));
	PlayTrack("anim_walk");
```

- [ ] **Step 2: ValidateEatingState 删 RestoreSpeed**

把（`Polevaulter.cpp:156-157`）：

```cpp
				PlayTrack("anim_walk", 0.0f, 0.2f);
				RestoreSpeed();
```

改成：

```cpp
				PlayTrack("anim_walk", 0.0f, 0.2f);   // clip 清零，自动回落走速
```

- [ ] **Step 3: StopEat 删 RestoreSpeed**

把（`Polevaulter.cpp:200-201`）：

```cpp
				this->PlayTrack("anim_walk", 0.0f, 0.2f);
				this->RestoreSpeed();
```

改成：

```cpp
				this->PlayTrack("anim_walk", 0.0f, 0.2f);   // clip 清零，自动回落走速
```

---

## Task 6: WallNut.cpp — 啃食暂停改用状态层

**Files:**
- Modify: `PlantVsZombies/Game/Plant/WallNut.cpp:19-25`

> WallNut 被啃食时暂停动画。旧代码用 `SetOriginalSpeed/SetSpeed(0)/RestoreSpeed` 玩 base——正是要废掉的机制。"被啃暂停"本质是**状态语义**（与轨道无关），用 `mExtraSpeedMultiplier` 表达最干净。WallNut 是植物，不受僵尸 `SetCooldown` 影响，`mExtraSpeedMultiplier` 默认 1.0 可独占。

- [ ] **Step 1: 替换暂停/恢复逻辑**

把（`WallNut.cpp:19-25`）：

```cpp
	if (isBeingEaten && !mWasBeingEaten) {
		mAnimator->SetOriginalSpeed(mAnimator->GetSpeed());
		mAnimator->SetSpeed(0);
	}
	else if (!isBeingEaten && mWasBeingEaten) {
		mAnimator->RestoreSpeed();
	}
```

改成：

```cpp
	if (isBeingEaten && !mWasBeingEaten) {
		mAnimator->SetExtraSpeedMultiplier(0.0f);   // 被啃食：状态层暂停动画，不动 base
	}
	else if (!isBeingEaten && mWasBeingEaten) {
		mAnimator->SetExtraSpeedMultiplier(1.0f);   // 不再被啃：恢复
	}
```

---

## Task 7: GameInfoSaver.cpp — 存档字段 animOriginalSpeed → animClipSpeed

**Files:**
- Modify: `PlantVsZombies/GameInfoSaver.cpp:166-167`（存）
- Modify: `PlantVsZombies/GameInfoSaver.cpp:397-398`（读）

> `animSpeed`（= base）继续照存照读，不变。`animOriginalSpeed`（旧的 base-before-eat 存档）删除，改存 `animClipSpeed`（当前轨道覆盖），保持存档保真（例如存档发生在僵尸吃植物中途时，读档能恢复吃速）。
> 向后兼容：旧存档无 `animClipSpeed` → `z.value(..., 0.0f)` 默认 0 → 回落 base，安全。

- [ ] **Step 1: 存档侧**

把（`GameInfoSaver.cpp:166-167`）：

```cpp
		z["animSpeed"] = zombie->GetAnimationSpeed();
		z["animOriginalSpeed"] = zombie->GetOriginalSpeed();
```

改成：

```cpp
		z["animSpeed"] = zombie->GetAnimationSpeed();
		z["animClipSpeed"] = zombie->GetClipSpeed();
```

- [ ] **Step 2: 读档侧（顺序：PlayTrack→SetAnimationSpeed→SetClipSpeed）**

把（`GameInfoSaver.cpp:395-398`）：

```cpp
				zombie->PlayTrack(track);
				zombie->SetCurrentFrame(z.value("animFrame", 0.0f));
				zombie->SetAnimationSpeed(z.value("animSpeed", 1.0f));
				zombie->SetOriginalSpeed(z.value("animOriginalSpeed", 1.0f));
```

改成：

```cpp
				zombie->PlayTrack(track);                                  // 会把 clip 清零
				zombie->SetCurrentFrame(z.value("animFrame", 0.0f));
				zombie->SetAnimationSpeed(z.value("animSpeed", 1.0f));     // 恢复 base
				zombie->SetClipSpeed(z.value("animClipSpeed", 0.0f));      // 再恢复轨道覆盖
```

> 顺序很重要：`PlayTrack(track)` 内部会 `SetClipSpeed(0)`，所以 `SetClipSpeed(savedClip)` 必须放在它之后。

---

## Task 8: PaperZombie.cpp — 删 RestoreSpeed（⚠️ 主人正在改，执行前重新读取）

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/PaperZombie.cpp`（行号执行时定位，勿信本计划行号）

> ⚠️ **此文件主人正在手动编辑，git 起始状态即为 modified。执行本任务前必须先 Read 整个文件，按当前内容协调。** 下面是计划编写时观察到的需删点，仅供定位参考。

- [ ] **Step 1: 重新读取整个 PaperZombie.cpp**

Run: 用 Read 工具读 `PlantVsZombies/Game/Zombie/PaperZombie.cpp` 全文。

- [ ] **Step 2: 定位并删除所有 `RestoreSpeed()` 调用**

用 Grep 在该文件搜 `RestoreSpeed`。计划编写时存在于：ValidateEatingState（曾在 ~41 行，`PlayTrack("anim_walk"/"anim_walk_nopaper", 0.0f, 0.2f)` 之后）与 StopEat（曾在 ~87 行，同样模式之后）。

对每一处，删除 `RestoreSpeed();` / `this->RestoreSpeed();` 那一行，**保留**其前面的 `PlayTrack(..., 0.0f, ...)`（clip 清零已由 PlayTrack 完成）。

> 说明：PaperZombie 的吃植物 `PlayTrack("anim_eat", 2.1f, ...)` / `PlayTrack("anim_eat_nopaper", 3.2f, ...)` 是绝对 clip 速度，**无需改动**——它们在新模型下行为完全不变（绝对覆盖）。撕报纸切到 `anim_walk_nopaper` 等也只是普通 PlayTrack，不受影响。

- [ ] **Step 3: 全仓库确认无遗漏的旧 API 调用**

用 Grep 在 `PlantVsZombies/` 全目录搜 `RestoreSpeed|SetOriginalSpeed|GetOriginalSpeed`。
Expected：仅剩 Animator.h/AnimatedObject 中**已被本计划删除**的位置不应再出现；若任何 `.cpp` 仍调用，逐一按"删 RestoreSpeed / 保留前面的 PlayTrack(0.0)"原则处理。

---

## Task 9: 编译 + 游戏内验证（主人在 VS 操作）

**Files:** 无（验证步骤）

- [ ] **Step 1: 主人在 VS 编译（F7 / Build Solution）**

Expected：0 error。常见可能错误与对策：
- `RestoreSpeed / SetOriginalSpeed / GetOriginalSpeed 不是成员` → 说明还有调用点没改，回 Task 8 Step 3 的 Grep 排查。
- `mOriginalSpeed 未声明` → Animator.cpp 仍有引用，全文件搜 `mOriginalSpeed` 应为 0 处。

- [ ] **Step 2: 主人 F5 运行，按下表逐项肉眼验证**

| 场景 | 操作 | 期望（与改动前一致） |
|---|---|---|
| 普通僵尸走路有速度差异 | 放出多只普通僵尸 | 各僵尸走速略有不同（base 随机生效） |
| 僵尸啃食速度 | 让僵尸啃坚果/普通植物 | 啃食动画明显快（绝对 2.1 速度，所有僵尸一致），**不因 base 而忽快忽慢** |
| 啃完恢复走路 | 植物被吃光/移走，僵尸继续走 | 立刻回到该僵尸自己的走速，**不残留吃饭快放** |
| 减速叠加啃食 | 寒冰射手减速 + 僵尸啃食 | 啃食动画整体变慢，减速不被啃食冲掉 |
| 撑杆僵尸跳跃 | 撑杆僵尸撞植物起跳 | 跳跃动画快放，落地后转为普通走速（比跑步慢） |
| 撑杆跳后再啃食 | 跳过后走到下一植物啃 | 啃食快放→啃完回到"普通走速"（**不是**跳前的快跑速度） |
| 坚果墙被啃 | 僵尸啃坚果墙 | 坚果墙"咀嚼/受击"动画暂停；僵尸离开后恢复 |
| 樱桃炸弹 | 种樱桃炸弹 | 爆炸前的蓄力动画速度与改动前一致（慢放 0.34~0.45） |
| 存读档 | 啃食中途存档→读档 | 读回后僵尸仍以正确速度啃食/走路，无突变 |

- [ ] **Step 3: -Debug 跑一遍确认无回归（可选）**

Run（主人）：以 `-Debug` 启动，观察碰撞框与僵尸/植物交互无异常。

---

## Task 10: 提交

**Files:** 全部上述改动一次提交（原子重构）

- [ ] **Step 1: 主人确认验证全过后，提交**

```bash
git add PlantVsZombies/Reanimation/Animator.h PlantVsZombies/Reanimation/Animator.cpp \
  PlantVsZombies/Game/AnimatedObject.h PlantVsZombies/Game/AnimatedObject.cpp \
  PlantVsZombies/Game/Zombie/Zombie.cpp PlantVsZombies/Game/Zombie/Polevaulter.cpp \
  PlantVsZombies/Game/Zombie/PaperZombie.cpp PlantVsZombies/Game/Plant/WallNut.cpp \
  PlantVsZombies/GameInfoSaver.cpp
git commit -m "refactor(anim): clip 速度层替代 mOriginalSpeed/RestoreSpeed 两步舞

有效速度 = (clip 覆盖 ? clip : base) * 状态倍率；PlayTrack 不再覆盖 base，
切轨道自动清 clip 回落 base。删除 mOriginalSpeed 与全局 RestoreSpeed 协议，
Polevaulter 跳后用 SetAnimationSpeed 重设 base，WallNut 啃食暂停改用状态层，
存档 animOriginalSpeed→animClipSpeed。行为对齐改动前。"
```

> 提交信息按需调整；若主人有自己的提交习惯，以主人为准。

---

## Self-Review（计划编写者自查记录）

- **Spec 覆盖**：删 `mOriginalSpeed`(T1/T2/T3) ✓；删 `RestoreSpeed` 全部调用点(T2/T4/T5/T8) ✓；PlayTrack 不碰 base(T2-S1) ✓；clip 切轨道自动失效(T2-S1 写 0) ✓；手感不变=绝对覆盖语义(EffectiveSpeed) ✓；存档兼容(T7) ✓；Polevaulter 重设 base(T5) ✓；WallNut 暂停(T6) ✓；GetTrackVelocity 一致(T2-S7) ✓；子动画递归一致(T2-S6) ✓。
- **类型/命名一致性**：`SetClipSpeed`/`GetClipSpeed`/`EffectiveSpeed`/`mClipSpeed` 全文一致；存档字段 `animClipSpeed` 存读一致。
- **占位符扫描**：无 TBD/TODO；每个代码步给出完整前后文。
- **已知 watch-points**：
  1. 加载**旧存档**且其僵尸恰处吃植物中途时，旧 `animSpeed` 字段存的是被覆盖的吃速（2.1）而非 base，会让 base 略偏——仅影响那一刻、极边缘、自纠正；新存档无此问题。
  2. 空 `returnTrack` 的一次性动画（土豆雷压扁/死亡/碎裂/爆炸）播完 clip 残留，但动画已停止且对象即将销毁、不入对象池，视觉无害。
  3. PaperZombie 为主人实时编辑，T8 强制重新读取。
