# 魅惑僵尸（Mind-Controlled Zombie）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现僵尸侧魅惑机制全套：`StartMindControlled()` 触发入口 + `CanBeCharmed()` 豁免、CHARMED 碰撞层双向互啃、红光+翻身视觉、存读档恢复、AutoTest 验证（spec：`docs/superpowers/specs/2026-07-02-charmed-zombie-design.md`）。

**Architecture:** 复用碰撞系统预留的 seeker/target 钩子——魅惑僵尸 `layerMask=CHARMED` 后自动落入 `mRowOthers` 成为廉价 seeker，二分搜同行僵尸产 pair；两侧碰撞回调各自触发"我啃你"实现双向互啃。视觉走现成 overlay 管线 + Animator 新增 `SetFlipX`（两条绘制路径的矩阵构造处做绕支点水平镜像）。

**Tech Stack:** C++17 / CMake preset `clang-release`（警告验证）与 `msvc-debug` / AutoTest JSON 脚本闭环验证。

## Global Constraints

- 构建须先导入 VS 开发环境（Installer 目录先上 PATH 再 VsDevCmd，见 CLAUDE.md）；`clang-release` 必须 **0 warning 0 error**。
- 运行/AutoTest 工作目录 = `build\clang-release\`（勿用过时 `x64\Release`）。
- 源码 UTF-8（`/utf-8`），注释沿用仓库中文风格。
- 每个 Task 结束 commit（`git push` 不做——主人的活）。
- `CollisionSystem.h` **一行都不改**（钩子已备好；`CanCollide` 是双向 OR：`((a.layer & b.mask) | (b.layer & a.mask)) != 0`，普通僵尸掩码无需加 CHARMED 位）。
- 所有新 `.h` 文件无涉及；改动均在既有文件（pre-commit 头文件守卫钩子不会触发新问题）。

## 勘察结论速查（写码前必读）

| 事实 | 出处 |
|---|---|
| `CanCollide` 双向 OR → 魅惑侧 `collisionMask=ZOMBIE` 单边即可与普通僵尸(layer ZOMBIE)成对；植物(PLANT/ZOMBIE)、子弹(BULLET/ZOMBIE)、小推车(MOWER/ZOMBIE)与 CHARMED/ZOMBIE 两向都为 0 → 自动互不碰撞 | `CollisionSystem.h:58`；掩码清单：`Plant.cpp:42`、`Bullet.cpp:21`、`LawnMower.cpp:27`、`Zombie.cpp:45` |
| pair 产出时僵尸(ZOMBIE 层)恒放 a、seeker 放 b，`HandleCollisionEnter` 先 a 后 b；两侧回调独立触发各自 StartEat，次序无害 | `CollisionSystem.h:234` |
| 右边界移除**已存在**：`Zombie::Update` 每秒检查 `x > SCENE_WIDTH+65 → Die()`，魅惑向右走会自动被移除，无需新代码 | `Zombie.cpp:258` |
| `SetLocalScale`/`mLocalScaleX` 是写而不读的死字段，**不能**用于翻转 | `Animator.cpp:679`，无任何读点 |
| 两条绘制路径矩阵构造：instanced `rec.tA..tD/tx/ty`（`Animator.cpp:390-395`）、slow-path mat4（`Animator.cpp:504-509`），公式同布局 | GATE A 实测 instanced 路径覆盖 ~100% |
| overlay 管线被减速蓝光占用：`SetCooldown` 开蓝光、`Update` 倒计时归零关 overlay（`Zombie.cpp:231-243`）；魅惑时清掉减速即可独占 overlay，之后子弹打不中不会再有新减速 | `Zombie.cpp:334-350` |
| `TakeDamage` 内部含免伤词条(`mFreeHitsRemaining`)与 `ScaleTotalDamageToZombie` 缩放；互啃走 `TakeDamage` 正常链，词条对啃咬同样生效（语义自洽，接受） | `Zombie.cpp:409-444` |
| 樱桃炸弹**保留**能炸魅惑（原版 KillAllZombiesInRadius bit7=0xFF 含魅惑）；大喷菇原版 DoRowAreaDamage(20,2U) 无 bit7 → 豁免魅惑 | C# `Zombie.cs:3298-3305` |
| `StartEat/StopEat` 有三处子类 override：DoorZombie(两个)、Polevaulter(StopEat+自定义 onTriggerEnter lambda)、PaperZombie(两个)——僵尸分支须在每个 override 顶部委托回基类 | `DoorZombie.h:28`、`Polevaulter.h:20`、`PaperZombie.h:17` |
| 读档链：`CreateZombieWithID` → 字段 → `LoadProtectedData` → `RestoreAnimState` → `LoadExtraData` → `ZombieItemUpdate`，最后统一 `ValidateEatingState` | `GameInfoSaver.cpp:480-523` |
| 无魅惑音效资产（无 SOUND_MINDCONTROLLED），本次**不加音效**；将来做魅惑菇时主人供 wav 再接 | `ResourceKeys.h` Sounds 全查 |
| 原版魅惑行为参考 | C# `Zombie.cs`：StartMindControlled(6170)、FindZombieTarget(6998)、EffectedByDamage(2005) |

**测试形态说明**：本仓库无单元测试框架，测试循环 = 编译 + AutoTest 脚本断言（dump_state JSON + 截图 Read 目验）。每个 Task 的"测试"步骤即构建+脚本。

**构建命令（每个 Task 通用，简称"构建"）：**
```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --preset clang-release; cmake --build --preset clang-release
```
预期：0 warning 0 error。

**跑脚本（简称"跑 <脚本名>"）：**
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本名>.json -Seed 42
$LASTEXITCODE   # 必须为 0
Pop-Location
```
产物在 `build\clang-release\autotest\out\<脚本名>\`（PNG 用 Read 工具直接看，state*.json 看断言字段）。

---

### Task 1: CHARMED 层 + 魅惑核心状态机

**Files:**
- Modify: `PlantVsZombies/Game/ColliderComponent.h:19-27`（CollisionLayer 加 CHARMED）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`（声明 StartMindControlled / CanBeCharmed / WalkTrackAfterEat / ApplyCharmEffects）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（实现 + LoadProtectedData 恢复）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.h`（CanBeCharmed / WalkTrackAfterEat override）

**Interfaces:**
- Consumes: 现成 `mIsMindControlled`、`mCollider`、`mAnimator->EnableOverlayEffect/SetOverlayColor/SetExtraSpeedMultiplier`、`CollisionLayer` 命名空间。
- Produces（后续 Task 依赖，签名以此为准）:
  - `void Zombie::StartMindControlled();`（public 非虚，唯一魅惑入口）
  - `virtual bool Zombie::CanBeCharmed() const;`（public，基类 return true）
  - `virtual const char* Zombie::WalkTrackAfterEat() const;`（protected，基类 `"anim_walk2"`，Polevaulter `"anim_walk"`）
  - `void Zombie::ApplyCharmEffects();`（protected 非虚：掩码+红光；Task 4 在此追加翻身）
  - `CollisionLayer::CHARMED == 1 << 5`

- [ ] **Step 1: CollisionLayer 加 CHARMED**

`ColliderComponent.h` 的 `namespace CollisionLayer` 中 `COIN` 行后加：

```cpp
	constexpr uint16_t CHARMED = 1 << 5;   // 魅惑僵尸专用层：落入碰撞 seeker 桶（mRowOthers），见 CollisionSystem 拆分说明
