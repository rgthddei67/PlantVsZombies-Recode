# 僵尸「啃食 → 走路」状态机重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把僵尸「怎么走路」收敛为唯一权威虚函数 `PlayWalkAnimation`、把「啃食视觉残留」收敛为一对对称钩子 `OnStartEating`/`OnStopEating`、`ResumeWalkAfterEat` 降级为非虚模板方法，删除三个子类各自复制的整段覆写，行为逐条等价。

**Architecture:** 模板方法模式。非虚 `ResumeWalkAfterEat(blend) { OnStopEating(); PlayWalkAnimation(blend); }` 锁死「先收尾后走路」顺序；子类只覆写两个正交钩子。分 5 个任务，**每个任务提交后 build 绿 + 行为不变**：先加基类脚手架（纯加法、行为等价），再逐个子类迁移（每个子类删覆写、加钩子），最后收口（删 `WalkTrackAfterEat`、`PlayWalkAnimation` 内联 walk2、`ResumeWalkAfterEat` 转非虚）。

**Tech Stack:** C++17，CMake + vcpkg（`clang-release` preset），AutoTest JSON 脚本 + `dump_state`（`track`/`armVisible`/`doorArmVisible` 字段）作回归验证。无单元测试框架——本重构以「回归行为不变」为验证目标。

设计来源：`docs/superpowers/specs/2026-07-03-zombie-eat-walk-state-machine-design.md`（逐语句等价证明见其 §6）。

## Global Constraints

- **路径前缀**：源码在仓库根下的嵌套目录 `PlantVsZombies/Game/Zombie/`（git 根 = 仓库根，`cwd` 亦是）。
- **构建**（每个任务的验证步骤都用它；须在 VS developer 环境内，见 `CLAUDE.md`）：
  ```powershell
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
  cmake --build --preset clang-release
  ```
  `clang-release` 是唯一报 `-Wreorder-ctor`/`-Wunused-*`/`-Woverride` 等诊断的配置——**warning-zero 验证只在此 preset 有效**。
- **AutoTest 运行**（工作目录 = `build\clang-release\`）：
  ```powershell
  Push-Location build\clang-release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本>.json -Seed 42
  $LASTEXITCODE   # 0=成功
  Pop-Location
  ```
  产物在 `build\clang-release\autotest\out\<脚本名>\`：`state_*.json`（读 `zombies[].track`/`armVisible`/`doorArmVisible`）、`*.png`（Read 工具直接看）、`run.log`。
- **回归套件**（每个任务改完全跑，逐条 `LASTEXITCODE==0` 且 dump 断言通过）：
  `smoke_gameplay`、`smoke_charm_basic`、`smoke_charm_bite`、`smoke_charm_door_eating`、`smoke_charm_door_eat_zombie`、`smoke_charm_paper_eating`、`smoke_charm_polevaulter`、`repro_paper_death`、`repro_paper_cherry`、`smoke_door_fume_death`。
- **验证边界**：`ValidateEatingState` 仅读档执行，AutoTest 短路存档 → 该路径靠 spec §6 等价证明 + 编译验证，不 AutoTest。
- **提交策略**：`git commit` 是本会话的活（每个任务末尾提交），`git push` 不做（主人的活）。
- 行为**有意变更（读档恢复走路混合时间统一 0.2→0.3）**：基类 `ValidateEatingState` 本用 0.3f；Paper/Polevaulter 被删覆写的植物已死+啃僵尸共 4 条子路径原用 0.2f，收敛后统一 0.3f。纯视觉、仅读档、0.1s 淡入差异、肉眼不可辨，取「与其余僵尸一致」。code-review 补正（原文档仅记撑杆一条）。其余全部逐条等价。

---

### Task 1: 基类脚手架（新契约 + 路由 + StartEat 触发 + 收编 682/742）

纯加法、对**所有**僵尸行为等价。此任务后基类六条啃食结束路径全部经 `ResumeWalkAfterEat`，但各子类旧覆写仍在（shadow），行为不变。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`（`ValidateEatingState`/`WalkTrackAfterEat`/`ResumeWalkAfterEat` 声明附近，约 123/139–143 行）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（`StartEat` 623–657、`StopEat` 682、`ValidateEatingState` 742、`ResumeWalkAfterEat` 373–377）

