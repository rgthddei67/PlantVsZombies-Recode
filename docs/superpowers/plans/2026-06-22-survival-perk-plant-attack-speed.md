# 植物攻速词条 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给生存模式加一个植物增益词条「植物攻速」——每层使全体射击类植物开火速度 +15%（最多 8 层，满层 2.2×）。

**Architecture:** 原型 A（无状态倍率）。`SurvivalPerkManager` 加聚合 getter `GetPlantAttackSpeedMultiplier()`（1 + 0.15×层数，0 层=1.0）。三个开火卡点（`Shooter`/`Repeater`/`PuffShroom` 的 `PlantUpdate`）各取一次倍率，**两处一起改**：缩短 `mShootTime` 间隔阈值（`/mult`）+ 同比例放大射击动画 clip-speed（`*mult`，让第 64/28 帧吐弹点跟上更短间隔）。层数走现有 Save/Load，存档零改动。

**Tech Stack:** C++17、CMake + vcpkg、clang-release 构建、AutoTest JSON 脚本（dump_state + 读 dump 人工核对数学闭合）。

---

## 测试方式说明（本项目无单元测试框架）

本仓库没有 C++ 单元测试。既定"测试"= AutoTest 烟雾脚本驱动游戏 → `dump_state` 写 JSON → **由实现者读 dump JSON 人工核对数学闭合 + 回归旧值**。不要尝试搭建 gtest/catch2 之类框架。

- 运行（工作目录 = exe 所在）：
  ```powershell
  Push-Location D:\PVZ\PlantsVsZombies\PlantVsZombies\build\clang-release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks_attackspeed.json -Seed 42
  $LASTEXITCODE   # 0=成功
  Pop-Location
  ```
- dump 产物：`build\clang-release\autotest\out\smoke_perks_attackspeed\attackspeed.json`，用 Read 工具直接看。
- `dump_state` 只能验证 **manager 层倍率数学**；游戏内真实开火节奏（三处卡点）由 build 通过 + 代码正确性 + Task 2 行为截图佐证。

## 构建命令（每个 build 步都用）

```powershell
# 1) 导入 VS dev 环境（Installer 先入 PATH 消除 vswhere 噪声）
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
# 2) 构建（clang-release 是唯一报全量警告的配置，验收须 0 warning/0 error）
cmake --build --preset clang-release
```

## 文件清单

| 文件 | 职责 | 动作 |
|---|---|---|
| `PlantVsZombies/Game/Perk/PerkType.h` | 词条枚举 | 改：`COUNT` 前加 `PLANT_ATTACK_SPEED` |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.h` | 聚合 getter 声明 | 改：加 `GetPlantAttackSpeedMultiplier()` |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp` | kPerks 表 + getter 实现 | 改：加表行 + getter |
| `PlantVsZombies/Game/Plant/Shooter.cpp` | 单发开火卡点 | 改：`PlantUpdate` 缩放间隔+动画 |
| `PlantVsZombies/Game/Plant/Repeater.h` | 双发开火卡点 | 改：`PlantUpdate` 缩放间隔+两处动画 |
| `PlantVsZombies/Game/Plant/PuffShroom.cpp` | 小喷菇开火卡点 | 改：`PlantUpdate` 缩放间隔+动画 |
| `PlantVsZombies/Game/AutoTest/TestDriver.cpp` | AutoTest 接入 | 改：`kPerkNames` + dump 字段 |
| `autotest/scripts/smoke_perks_attackspeed.json` | 闭合断言脚本 | 建 |

## 预飞决策（执行前与主人确认一次）

工作树里 `SurvivalPerkManager.cpp` 已带主人**未提交**的 PLANT_REGEN 平衡改动（25HP/5层 → 65HP/7层）。`git add <file>` 会把它和本词条改动一起暂存。执行前问主人：PLANT_REGEN 改动**先单独提交**，还是**与本词条同提交**。下面 Task 默认"先单独提交 PLANT_REGEN 再做本词条"，使每个 commit 单一可回溯。