```

- [ ] **Step 2: Zombie.h 声明**

public 段（`IsMindControlled()` 附近）加：

```cpp
	// 魅惑唯一入口：豁免(CanBeCharmed)/重复/垂死则 no-op。魅惑菇、AutoTest charm_zombie 都走这里。
	void StartMindControlled();
	// 子类豁免点：不可魅惑态（如撑杆 RUNNING/JUMPING）返回 false
	virtual bool CanBeCharmed() const { return true; }
```

protected 段加：

```cpp
	// 停止啃食后回落的走路轨道（基类随机双走路动画统一回 walk2；撑杆只有 anim_walk）
	virtual const char* WalkTrackAfterEat() const { return "anim_walk2"; }
	// 魅惑的派生状态（碰撞掩码+视觉）：StartMindControlled 与读档恢复共用
	void ApplyCharmEffects();
```

- [ ] **Step 3: Zombie.cpp 实现**

放在 `Zombie::SetCooldown` 之后：

```cpp
void Zombie::ApplyCharmEffects()
{
	// 碰撞：换 CHARMED 层 → 分桶自动落入 seeker 桶（mRowOthers），二分搜同行僵尸；
	// collisionMask 只含 ZOMBIE：不含 PLANT（不误啃植物）、不含 BULLET（子弹判不中）。
	// 植物/子弹/小推车的精确掩码与 CHARMED 两向都不相交 → 它们自动无视魅惑僵尸（CanCollide 是双向 OR）。
	if (mCollider) {
		mCollider->layerMask = CollisionLayer::CHARMED;
		mCollider->collisionMask = CollisionLayer::ZOMBIE;
	}
	// 视觉：红光。overlay 与减速蓝光共用——调用方已保证减速被清、且魅惑后子弹打不中不会再有新减速。
	if (mAnimator) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(255, 64, 64, 160);
	}
}