**Interfaces:**
- Produces:
  - `virtual void Zombie::PlayWalkAnimation(float blendTime = 0.0f);` — 稳态走路唯一权威。Task 1 临时实现 `PlayTrack(WalkTrackAfterEat(), 0.0f, blendTime)`（桥接，Task 5 收口）。
  - `virtual void Zombie::OnStartEating() {}` / `virtual void Zombie::OnStopEating() {}` — 啃食视觉残留对称钩子，默认空。
  - `void Zombie::ResumeWalkAfterEat(float blendTime)`（仍 virtual）实现改为 `{ OnStopEating(); PlayWalkAnimation(blendTime); }`。
- Consumes: 现有 `PlayTrack(name, clipSpeed, blendTime)`、`WalkTrackAfterEat()`、成员 `mIsEating`。

- [ ] **Step 1: 在 `Zombie.h` 加三个新虚函数声明**（保留现有 `WalkTrackAfterEat`/`ResumeWalkAfterEat` 声明不动）

在 `virtual void ResumeWalkAfterEat(float blendTime);`（约 143 行）之后插入：

```cpp
// 稳态走路的唯一权威：播哪条走路轨道 + clip 速度（确定性，勿随机）。Task 5 收口后内联 anim_walk2。
virtual void PlayWalkAnimation(float blendTime = 0.0f);
// 啃食视觉残留对称钩子：OnStartEating 开吃触发、OnStopEating 停吃触发，默认空操作。
virtual void OnStartEating() {}
virtual void OnStopEating()  {}
```

- [ ] **Step 2: 在 `Zombie.cpp` 实现 `PlayWalkAnimation`，并改写 `ResumeWalkAfterEat`**

把现有（373–377）：
```cpp
void Zombie::ResumeWalkAfterEat(float blendTime)
{
	// clip 清零 → EffectiveSpeed 回落到 base 走速（普通僵尸的 anim_walk2 无需 clip）。
	PlayTrack(WalkTrackAfterEat(), 0.0f, blendTime);
}
```
替换为：
```cpp
void Zombie::PlayWalkAnimation(float blendTime)
{
	// Task 1 临时桥接：暂经 WalkTrackAfterEat() 保持子类行为不变；Task 5 删 WalkTrackAfterEat 后改硬编码 "anim_walk2"。
	PlayTrack(WalkTrackAfterEat(), 0.0f, blendTime);
}

void Zombie::ResumeWalkAfterEat(float blendTime)
{
	// 模板：先收尾（藏啃食露出的部件），再回走路。子类改 OnStopEating / PlayWalkAnimation，勿覆写本函数。
	OnStopEating();
	PlayWalkAnimation(blendTime);
}
```

- [ ] **Step 3: `StartEat` 两分支加 `OnStartEating` 触发**

在 `Zombie::StartEat`（约 623）顶部 `if (mIsPreview || mIsDying) return;` 之后加 `const bool wasEating = mIsEating;`。
啃僵尸分支 `mEatZombieID = target->mZombieID;` 之后、`return;` 之前插入：
```cpp
		if (!wasEating && mIsEating) OnStartEating();
```
啃植物分支 `plant->mEaterCount++;` 之后插入：
```cpp
				if (!wasEating && mIsEating) OnStartEating();
```

- [ ] **Step 4: 收编两处硬编码到 `ResumeWalkAfterEat`**

`StopEat` 植物分支（682）：
```cpp
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);   // clip 清零，自动回落走速
```
→
```cpp
				this->ResumeWalkAfterEat(0.2f);
```
`ValidateEatingState` 植物没了分支（742）：
```cpp
			PlayTrack("anim_walk2", 0.0f, 0.3f);   // clip 清零，自动回落走速
```
→
```cpp
			ResumeWalkAfterEat(0.3f);
```

- [ ] **Step 5: 构建（warning-zero）**

Run: Global Constraints 的构建命令。
Expected: `cmake --build` 成功，0 warning / 0 error。

- [ ] **Step 6: 回归套件全绿**