---

### Task 0: 厘清 PLANT_REGEN 既存改动（按主人选择）

**Files:**
- 取决于主人：`PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp`（已修改，未暂存）

- [ ] **Step 1: 与主人确认 PLANT_REGEN 改动归属**

若主人选「先单独提交」：
```powershell
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies add -- PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies commit -m "balance(survival): 植物回血 25HP/5层 → 65HP/7层"
```
若主人选「同提交」：跳过，本文件改动在 Task 1 末一并提交。

---

### Task 1: 词条定义 + 聚合 getter + AutoTest 接入 + 闭合断言脚本

**Files:**
- Modify: `PlantVsZombies/Game/Perk/PerkType.h`
- Modify: `PlantVsZombies/Game/Perk/SurvivalPerkManager.h`
- Modify: `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp`
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Create: `autotest/scripts/smoke_perks_attackspeed.json`

- [ ] **Step 1: 写 smoke 脚本（先红）**

创建 `autotest/scripts/smoke_perks_attackspeed.json`：
```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "add_perk", "type": "PLANT_ATTACK_SPEED", "count": 2 },
    { "op": "dump_state", "name": "attackspeed.json" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 跑脚本验证它失败（红）**

Run（构建当前 exe 已存在即可，无需先 build；此步证明未知词条被拒）：
```powershell
Push-Location D:\PVZ\PlantsVsZombies\PlantVsZombies\build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks_attackspeed.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: 退出码 1，run.log 含 `未知词条类型: PLANT_ATTACK_SPEED`。

- [ ] **Step 3: PerkType.h 加枚举**

在 `enum class PerkType { ... }` 中，`PLANT_REGEN,` 之后、`COUNT` 之前插入：
```cpp
    PLANT_ATTACK_SPEED,    // 全体植物开火速度 +15%/层（最多 8 层）
```

- [ ] **Step 4: SurvivalPerkManager.cpp kPerks 表加行**

在 `kPerks[]` 数组中，`PLANT_REGEN` 那一行之后追加（顺序须与枚举一致，`static_assert` 会强制）：
```cpp
        { "PLANT_ATTACK_SPEED",   u8"植物攻速",     u8"每层使全体植物开火速度 +15%（最多 8 层）", 0.15f, 8, PerkCategory::PLANT_BUFF },
```

- [ ] **Step 5: SurvivalPerkManager.h 加 getter 声明**

在 `double GetPlantDamageMultiplier() const;` 一行下方加：
```cpp
    double GetPlantAttackSpeedMultiplier() const;  // 1 + 0.15 * stacks，0 层=1.0
```

- [ ] **Step 6: SurvivalPerkManager.cpp 加 getter 实现**

在 `GetPlantDamageMultiplier()` 实现函数之后加：
```cpp
double SurvivalPerkManager::GetPlantAttackSpeedMultiplier() const {
    return 1.0 + GetInfo(PerkType::PLANT_ATTACK_SPEED).perStack
               * GetStacks(PerkType::PLANT_ATTACK_SPEED);
}
```

- [ ] **Step 7: TestDriver.cpp 接入 kPerkNames**

把 `kPerkNames` 表里那行补上新词条（与 `PK(PLANT_REGEN)` 同段）：
```cpp
		PK(PLANT_DAMAGE_UP), PK(ZOMBIE_HEALTH_UP), PK(ZOMBIE_DAMAGE_RESIST),
		PK(ZOMBIE_DAMAGE_UP), PK(ZOMBIE_INVULN_HITS), PK(PLANT_REGEN),
		PK(PLANT_ATTACK_SPEED),
```

- [ ] **Step 8: TestDriver.cpp dump_state 加字段**