void Zombie::StartMindControlled()
{
	if (mIsMindControlled || mIsDying || !CanBeCharmed()) return;

	// 正在啃植物：先解除（平衡 mEaterCount）。掩码换掉后与植物不再成对，onTriggerExit 不会补触发。
	if (mIsEating && mEatPlantID != NULL_PLANT_ID && mBoard) {
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			plant->mEaterCount--;
		}
		mIsEating = false;
		mEatPlantID = NULL_PLANT_ID;
		PlayTrack(WalkTrackAfterEat(), 0.0f, 0.2f);
	}

	// 清减速：把 overlay 让给红光；魅惑后子弹打不中，减速不会再来
	if (mCooldownTimer > 0.0f) {
		mCooldownTimer = 0.0f;
		if (mAnimator) mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
	}

	mIsMindControlled = true;
	ApplyCharmEffects();
}
```

- [ ] **Step 4: 读档恢复派生状态**

`Zombie::LoadProtectedData` 中 `mIsMindControlled = j.value("isMindControlled", false);` 行后加：

```cpp
	// 魅惑是持久状态，掩码/红光是派生状态：读档按标志重建（存档只存布尔）
	if (mIsMindControlled) {
		ApplyCharmEffects();
	}
```

（注意此点在 SetCooldown 恢复之前执行、而 SetCooldown 只在 `cooldown > 0` 时开蓝光覆盖红光——但魅惑的档 `cooldownTimer` 恒为 0（StartMindControlled 已清），不会冲突。）

- [ ] **Step 5: Polevaulter 豁免 override**

`Polevaulter.h` public 段加：

```cpp
	// 撑杆跑/跳阶段不可魅惑（原版：跳过魅惑菇根本不吃它）；落地 WALKING 后可
	bool CanBeCharmed() const override { return mVaultState == VaultState::WALKING; }
```

protected 段加：

```cpp
	const char* WalkTrackAfterEat() const override { return "anim_walk"; }
```

- [ ] **Step 6: 构建**

跑上方通用构建命令。预期 0 warning 0 error（`-Wswitch`/`-Wunused` 等只有 clang 报，勿用 msvc 验）。

- [ ] **Step 7: 回归冒烟**

跑 `smoke_gameplay`、`demo_peashooter`。预期 `$LASTEXITCODE = 0`（未魅惑路径行为不变——本 Task 只加代码不改旧路径）。

- [ ] **Step 8: Commit**

```powershell
git add PlantVsZombies/Game/ColliderComponent.h PlantVsZombies/Game/Zombie/Zombie.h PlantVsZombies/Game/Zombie/Zombie.cpp PlantVsZombies/Game/Zombie/Polevaulter.h
git commit -m @'
feat(charm): CHARMED 碰撞层 + StartMindControlled/CanBeCharmed 魅惑核心状态机

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 2: AutoTest `charm_zombie` op + 基础验证脚本

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（新 op + dump_state 加 mindControlled 字段）
- Create: `autotest/scripts/smoke_charm_basic.json`
- Modify: `CLAUDE.md`（AutoTest 命令集清单补 `charm_zombie` 一词）

**Interfaces:**
- Consumes: Task 1 的 `Zombie::StartMindControlled()` / `IsMindControlled()`；既有 `kZombieNames`、`mEntityManager.GetAllZombieIDs()/GetZombie(id)`。
- Produces: AutoTest op `charm_zombie`：`{ "op":"charm_zombie", "row":0, "index":0 }`（row 省略或 -1 = 不过滤行；index = 过滤后按 ID 升序第 N 只，默认 0）；dump_state 僵尸对象新增 `"mindControlled": bool` 字段。

- [ ] **Step 1: 先写脚本（会失败——op 未实现）**

`autotest/scripts/smoke_charm_basic.json`：