Run: 依次跑 Global Constraints「回归套件」10 个脚本。
Expected: 每个 `LASTEXITCODE==0`。抽查 `smoke_charm_door_eating` 的 `state_after_charm.json`：门僵尸 `armVisible`/`doorArmVisible` 与改动前一致（此任务门僵尸仍走自己的覆写，行为必然不变）；`smoke_charm_paper_eating` 的纸僵尸 `track` 为走路轨道（非卡 `anim_eat`）。

- [ ] **Step 7: 提交**

```bash
git add PlantVsZombies/Game/Zombie/Zombie.h PlantVsZombies/Game/Zombie/Zombie.cpp
git commit -m "refactor(zombie): 基类引入 PlayWalkAnimation+OnStart/OnStopEating 钩子，ResumeWalkAfterEat 改模板方法，收编 682/742 硬编码（行为等价）"
```

---

### Task 2: DoorZombie 迁移到对称钩子

删 `StartEat`/`StopEat`/`ValidateEatingState`/`ResumeWalkAfterEat` 四个覆写，改为 `OnStartEating`/`OnStopEating` 两个一行钩子。基类 StartEat/StopEat/Validate 现全经 `OnStartEating`/`OnStopEating`，门臂在**每条**路径对称显隐。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/DoorZombie.h`（17–33 行覆写声明）
- Modify: `PlantVsZombies/Game/Zombie/DoorZombie.cpp`（删 26–119 的 4 个函数，加 2 钩子）

**Interfaces:**
- Consumes: 基类 `OnStartEating`/`OnStopEating`（Task 1）、成员 `ShowArm(bool)`、`mShieldType`、`ShieldType::SHIELDTYPE_NONE`。
- Produces: `void DoorZombie::OnStartEating() override` / `void DoorZombie::OnStopEating() override`。

- [ ] **Step 1: 改 `DoorZombie.h` 声明**

删除这四行覆写声明（`ResumeWalkAfterEat` 19、`ValidateEatingState` 31、`StartEat` 32、`StopEat` 33，含其上方注释块 17–18）：
```cpp
	void ResumeWalkAfterEat(float blendTime) override;
	...
	void ValidateEatingState(EntityManager& em) override;
	void StartEat(ColliderComponent* other) override;
	void StopEat(ColliderComponent* other) override;
```
加入（`protected:` 区或与其余 override 同级）：
```cpp
	// 啃食露常规手臂 / 收尾藏回门后（门还在才动）。见 c0cb799 / 52bd86d。
	void OnStartEating() override;
	void OnStopEating()  override;
```

- [ ] **Step 2: 改 `DoorZombie.cpp`——删四函数、加两钩子**

删除 `DoorZombie::StartEat`（26–56）、`DoorZombie::StopEat`（58–84）、`DoorZombie::ValidateEatingState`（86–109）、`DoorZombie::ResumeWalkAfterEat`（111–119）。
在其位置加入：
```cpp
void DoorZombie::OnStartEating()
{
	// 啃食露出常规手臂（门还在才露）。与 OnStopEating 对称——修复"啃敌方僵尸时无手臂"(52bd86d)。
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		this->ShowArm(true);
}