在 `stacks["PLANT_REGEN"] = ...;` 一行下方加：
```cpp
			stacks["PLANT_ATTACK_SPEED"]   = pm.GetStacks(PerkType::PLANT_ATTACK_SPEED);
```
在 `perks["plantRegenCapOn300"] = ...;` 一行下方加：
```cpp
			double asMult = pm.GetPlantAttackSpeedMultiplier();
			perks["plantAttackSpeedMult"] = asMult;                                  // 原始倍率（可见性）
			perks["shootIntervalOn1500"]  = static_cast<int>(1500.0 / asMult + 0.5); // 整数化：1.5s 间隔被缩到多少 ms（闭合断言用）
```

- [ ] **Step 9: 构建**

用上文「构建命令」跑 `cmake --build --preset clang-release`。
Expected: 0 warning / 0 error；`static_assert` 通过（证明枚举与 kPerks 表对齐）。

- [ ] **Step 10: 跑 smoke 验证它通过（绿）**

```powershell
Push-Location D:\PVZ\PlantsVsZombies\PlantVsZombies\build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks_attackspeed.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
然后 Read `build\clang-release\autotest\out\smoke_perks_attackspeed\attackspeed.json`，核对 `perks` 块：
- `stacks.PLANT_ATTACK_SPEED == 2`
- `plantAttackSpeedMult ≈ 1.30`
- `shootIntervalOn1500 == 1154`（= round(1500 / 1.30)，数学闭合）

- [ ] **Step 11: 回归既有 smoke**

```powershell
Push-Location D:\PVZ\PlantsVsZombies\PlantVsZombies\build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
Read 旧 dump，确认**既有键**（`plantDamageOn100`、`zombieHealthMult`、`plantRegenPerPulse` 等）值**逐位不变**（新增了 `shootIntervalOn1500` 等键属预期，0 层时 `shootIntervalOn1500==1500`、`plantAttackSpeedMult==1.0`，是中性默认的证明）。

- [ ] **Step 12: 提交**

```powershell
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies add -- PlantVsZombies/Game/Perk/PerkType.h PlantVsZombies/Game/Perk/SurvivalPerkManager.h PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp PlantVsZombies/Game/AutoTest/TestDriver.cpp autotest/scripts/smoke_perks_attackspeed.json
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies commit -m @'
feat(survival): 植物攻速词条 manager 层 + AutoTest 接入

新枚举 PLANT_ATTACK_SPEED(+15%/层,max8,PLANT_BUFF)；
GetPlantAttackSpeedMultiplier=1+0.15*层数(0层=1.0中性)；
dump_state 暴露 plantAttackSpeedMult + 整数化 shootIntervalOn1500
供闭合断言(2层→round(1500/1.30)=1154)。三处开火卡点接线见下一提交。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```
（注：若 Task 0 主人选「同提交」，此处 `git add` 同一文件会带上 PLANT_REGEN 改动，属预期。）

---

### Task 2: 三个开火卡点接线（间隔 + 动画同步缩放）

**Files:**
- Modify: `PlantVsZombies/Game/Plant/Shooter.cpp:40-51`（`PlantUpdate`）
- Modify: `PlantVsZombies/Game/Plant/Repeater.h:12-28`（`PlantUpdate`）
- Modify: `PlantVsZombies/Game/Plant/PuffShroom.cpp:22-33`（`PlantUpdate`）

- [ ] **Step 1: Shooter.cpp PlantUpdate 缩放**

把现有：
```cpp
void Shooter::PlantUpdate()
{
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f, 0.2f);
		}
	}
}
```
改为：
```cpp
void Shooter::PlantUpdate()
{
	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0，自动 no-op）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime / mult)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 64 帧 frame event 跟上更短间隔
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f * mult, 0.2f);
		}
	}
}
```

- [ ] **Step 2: Repeater.h PlantUpdate 缩放（两处动画 + 补发分支）**