```json
[
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "set_sun", "value": 1000 },
  { "op": "set_timescale", "value": 3.0 },

  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 500 },
  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 900 },
  { "op": "wait_frames", "value": 5 },

  { "op": "charm_zombie", "row": 0, "index": 0 },
  { "op": "wait_frames", "value": 5 },
  { "op": "dump_state", "name": "state_after_charm.json" },
  { "op": "screenshot", "name": "charm_red.png" },

  { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 0, "col": 0 },
  { "op": "wait_seconds", "value": 6 },
  { "op": "dump_state", "name": "state_after_shots.json" },

  { "op": "spawn_zombie", "type": "ZOMBIE_POLEVAULTER", "row": 2, "x": 900 },
  { "op": "wait_frames", "value": 5 },
  { "op": "charm_zombie", "row": 2, "index": 0 },
  { "op": "wait_frames", "value": 5 },
  { "op": "dump_state", "name": "state_polevaulter.json" },
  { "op": "quit" }
]
```

- [ ] **Step 2: 跑它验证失败**

跑 `smoke_charm_basic`。预期：**非 0 退出**，run.log 报未知 op `charm_zombie`（TestDriver 对未知 op 的既有失败路径）。

- [ ] **Step 3: 实现 charm_zombie op**

`TestDriver.cpp`，`show_zombie_hp` 块之后插入：

```cpp
	if (op == "charm_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("charm_zombie: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		const int row = cmd.value("row", -1);     // -1 = 不过滤行
		const int index = cmd.value("index", 0);  // 行过滤后按 ID 升序第 index 只
		int seen = 0;
		for (int id : board->mEntityManager.GetAllZombieIDs()) {
			Zombie* z = board->mEntityManager.GetZombie(id);
			if (!z) continue;
			if (row >= 0 && z->mRow != row) continue;
			if (seen++ == index) {
				z->StartMindControlled();   // 不可魅惑目标是 no-op：脚本用 dump_state 的 mindControlled 断言
				return true;
			}
		}
		Fail("charm_zombie: 未找到目标僵尸 (row=" + std::to_string(row) + ", index=" + std::to_string(index) + ")");
		return false;
	}
```

- [ ] **Step 4: dump_state 加 mindControlled 字段**

`TestDriver.cpp` dump_state 的 zombies push_back 初始化列表里 `{ "hasHead", z->HasHead() }` 行前加一行：

```cpp
				{ "mindControlled", z->IsMindControlled() },
```

- [ ] **Step 5: 构建 + 跑脚本 + 断言**

构建（0 warn），跑 `smoke_charm_basic`，`$LASTEXITCODE` = 0。然后 Read `build\clang-release\autotest\out\smoke_charm_basic\` 下产物，逐项断言：

1. `state_after_charm.json`：row 0 两只僵尸，index 0（x=500 先刷者，ID 最小）`mindControlled == true`，另一只 `false`。
2. `state_after_shots.json` 对比 `state_after_charm.json`：**魅惑僵尸 bodyHealth 不变(270)**（子弹穿过；豌豆掩码与 CHARMED 两向不相交，物理上不可能命中），**未魅惑那只 bodyHealth 下降**（豌豆越过前方魅惑者打中它）。两僵尸初始相距 400px、相向 ~10px/s×2，6 游戏秒窗口内不会接触——Task 3 互啃落地后重跑本脚本该断言依然成立。
3. `state_after_charm.json` vs `state_after_shots.json`：魅惑僵尸 `x` 增大（反向走）。
4. `state_polevaulter.json`：撑杆（RUNNING 态）`mindControlled == false`（CanBeCharmed 豁免生效）。
5. Read `charm_red.png`：魅惑僵尸带红色调、另一只正常色（目验）。

- [ ] **Step 6: CLAUDE.md 命令集清单补词**

CLAUDE.md AutoTest 节的命令集列表 `set_timescale` 后插入 `charm_zombie`。

- [ ] **Step 7: Commit**

```powershell
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp autotest/scripts/smoke_charm_basic.json CLAUDE.md
git commit -m @'
feat(autotest): charm_zombie op + dump_state mindControlled 字段 + 魅惑基础冒烟脚本

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 3: 双向互啃

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`（`mEatZombieID` 成员）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（StartEat/StopEat/EatTarget 僵尸分支、Die 清理、ValidateEatingState 兜底）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp`（onTriggerEnter lambda 放行僵尸；StopEat 顶部委托）
- Modify: `PlantVsZombies/Game/Zombie/DoorZombie.cpp`（StartEat/StopEat 顶部委托；ValidateEatingState 兜底）
- Modify: `PlantVsZombies/Game/Zombie/PaperZombie.cpp`（StartEat/StopEat 顶部委托；ValidateEatingState 兜底）
- Modify: `PlantVsZombies/Game/Zombie/Polevaulter.cpp`（ValidateEatingState 兜底）
- Create: `autotest/scripts/smoke_charm_bite.json`