void DoorZombie::OnStopEating()
{
	// 收尾把常规手臂藏回门后（门还在才藏）。与 OnStartEating 对称——修复"被魅惑后多一条手臂"(c0cb799)。
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		this->ShowArm(false);
}
```
（`SetupZombie` 的 `ShowArm(false)` 与随机起步走路、`HeadDrop`/`ArmDrop`/`ZombieItemUpdate` 全部保留不动。）

- [ ] **Step 3: 构建（warning-zero）**

Run: 构建命令。Expected: 成功，0 warning（若漏删声明会报 `-Woverride`/未定义符号）。

- [ ] **Step 4: 门僵尸回归 + 手臂断言**

Run: `smoke_charm_door_eating`、`smoke_charm_door_eat_zombie`、`smoke_door_fume_death`。
Expected: 各 `LASTEXITCODE==0`。
- `smoke_charm_door_eating` 的 `state_before_charm.json`：门僵尸啃墙果时 `armVisible==true`（OnStartEating 露臂）；`state_after_charm.json`：魅惑收尾后 `armVisible==false`（OnStopEating 藏臂），无多余手臂。
- `smoke_charm_door_eat_zombie` 的 dump：门僵尸啃敌方僵尸时 `armVisible==true`（52bd86d 回归）。
截图 `door_charmed.png` 目视手臂无重影。

- [ ] **Step 5: 全套回归**

Run: 其余回归套件脚本。Expected: 全绿。

- [ ] **Step 6: 提交**

```bash
git add PlantVsZombies/Game/Zombie/DoorZombie.h PlantVsZombies/Game/Zombie/DoorZombie.cpp
git commit -m "refactor(zombie): DoorZombie 四覆写→OnStart/OnStopEating 对称钩子，门臂显隐结构性收敛"
```

---

### Task 3: PaperZombie 迁移到 PlayWalkAnimation

`ResumeWalkAfterEat` 覆写 → `PlayWalkAnimation` 覆写；删 `ValidateEatingState`/`StopEat`；`StartEat` 植物分支补 `OnStartEating()`（契约普适，纸僵尸自身 no-op）；`LoadExtraData` 转调 `PlayWalkAnimation`。`FastPaperZombie` 无覆写、自动继承。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/PaperZombie.h`（16–21 覆写声明）
- Modify: `PlantVsZombies/Game/Zombie/PaperZombie.cpp`（`LoadExtraData` 125、`ValidateEatingState` 163–187、`StartEat` 189–215、`StopEat` 217–242、`ResumeWalkAfterEat` 244–252）

**Interfaces:**
- Consumes: 基类 `PlayWalkAnimation`/`OnStartEating`（Task 1）、`kNoPaperWalkClip`、`mHasNewspaper`、`mIsEating`。
- Produces: `void PaperZombie::PlayWalkAnimation(float blendTime) override`（walk / walk_nopaper+clip）。

- [ ] **Step 1: 改 `PaperZombie.h` 声明**

删除：
```cpp
	void ValidateEatingState(EntityManager& em) override;
	...
	void StopEat(ColliderComponent* other) override;
```
把（21 行附近，含上方注释 19–20）：
```cpp
	// anim_walk / anim_walk_nopaper，狂暴态还要带 clip 速度（与自身 StopEat 一致）。
	void ResumeWalkAfterEat(float blendTime) override;
```
改为：
```cpp
	// 稳态走路：有报纸播 anim_walk；狂暴无报纸播 anim_walk_nopaper 且带 clip 速度。
	void PlayWalkAnimation(float blendTime) override;
```
保留 `void StartEat(ColliderComponent* other) override;`。

- [ ] **Step 2: `PaperZombie.cpp`——`ResumeWalkAfterEat` 改名为 `PlayWalkAnimation`**

把（244–252）：
```cpp
void PaperZombie::ResumeWalkAfterEat(float blendTime)
{
	// 基类默认会播 "anim_walk2"，而 PaperZombie.reanim 只有 anim_walk / anim_walk_nopaper。
	// 与 StopEat / ValidateEatingState 的选轨道逻辑保持一致：狂暴态带 clip 速度，普通态 clip 清零。
	if (mHasNewspaper)
		PlayTrack("anim_walk", 0.0f, blendTime);
	else
		PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blendTime);
}
```
改为：
```cpp
void PaperZombie::PlayWalkAnimation(float blendTime)
{
	// PaperZombie.reanim 无 anim_walk2；有报纸走 anim_walk，狂暴无报纸走 anim_walk_nopaper 且带 clip 速度。
	if (mHasNewspaper)
		PlayTrack("anim_walk", 0.0f, blendTime);
	else
		PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blendTime);
}
```

- [ ] **Step 3: 删 `ValidateEatingState` 和 `StopEat`**

删除 `PaperZombie::ValidateEatingState`（163–187）与 `PaperZombie::StopEat`（217–242）整段。（基类版本现经 `PlayWalkAnimation` 兜底，等价。）

- [ ] **Step 4: `StartEat` 植物分支补 `OnStartEating()`**