把现有 `PlantUpdate()` 改为（在函数最上方取一次 `mult`，覆盖补发与主发两条路径）：
```cpp
	void PlantUpdate() override {
		// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0）。
		float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;

		if (mPendingSecondShot) {
			mPendingSecondShot = false;
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 2.0f * mult, 0.1f);
			return;
		}

		this->mShootTimer += DeltaTime::GetDeltaTime();
		if (this->mShootTimer >= this->mShootTime / mult)
		{
			if (HasZombieInRow())
			{
				mShootTimer = 0;
				mHeadAnim->PlayTrackOnce("anim_shooting", "", 1.9f * mult, 0.1f);
			}
		}
	}
```

- [ ] **Step 3: PuffShroom.cpp PlantUpdate 缩放**

把现有：
```cpp
void PuffShroom::PlantUpdate()
{
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			PlayTrackOnce("anim_shooting", "anim_idle", 1.5f, 0.2f);
		}
	}
}
```
改为：
```cpp
void PuffShroom::PlantUpdate()
{
	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime / mult)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 28 帧 frame event 跟上更短间隔
			PlayTrackOnce("anim_shooting", "anim_idle", 1.5f * mult, 0.2f);
		}
	}
}
```

- [ ] **Step 4: 构建**

用上文「构建命令」跑 `cmake --build --preset clang-release`。
Expected: 0 warning / 0 error。

- [ ] **Step 5: 行为佐证（满层攻速 → 肉眼更密集的豌豆）**

创建临时脚本 `autotest/scripts/tmp_attackspeed_behavior.json`：
```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "add_perk", "type": "PLANT_ATTACK_SPEED", "count": 8 },
    { "op": "set_sun", "value": 1000 },
    { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 2, "col": 0 },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 2 },
    { "op": "wait_seconds", "value": 2.0 },
    { "op": "screenshot", "name": "as_full.png" },
    { "op": "quit" }
  ]
}
```
跑它，Read `build\clang-release\autotest\out\tmp_attackspeed_behavior\as_full.png`：满层 2.2× 下，2 秒内本行应有**多颗豌豆在飞**（基准 1.5s 间隔 2 秒只发 ~1 颗）。确认无"打不出子弹"回归。看完删除临时脚本（不入库）。

> 校验僵尸类型名：若 `ZOMBIE_NORMAL` 在 `TestDriver.cpp` 名表里不是这个标识符，改用名表里的普通僵尸枚举名（如 `ZOMBIE_ZOMBIE`）。先 grep `TestDriver.cpp` 的僵尸名表确认。

- [ ] **Step 6: 提交**

```powershell
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies add -- PlantVsZombies/Game/Plant/Shooter.cpp PlantVsZombies/Game/Plant/Repeater.h PlantVsZombies/Game/Plant/PuffShroom.cpp
git -C D:\PVZ\PlantsVsZombies\PlantVsZombies commit -m @'
feat(survival): 植物攻速词条接线三个开火卡点

Shooter/Repeater/PuffShroom 的 PlantUpdate 各取一次攻速倍率：
间隔阈值 /mult 缩短 + 射击动画 clip-speed *mult 同步加快
(吐弹 frame event 跟上更短间隔，防高层数下动画重启吞子弹)。
mBoard 守卫防预览植物空指针；非生存关 mult 恒 1.0 自动失效。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## 自审（写完对照 spec）

- **Spec 覆盖**：枚举✓(T1S3) 表行✓(T1S4) getter✓(T1S5/6) 三卡点✓(T2S1-3) 存档(零改动,层数走现有Save/Load)✓ AutoTest✓(T1S7/8 + smoke) 回归✓(T1S11)。
- **占位符**：无 TBD/TODO；每个 code 步均给完整代码。
- **类型一致**：getter 名 `GetPlantAttackSpeedMultiplier` 在 T1S5/6 与 T2S1-3、T1S8 全一致；枚举名 `PLANT_ATTACK_SPEED` 全一致；dump 键 `shootIntervalOn1500`/`plantAttackSpeedMult` 在 T1S8 与 T1S10 一致。
- **数值闭合**：2 层 mult=1.30，round(1500/1.30)=1154；满层 8 层 mult=2.2，间隔 1.5/2.2=0.68s。