**Interfaces:**
- Consumes: Task 1 的 `WalkTrackAfterEat()`；Task 2 的 `charm_zombie` op；`EntityManager::GetZombie(int)`、`NULL_ZOMBIE_ID`。
- Produces: `int Zombie::mEatZombieID`（protected，默认 `NULL_ZOMBIE_ID`，**不持久化**）；StartEat/StopEat/EatTarget 均能处理 `ObjectType::OBJECT_ZOMBIE`。

- [ ] **Step 1: 先写脚本（当前会失败——互啃不存在，血量不掉）**

`autotest/scripts/smoke_charm_bite.json`：

```json
[
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "set_timescale", "value": 5.0 },

  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 500 },
  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 700 },
  { "op": "wait_frames", "value": 5 },
  { "op": "charm_zombie", "row": 0, "index": 0 },

  { "op": "wait_seconds", "value": 10 },
  { "op": "dump_state", "name": "state_meet.json" },
  { "op": "wait_seconds", "value": 5 },
  { "op": "dump_state", "name": "state_biting.json" },
  { "op": "screenshot", "name": "charm_bite.png" },

  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 4, "x": 1000 },
  { "op": "wait_frames", "value": 5 },
  { "op": "charm_zombie", "row": 4, "index": 0 },
  { "op": "wait_seconds", "value": 25 },
  { "op": "dump_state", "name": "state_edge.json" },
  { "op": "quit" }
]
```

- [ ] **Step 2: 跑它记录失败基线**

跑 `smoke_charm_bite`（退出码 0——脚本本身能跑）。看 `state_biting.json`：**当前两僵尸 bodyHealth 都是 270 不掉**（互啃未实现），`state_edge.json` 里 row 4 僵尸仍在（右边界 Die 虽已有，x=1000 起步 25 游戏秒 × ~10px/s ≈ 走到 1250 > SCENE_WIDTH+65，若已消失则该断言直接过）。记录这份基线用于对比。

- [ ] **Step 3: Zombie.h 加成员**

`mEatPlantID` 行后加：

```cpp
	int mEatZombieID = NULL_ZOMBIE_ID;   // 互啃目标（魅惑↔普通）；不持久化——读档后由碰撞下一帧重建
```

- [ ] **Step 4: StartEat 僵尸分支**

`Zombie::StartEat` 的 `if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)` 之前插入：

```cpp
	if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
	{
		auto* target = dynamic_cast<Zombie*>(gameObject);
		if (!target) return;
		if (target->IsMindControlled() == mIsMindControlled) return;   // 同阵营不啃（魅惑×魅惑掩码本就不成对，此为语义兜底）
		if (target->mIsDying) return;
		if (target->mRow != this->mRow) return;
		if (mEatPlantID != NULL_PLANT_ID || mEatZombieID != NULL_ZOMBIE_ID) return;   // 一次只啃一个目标

		if (!mIsEating) {
			this->PlayTrack("anim_eat", 2.1f, 0.2f);
		}
		mIsEating = true;
		mEatZombieID = target->mZombieID;
		return;
	}
```

同函数植物分支的守卫改为双目标互斥（原 `if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;` 改成）：

```cpp
			if (mEatPlantID != NULL_PLANT_ID || mEatZombieID != NULL_ZOMBIE_ID || plant->mRow != this->mRow) return;	// 正在吃一个目标，那么不吃别的
```

- [ ] **Step 5: StopEat 僵尸分支**

`Zombie::StopEat` 的植物分支之前插入：

```cpp
	if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
	{
		auto* target = dynamic_cast<Zombie*>(gameObject);
		if (!target || target->mZombieID != mEatZombieID) return;

		if (mIsEating) {
			this->PlayTrack(WalkTrackAfterEat(), 0.0f, 0.2f);   // clip 清零，自动回落走速
		}
		mIsEating = false;
		mEatZombieID = NULL_ZOMBIE_ID;
		return;
	}
```

- [ ] **Step 6: EatTarget 僵尸分支**

`Zombie::EatTarget` 现有植物逻辑之前插入（保持原函数 `mHasHead` 语义——僵尸分支同样要求有头）：