在 `PaperZombie::StartEat` 植物分支的 `if (!mIsEating) { ... }` 块内、播完 eat 动画之后加 `OnStartEating();`：
```cpp
				if (!mIsEating) {
					if (mHasNewspaper)
						this->PlayTrack("anim_eat", 2.1f, 0.2f);
					else
						this->PlayTrack("anim_eat_nopaper", kNoPaperEatClip, 0.2f);
					OnStartEating();   // 契约：开吃即触发（纸僵尸自身 no-op，保钩子普适）
				}
```
（啃僵尸分支已调 `Zombie::StartEat`，基类会触发，无需补。）

- [ ] **Step 5: `LoadExtraData` 狂暴脱困转调 `PlayWalkAnimation`**

把（125）：
```cpp
				else
					PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.0f);
```
改为：
```cpp
				else
					PlayWalkAnimation(0.0f);
```
（此分支 `!mHasNewspaper` 恒成立，`PlayWalkAnimation` 走 walk_nopaper+clip，等价。）

- [ ] **Step 6: 构建（warning-zero）**

Run: 构建命令。Expected: 成功，0 warning。

- [ ] **Step 7: 纸僵尸回归**

Run: `smoke_charm_paper_eating`、`repro_paper_death`、`repro_paper_cherry`。
Expected: 各 `LASTEXITCODE==0`。`smoke_charm_paper_eating` dump：纸僵尸被魅惑收尾后 `track` 为 `anim_walk`/`anim_walk_nopaper`（**非卡 `anim_eat`**，d83bab0 回归）。追加：临时脚本或复用 `smoke_charm_paper_eating` 把 `ZOMBIE_NEWSPAPER` 换 `ZOMBIE_FASTPAPER` 各跑一次确认两级继承（`FastPaperZombie` 无覆写，行为随 `PaperZombie::PlayWalkAnimation`）。

- [ ] **Step 8: 全套回归 + 提交**

Run: 其余回归套件。Expected: 全绿。
```bash
git add PlantVsZombies/Game/Zombie/PaperZombie.h PlantVsZombies/Game/Zombie/PaperZombie.cpp
git commit -m "refactor(zombie): PaperZombie ResumeWalkAfterEat→PlayWalkAnimation，删 Validate/StopEat 覆写，StartEat 补 OnStartEating 保契约"
```

---

### Task 4: Polevaulter 迁移到 PlayWalkAnimation

删 `WalkTrackAfterEat`/`ValidateEatingState`/`StopEat`；加 `PlayWalkAnimation`（anim_walk）；`EndJump`/`LoadExtraData` 转调。保留 `StartEat`（RUNNING 守卫）。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.h`（20/21/27/42 覆写声明）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp`（`EndJump` 124、`ValidateEatingState` 155–173、`LoadExtraData` 191、`StopEat` 210–231）

**Interfaces:**
- Consumes: 基类 `PlayWalkAnimation`（Task 1）。
- Produces: `void Polevaulter::PlayWalkAnimation(float blendTime) override`（播 `anim_walk`）。

- [ ] **Step 1: 改 `Polevaulter.h` 声明**

删除：
```cpp
	void StopEat(ColliderComponent* other) override;
	...
	void ValidateEatingState(EntityManager& em) override;
	...
	const char* WalkTrackAfterEat() const override { return "anim_walk"; }
```
加入（`protected:` 区）：
```cpp
	// Polevaulter.reanim 无 anim_walk2，走路统一 anim_walk。
	void PlayWalkAnimation(float blendTime) override;
```
保留 `void StartEat(ColliderComponent* other) override;`。

- [ ] **Step 2: `Polevaulter.cpp`——加 `PlayWalkAnimation`，删两覆写**

在文件内（如 `StopEat` 原位置）加入：
```cpp
void Polevaulter::PlayWalkAnimation(float blendTime)
{
	PlayTrack("anim_walk", 0.0f, blendTime);
}
```
删除 `Polevaulter::ValidateEatingState`（155–173）与 `Polevaulter::StopEat`（210–231）整段。

- [ ] **Step 3: `EndJump` 与 `LoadExtraData` 转调 `PlayWalkAnimation`**