```cpp
	if (mEatZombieID != NULL_ZOMBIE_ID && mHasHead)
	{
		Zombie* target = mBoard ? mBoard->mEntityManager.GetZombie(mEatZombieID) : nullptr;
		if (!target || target->mIsDying) {
			// 目标没了/垂死：正常由 onTriggerExit 收尾，这里兜底（含读档后目标失效）
			mIsEating = false;
			mEatZombieID = NULL_ZOMBIE_ID;
			PlayTrack(WalkTrackAfterEat(), 0.0f, 0.2f);
			return;
		}
		// 互啃走 TakeDamage 正常链（护盾→头盔→本体）：免伤/减伤词条对啃咬同样生效（语义自洽）；
		// 不过 ScaleZombieDamage——那是僵尸对植物的词条
		target->TakeDamage(mAttackDamage);
		if (GameRandom::Range(0, 1) == 0)
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.17f);
		else
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.17f);
		return;
	}
```

- [ ] **Step 7: Die 清理互啃状态**

`Zombie::Die` 的植物啃食清理块后加（无 mEaterCount 概念，只清字段防悬挂）：

```cpp
	mEatZombieID = NULL_ZOMBIE_ID;
```

- [ ] **Step 8: 三处子类 override 顶部委托 + Polevaulter lambda 放行**

`Polevaulter::SetupZombie` 的 onTriggerEnter lambda，`if (gameObject->GetObjectType() != ObjectType::OBJECT_PLANT) return;` 之前插入：

```cpp
				if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
					// 持杆奔跑不停下啃（跳跃语义优先，径直跑过魅惑僵尸）；跳后 WALKING 才互啃
					if (mVaultState != VaultState::RUNNING) StartEat(other);
					return;
				}
```

`Polevaulter::StopEat`、`DoorZombie::StartEat`、`DoorZombie::StopEat`、`PaperZombie::StartEat`、`PaperZombie::StopEat` 五个函数体第一行（各自 mIsPreview/mIsDying 守卫之后，若守卫在基类则直接函数体顶部）插入同一段：

```cpp
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
		Zombie::StartEat(other);   // StopEat 处写 Zombie::StopEat(other)
		return;
	}
```

（DoorZombie 的护盾亮手臂逻辑只在植物路径，魅惑铁门啃僵尸时手臂表现有小瑕疵——主人已豁免视觉瑕疵。）

- [ ] **Step 9: ValidateEatingState 兜底（4 处）**

存档只存 `isEating` 不存 `mEatZombieID` → 啃僵尸进行时存档、读档后 `mIsEating=true` 且两 ID 全空，`Update` 的 `if (mIsEating) return;` 会把僵尸永久冻结。在 `Zombie::ValidateEatingState`（`Zombie.cpp:619`）加 else-if：

```cpp
void Zombie::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk2", 0.0f, 0.3f);   // clip 清零，自动回落走速
		}
		else {
			plant->mEaterCount++;
		}
	}
	else if (mIsEating) {
		// mEatPlantID 为空却在啃：啃僵尸进行时存的档（mEatZombieID 不持久化）→ 回走路，碰撞下一帧重建互啃
		mIsEating = false;
		PlayTrack(WalkTrackAfterEat(), 0.0f, 0.3f);
	}
}
```

`Polevaulter::ValidateEatingState`（`Polevaulter.cpp:150`）、`DoorZombie::ValidateEatingState`（`DoorZombie.cpp:73`）、`PaperZombie::ValidateEatingState`（`PaperZombie.cpp:163`）各自加同构 else-if（动画轨道用各自现有 PlayTrack 参数风格；Polevaulter/DoorZombie 用 `WalkTrackAfterEat()`，PaperZombie 若有狂怒轨道逻辑则沿用其现有 StopEat 的轨道选择）。

- [ ] **Step 10: 构建 + 跑脚本 + 断言**

构建（0 warn）。跑 `smoke_charm_bite`，断言：

1. `state_biting.json` 对比 `state_meet.json`：row 0 两只僵尸 **bodyHealth 都下降**（双向互啃；50/口 × ~1 口/秒 × 5 游戏秒 ≈ 各掉 ≥100）。若 `state_meet` 时刻尚未相遇（x 差 > 碰撞箱和），断言改用 `state_biting` 里任一方 < 270 即可，并检查两者 `track == "anim_eat"`。
2. `state_edge.json`：row 4 无僵尸（魅惑者走出右边界被既有出屏逻辑移除），`zombieNumber` 相应减少。
3. Read `charm_bite.png` 目验：两僵尸面对面啃咬。

- [ ] **Step 11: 回归**

跑 `smoke_gameplay`、`repro_paper_death`（PaperZombie StartEat/StopEat 动过）、`demo_peashooter`。全部退出码 0。

- [ ] **Step 12: Commit**