`EndJump`（124）`PlayTrack("anim_walk");` → `PlayWalkAnimation();`
`LoadExtraData` WALKING 恢复（191）`PlayTrack("anim_walk");` → `PlayWalkAnimation();`
（两处均 clip=0 回落 base 速度，与旧单参 `PlayTrack("anim_walk")` 等价——`EndJump` 已先 `SetAnimationSpeed` 设 base 速度。）

- [ ] **Step 4: 构建（warning-zero）**

Run: 构建命令。Expected: 成功，0 warning。

- [ ] **Step 5: 撑杆回归**

Run: `smoke_charm_polevaulter`。
Expected: `LASTEXITCODE==0`。`state_after_cross.json`：撑杆跳跃落地后 `track==anim_walk`（`EndJump`→`PlayWalkAnimation` 生效，非卡 `anim_jump`/`anim_run`）。截图 `charm_polevaulter_cross.png` 目视撑杆落地正常走路。

- [ ] **Step 6: 全套回归 + 提交**

Run: 全部回归套件。Expected: 全绿（撑杆改动不影响其他僵尸）。
```bash
git add PlantVsZombies/Game/Zombie/Polevaulter.h PlantVsZombies/Game/Zombie/Polevaulter.cpp
git commit -m "refactor(zombie): Polevaulter WalkTrackAfterEat/Validate/StopEat 三覆写→PlayWalkAnimation，EndJump/Load 转调"
```

---

### Task 5: 收口——删 `WalkTrackAfterEat`、内联 walk2、`ResumeWalkAfterEat` 转非虚

此时**无任何子类**覆写 `WalkTrackAfterEat`（Polevaulter 已于 Task 4 删）或 `ResumeWalkAfterEat`（Door/Paper 已删）。可安全删桥接、锁死模板方法。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`（删 `WalkTrackAfterEat` 声明；`ResumeWalkAfterEat` 转非虚并内联；补新契约注释块）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（`PlayWalkAnimation` 内联 walk2；删 `ResumeWalkAfterEat` 的 .cpp 定义）

**Interfaces:**
- Produces: 最终契约（`PlayWalkAnimation` 硬编码 `anim_walk2`；`ResumeWalkAfterEat` 非虚 inline）。
- Consumes: —（无桥接依赖）。

- [ ] **Step 1: 全仓库确认无 `WalkTrackAfterEat` 残留引用**

Run（Grep 工具）：搜索 `WalkTrackAfterEat`。
Expected: 仅剩 `Zombie.h`/`Zombie.cpp` 的声明与 `PlayWalkAnimation` 桥接调用；无任何子类 override。若有残留，回到对应任务补删。

- [ ] **Step 2: `Zombie.cpp` `PlayWalkAnimation` 内联 walk2**

```cpp
void Zombie::PlayWalkAnimation(float blendTime)
{
	// Task 1 临时桥接：暂经 WalkTrackAfterEat() ...
	PlayTrack(WalkTrackAfterEat(), 0.0f, blendTime);
}
```
→
```cpp
void Zombie::PlayWalkAnimation(float blendTime)
{
	// 基类稳态走路：anim_walk2、clip 清零（回落 base 走速）。reanim 无 anim_walk2 的子类覆写本函数。
	PlayTrack("anim_walk2", 0.0f, blendTime);
}
```

- [ ] **Step 3: `Zombie.cpp` 删 `ResumeWalkAfterEat` 的 .cpp 定义**

删除 `void Zombie::ResumeWalkAfterEat(float blendTime) { OnStopEating(); PlayWalkAnimation(blendTime); }` 整段（改为 Step 4 在头文件内联）。

- [ ] **Step 4: `Zombie.h`——删 `WalkTrackAfterEat`、`ResumeWalkAfterEat` 转非虚内联、补最终注释**

删除：
```cpp
	// 停止啃食后回落的走路轨道（基类随机双走路动画统一回 walk2；撑杆只有 anim_walk）
	virtual const char* WalkTrackAfterEat() const { return "anim_walk2"; }