```powershell
git add PlantVsZombies/Game/Zombie/Zombie.h PlantVsZombies/Game/Zombie/Zombie.cpp PlantVsZombies/Game/Zombie/Polevaulter.cpp PlantVsZombies/Game/Zombie/DoorZombie.cpp PlantVsZombies/Game/Zombie/PaperZombie.cpp autotest/scripts/smoke_charm_bite.json
git commit -m @'
feat(charm): 魅惑↔普通僵尸双向互啃（StartEat/StopEat/EatTarget 僵尸分支 + 读档兜底）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 4: Animator::SetFlipX 翻身

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h`（SetFlipX 声明 + 成员）
- Modify: `PlantVsZombies/Reanimation/Animator.cpp`（两条绘制路径应用镜像）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（ApplyCharmEffects 接线）

**Interfaces:**
- Consumes: Task 1 的 `ApplyCharmEffects()`（追加一行调用点）。
- Produces: `void Animator::SetFlipX(bool flip, float pivotX);`（public；绕动画局部 `x = pivotX` 竖直轴水平镜像，仅影响渲染，不动碰撞箱/影子/`_ground` 速度/帧事件）。

- [ ] **Step 1: Animator.h**

`SetLocalScale` 声明附近加：

```cpp
	/**
	 * 水平镜像（仅渲染）：绕动画局部 x = pivotX 的竖直轴翻转。
	 * 注意 SetLocalScale/mLocalScaleX 是历史死字段（绘制不读），翻转必须走本接口。
	 * 不影响碰撞箱/影子/_ground 轨道速度/帧事件（魅惑僵尸的移动方向由 ZombieMove 按 mIsMindControlled 处理）。
	 */
	void SetFlipX(bool flip, float pivotX = 0.0f);
```

private 成员区（`mLocalScaleY` 附近）加：

```cpp
	bool  mFlipX = false;
	float mFlipPivotX = 0.0f;
```

- [ ] **Step 2: Animator.cpp 实现 + 两路径应用**

实现（放 `SetLocalScale` 之后）：

```cpp
void Animator::SetFlipX(bool flip, float pivotX) {
	mFlipX = flip;
	mFlipPivotX = pivotX;
}
```

**instanced 快路径**（`DrawInternalInstanced`，`rec.ty = baseY + ty * Scale;` 行之后插入）：

```cpp
			// 水平镜像：世界 x' = 2*(baseX + pivot*Scale) - x → x 行取负、平移分量绕 pivot 反射。
			// glow/overlay 复制 rec，翻转天然一并生效。
			if (mFlipX) {
				rec.tA = -rec.tA;
				rec.tC = -rec.tC;
				rec.tx = baseX + (2.0f * mFlipPivotX - tx) * Scale;
			}
```

**slow-path**（`DrawInternal`，`glm::mat4 mat(...)` 构造之后插入）：

```cpp
			if (mFlipX) {
				mat[0][0] = -mat[0][0];
				mat[1][0] = -mat[1][0];
				mat[3][0] = baseX + (2.0f * mFlipPivotX - tx) * Scale;
			}
```

（已知豁免：slow-path 子动画（`mAttachedReanims`）的锚点不随翻转反射——僵尸无子动画、instanced 路径覆盖 ~100%（GATE A 实测），不处理。）

- [ ] **Step 3: ApplyCharmEffects 接线**

`Zombie::ApplyCharmEffects` 的 `SetOverlayColor` 行后加：

```cpp
		mAnimator->SetFlipX(true, 40.0f);   // 支点≈身体中线（动画局部坐标），截图目验后微调
```

- [ ] **Step 4: 构建 + 截图校支点**

构建（0 warn）。跑 `smoke_charm_basic`，Read `charm_red.png`：

1. 魅惑僵尸**面朝右**、红色调；未魅惑僵尸面朝左、正常色。
2. 对比魅惑僵尸脚下阴影与身体的相对位置：若身体明显偏离阴影（横向平移 > 半个身位），调整 Step 3 的支点值（±10 一档）重建再截，直至身体大致踩在阴影上。碰撞框小瑕疵主人已豁免（`-Debug` 模式可看 hitbox，不必对齐）。

- [ ] **Step 5: 回归**

跑 `smoke_gameplay`、`demo_peashooter`、`repro_blend_hand`（动了 Animator 绘制路径，务必跑动画回归）。全部退出码 0；`repro_blend_hand` 截图与既有基线目验无异常（未魅惑对象 mFlipX=false，两路径公式不变）。

- [ ] **Step 6: Commit**