```
把 `virtual void ResumeWalkAfterEat(float blendTime);` 与 Task 1 加的三个虚函数声明整体替换为带示例的最终契约（spec §3 成品）：
```cpp
// ═══ 啃食 → 走路 状态机：扩展新僵尸只需覆写下面带 ★ 的 virtual，永不碰 ResumeWalkAfterEat ═══

// ★关注点A｜"我此刻怎么走路"的唯一权威：播哪条稳态走路轨道 + clip 速度（须确定性，勿随机）。
//   啃完回走路 / 撑杆落地 / 读档恢复 全经它，所以"改走路动画"永远只改这一处。
//   何时覆写：reanim 没有 anim_walk2，或走路轨道随状态变。
//   例（读报僵尸·狂暴撕报后换轨道并带 clip 速度）：
//     void PlayWalkAnimation(float blend) override {
//         if (mHasNewspaper) PlayTrack("anim_walk", 0.0f, blend);
//         else               PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blend);
//     }
virtual void PlayWalkAnimation(float blendTime = 0.0f);

// ★关注点B｜啃食视觉残留（一对对称钩子，默认空操作，绝大多数僵尸不用管）：
//   OnStartEating 一开吃触发、OnStopEating 一停吃触发。在 StartEat 里改过的视觉状态，在这对里对称还原。
//   例（铁门僵尸·啃食露常规手臂、收尾藏回门后，门还在才动）：
//     void OnStartEating() override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(true);  }
//     void OnStopEating()  override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(false); }
virtual void OnStartEating() {}
virtual void OnStopEating()  {}

// 模板方法（非虚，勿覆写）：啃完回走路 = 先收尾、再走路。执行顺序由基类锁死。
void ResumeWalkAfterEat(float blendTime) { OnStopEating(); PlayWalkAnimation(blendTime); }
```

- [ ] **Step 5: 构建（warning-zero）**

Run: 构建命令。Expected: 成功，0 warning / 0 error（编译器确认无子类再覆写 `WalkTrackAfterEat`/`ResumeWalkAfterEat`）。

- [ ] **Step 6: 全套回归（最终验收）**

Run: 全部 10 个回归套件脚本。
Expected: 全 `LASTEXITCODE==0`；关键 dump 断言：
- 普通/路障/铁桶僵尸啃食后 `track==anim_walk2`；
- 门僵尸 `armVisible` 开吃 true / 收尾 false；
- 纸僵尸 `track` 走路轨道不卡 `anim_eat`；
- 撑杆落地 `track==anim_walk`。

- [ ] **Step 7: 提交**

```bash
git add PlantVsZombies/Game/Zombie/Zombie.h PlantVsZombies/Game/Zombie/Zombie.cpp
git commit -m "refactor(zombie): 收口——删 WalkTrackAfterEat、PlayWalkAnimation 内联 anim_walk2、ResumeWalkAfterEat 转非虚模板方法+最终契约注释"
```

---

## Self-Review（对照 spec）

- **Spec 覆盖**：§3 契约→Task 1+5；§4 A 组收编→Task 1 Step 4；§4 B 组转调→Task 3 Step 5 / Task 4 Step 3；§4 C 组不动→未列入任何任务（正确）；§4 OnStartEating 触发+普适补丁→Task 1 Step 3 + Task 3 Step 4；§5 每子类改动→Task 2/3/4；§6 兼容→各任务验证步 dump 断言；§7 回归矩阵→Global Constraints 回归套件 + 各任务 dump 断言；§8 风险→逐任务小步 + warning-zero + 门臂断言。全覆盖。
- **占位符扫描**：无 TBD/TODO；所有代码步给出完整前后代码。
- **类型一致**：`PlayWalkAnimation(float blendTime)`、`OnStartEating()`、`OnStopEating()` 签名跨 Task 1→5 一致；子类 override 签名（`PaperZombie`/`Polevaulter` 的 `PlayWalkAnimation(float)`、`DoorZombie` 的 `OnStart/OnStopEating()`）与基类一致。
- **顺序安全**：Task 1 纯加法（子类旧覆写 shadow，行为不变）；Task 2–4 逐子类删覆写时，基类路径已（Task 1）就绪；Task 5 删桥接前 Step 1 先确认无残留 override。每个任务提交点 build 绿、行为等价。