```powershell
git add PlantVsZombies/Reanimation/Animator.h PlantVsZombies/Reanimation/Animator.cpp PlantVsZombies/Game/Zombie/Zombie.cpp
git commit -m @'
feat(charm): Animator::SetFlipX 渲染层水平镜像，魅惑僵尸翻身面朝右

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 5: 友方范围伤害豁免 + 收尾

**Files:**
- Modify: `PlantVsZombies/Game/Plant/FumeShroom.cpp:69`（跳过魅惑）
- Modify: `docs/superpowers/specs/2026-07-02-charmed-zombie-design.md`（三处实现修订注记）

**Interfaces:**
- Consumes: `Zombie::IsMindControlled()`。
- Produces: 无新接口；行为修订——大喷菇锥形伤害跳过魅惑僵尸；樱桃炸弹**保留**能炸魅惑（原版 bit7 语义：Jack 爆炸/樱桃 0xFF 含魅惑，DoRowAreaDamage(20,2U) 不含）。

- [ ] **Step 1: FumeShroom 豁免**

`FumeShroom.cpp` 的判定行改为：

```cpp
		if (dx >= 0 && dx <= kFumeReach && zombie->HasHead() && !zombie->IsMindControlled())
			zombie->TakeDamage(kFumeDamage, /*penetrateShield=*/true);
```

（樱桃 `Board::CreateBoom` **不改**：原版爆炸 damageRangeFlags=0xFF 含 bit7=也炸魅惑。子弹/小推车已由掩码天然豁免，Chomper/PotatoMine 已有既存跳过。）

- [ ] **Step 2: spec 修订注记**

spec `2026-07-02-charmed-zombie-design.md` 相应小节追加三条实现修订（保持文档与实现一致）：

1. §2：`CanCollide` 是双向 OR（`CollisionSystem.h:58`），普通僵尸 `collisionMask` **无需**加 CHARMED 位——魅惑侧单边掩码即成对；
2. §4：右边界移除复用既有出屏清理（`Zombie.cpp:258` `x > SCENE_WIDTH+65 → Die()`），未新增代码；
3. 新增小节"范围伤害"：大喷菇豁免魅惑（原版 2U 无 bit7）、樱桃保留炸魅惑（原版 0xFF 含 bit7）、魅惑音效因无资产暂缺，随魅惑菇补。

- [ ] **Step 3: 构建 + 定向验证**

构建（0 warn）。临时脚本验大喷菇豁免——创建 `autotest/scripts/smoke_charm_fume.json`：

```json
[
  { "op": "goto_level", "level": 10 },
  { "op": "choose_cards", "cards": ["PLANT_FUMESHROOM"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "set_sun", "value": 1000 },
  { "op": "set_timescale", "value": 3.0 },
  { "op": "plant", "type": "PLANT_FUMESHROOM", "row": 0, "col": 1 },
  { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 450 },
  { "op": "wait_frames", "value": 5 },
  { "op": "charm_zombie", "row": 0, "index": 0 },
  { "op": "wait_seconds", "value": 6 },
  { "op": "dump_state", "name": "state_fume.json" },
  { "op": "quit" }
]
```

断言：`state_fume.json` 魅惑僵尸 `bodyHealth == 270`（喷雾未伤到；注意夜晚关 10-19 蘑菇才醒）。该脚本保留入库。

- [ ] **Step 4: 全量回归**

跑全部既有 smoke：`smoke_gameplay`、`smoke_perks`、`smoke_perk_select`、`smoke_survival_spawn_round`、`demo_peashooter`、`repro_paper_death`、`smoke_charm_basic`、`smoke_charm_bite`、`smoke_charm_fume`。全部退出码 0。

- [ ] **Step 5: Commit**

```powershell
git add PlantVsZombies/Game/Plant/FumeShroom.cpp autotest/scripts/smoke_charm_fume.json docs/superpowers/specs/2026-07-02-charmed-zombie-design.md
git commit -m @'
feat(charm): 大喷菇范围伤害豁免魅惑僵尸（原版 bit7 语义）+ spec 实现修订注记

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

## 计划外事项（显式不做）

- 魅惑菇 HypnoShroom 本体（触发点：将来 `Zombie::EatTarget` 植物分支判 `plant->mPlantType == PlantType::PLANT_HYPNOSHROOM` → `plant->Die()` + `StartMindControlled()`；依赖本计划全部交付）。
- 魅惑音效（无资产）。
- 存档持久化 `mEatZombieID`（读档由碰撞下一帧重建，Task 3 Step 9 已兜底冻结 bug）。
- 小推车碾魅惑、魅惑僵尸的 additive 高光、slow-path 子动画翻转锚点。
