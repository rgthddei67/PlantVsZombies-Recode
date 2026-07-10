# 舞王僵尸(Zombie_Jackson) + 伴舞僵尸(Zombie_dancer) 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现舞王僵尸（入场月球漫步→打响指召唤十字4伴舞→全队随全局节拍齐舞，死伴舞按拍补位）与伴舞僵尸（出土升起→齐舞），含断手/断头/掉头粒子/魅惑/存档。

**Architecture:** Board 全局逻辑帧计数器推导"舞蹈节拍帧"（0~22 循环），舞王/伴舞都从它映射动画轨道 → 全队天然同步、领队死亡不影响。舞王四阶段状态机（DANCING_IN→SNAPPING→HOLD→DANCING），召唤经 `Board::CreateZombie`，僵尸间关联用 EntityManager 整型 ID（死亡自动失效）。断手无材质替换（MJ 版 reanim 无残肢轨道，藏小臂+手保大臂）。

**Tech Stack:** C++17 / 自研引擎（Animator reanim 骨骼动画、nlohmann/json 存档、AutoTest JSON 驱动验证）。

**Spec:** `docs/superpowers/specs/2026-07-10-dancer-zombie-design.md`（主人已批准；帧号 146/100、90+99、46+59 为主人指定，不得擅改）。

## Global Constraints

- 源码根在 `PlantVsZombies/` 子目录（本文件所有源码路径省略该前缀时已写全）；**资源没有独立源目录**，`gamedata.json`、`particles/config/*.xml` 必须同时改两份：`build/clang-release/resources/…` 和 `build/msvc-debug/resources/…`。
- 构建（先导入 VS 环境，一次导入本会话内有效）：
  ```powershell
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
  cmake --build --preset clang-release
  ```
- AutoTest 运行（工作目录必须是 exe 所在目录）：
  ```powershell
  Push-Location build\clang-release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本>.json -Seed 42
  $LASTEXITCODE   # 0=过 1=命令失败/超时 100=脚本解析失败
  Pop-Location
  ```
  产物在 `build\clang-release\autotest\out\<脚本名>\`（截图无 .png 扩展也可能有——用 Read 直接看；run.log 是权威记录）。
- 新 .h 首行 `#pragma once`（pre-commit 钩子强制）；中文注释 UTF-8。
- 每个 Task 完成即 `git commit`（不 push）；提交信息中文，结尾加 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- **主人指定、不可改动的帧事件帧号**：舞王 Die@146、EatTarget@90+99；伴舞 Die@100、EatTarget@46+59。
- reanim 事实（实测）：Jackson 时间线 0–147（moonwalk 0–26 / point 27–36 / walk 46–66 / armraise 67–77 / eat 78–111 / death 112–147），头=`anim_head1`+`anim_head2`(下巴)+`anim_hair`（无 tongue），外臂=`Zombie_Jackson_outerarm_upper`+`Zombie_outerarm_lower`+`Zombie_outerarm_hand`；dancer 时间线 0–101（walk 0–20 / armraise 21–31 / eat 32–64 / death 65–101），额外 `anim_earing`，部件轨道名复用 Jackson 前缀。**两个 reanim 都没有残肢(_bone/upper2)轨道**。
- 帧事件的帧号是**全时间线绝对帧**；一个帧号只会在包含它的剪辑段播放时触发（如 36 只在 anim_point 27–36 内经过）。
- 视觉调参常量（clip 速度、gamedata offset/scale、召唤间距）允许按截图微调，微调后在提交信息注明；帧号与状态机结构不许动。

---

### Task 1: Board 节拍时钟（mBoardFrame + GetDanceBeatFrame + 存档）

**Files:**
- Modify: `PlantVsZombies/Game/Board.h`（public 字段区，`mZombieNumber` 附近）
- Modify: `PlantVsZombies/Game/Board.cpp`（`Board::Update`，655 行附近）
- Modify: `PlantVsZombies/GameInfoSaver.cpp`（保存 ~170 行 `currentWave` 处；加载 ~397 行处）

**Interfaces:**
- Consumes: 无
- Produces: `int Board::mBoardFrame`（public，逻辑步计数）；`int Board::GetDanceBeatFrame() const`（返回 0~22 节拍帧，每拍 12 逻辑步=0.2s@60Hz，等价原版 100Hz 的 20cs/拍）。Task 2/3 的两只僵尸调用 `mBoard->GetDanceBeatFrame()`。

- [ ] **Step 1: Board.h 加字段与方法**

在 `Board.h` 的 public 区（`int mZombieNumber = 0;` 之后）加：

```cpp
	// 全局逻辑帧计数（固定步长 60Hz，每逻辑步 +1，入存档）。
	// 用途：舞王/伴舞全队共舞节拍源——所有舞者从同一计数推导动作，不互相通信也能整齐划一。
	int mBoardFrame = 0;

	// 舞蹈节拍帧 0~22 循环，每拍 12 逻辑步(0.2s)，等价原版 100Hz 的 20cs/拍、23 拍一循环。
	// 0~11 = 舞步段(anim_walk)，12~22 = 举手段(anim_armraise)；补充召唤只在节拍==12 触发。
	int GetDanceBeatFrame() const { return mBoardFrame % (12 * 23) / 12; }
```

- [ ] **Step 2: Board.cpp Update 里递增**

`Board::Update()`（655 行）改为：

```cpp
void Board::Update()
{
	mBoardFrame++;
	CleanupExpiredObjects();
	UpdateLevel();
}
```

- [ ] **Step 3: GameInfoSaver 存读**

保存侧（~170 行 `j["currentWave"] = board->mCurrentWave;` 的下一行）加：

```cpp
	j["boardFrame"] = board->mBoardFrame;
```

加载侧（~397 行 `board->mCurrentWave = j.value("currentWave", 0);` 的下一行）加：

```cpp
	board->mBoardFrame = j.value("boardFrame", 0);
```

- [ ] **Step 4: 构建 + 回归**

Run: `cmake --build --preset clang-release`（预期 0 error 0 warning）
Run（cwd=build\clang-release）: `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json -Seed 42`
Expected: `$LASTEXITCODE == 0`

- [ ] **Step 5: Commit**

```powershell
git add PlantVsZombies/Game/Board.h PlantVsZombies/Game/Board.cpp PlantVsZombies/GameInfoSaver.cpp
git commit -m "Board加全局节拍时钟mBoardFrame：舞王全队齐舞的同步源(0~22拍,12步/拍),入存档"
```

---

### Task 2: 伴舞僵尸 BackupDancerZombie（类 + 注册链 + 掉头粒子 + smoke）

**Files:**
- Create: `PlantVsZombies/Game/Zombie/BackupDancerZombie.h`
- Create: `PlantVsZombies/Game/Zombie/BackupDancerZombie.cpp`
- Modify: `PlantVsZombies/Game/Zombie/ZombieType.h`（枚举移动）
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`（include + RegisterZombie，~195 行 FOOTBALL 之后）
- Modify: `build/clang-release/resources/gamedata.json` 与 `build/msvc-debug/resources/gamedata.json`
- Create: `build/clang-release/resources/particles/config/ZombieBackupDancerHeadOff.xml` 与 msvc-debug 同路径
- Create: `autotest/scripts/smoke_backupdancer.json`（测试脚本）

**Interfaces:**
- Consumes: `Board::GetDanceBeatFrame()`（Task 1）
- Produces: `class BackupDancerZombie : public Zombie`，public 字段 `int mLeaderID`（Task 3 舞王召唤后写入）；`enum class BackupPhase { RISING, DANCING }`。ZombieType 枚举值 `ZOMBIE_BACKUP_DANCER` 移入 `[0, NUM_ZOMBIE_TYPES)`。

- [ ] **Step 1: 先写测试脚本（会失败）**

Create `autotest/scripts/smoke_backupdancer.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "set_sun", "value": 1000 },
    { "op": "spawn_zombie", "type": "ZOMBIE_BACKUP_DANCER", "row": 2, "x": 750 },
    { "op": "wait_frames", "value": 30 },
    { "op": "screenshot", "name": "rising_mid" },
    { "op": "assert_state", "path": "zombies.0.type", "equals": "ZOMBIE_BACKUP_DANCER" },
    { "op": "wait_seconds", "value": 3 },
    { "op": "screenshot", "name": "dancing" },
    { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 2, "col": 0 },
    { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 2, "col": 1 },
    { "op": "set_timescale", "value": 3.0 },
    { "op": "wait_seconds", "value": 7 },
    { "op": "dump_state", "name": "after_arm.json" },
    { "op": "screenshot", "name": "arm_off" },
    { "op": "assert_state", "path": "zombies.0.hasArm", "equals": false },
    { "op": "assert_state", "path": "zombies.0.armVisible", "equals": false },
    { "op": "wait_seconds", "value": 6 },
    { "op": "screenshot", "name": "head_off" },
    { "op": "assert_state", "path": "zombies.0.hasHead", "equals": false },
    { "op": "wait_seconds", "value": 8 },
    { "op": "dump_state", "name": "final.json" },
    { "op": "quit" }
  ]
}
```

伤害节奏参考：HP270，断臂阈值 ≤180（豌豆 20 伤×2 株 ≈ 5 秒内 90 伤），断头 ≤90，无头流血 ~3s 死。若实跑时 assert 时点不符（僵尸走位/啃食差异），只调 `wait_seconds` 数值，不改断言本身。

- [ ] **Step 2: 跑脚本确认失败**

Run（cwd=build\clang-release）: `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_backupdancer.json -Seed 42`
Expected: `$LASTEXITCODE == 1`，run.log 含 spawn 失败（工厂未注册/类型不可用）。

- [ ] **Step 3: 枚举移动**

`ZombieType.h`：把 `ZOMBIE_DANCER,`、`ZOMBIE_BACKUP_DANCER,` 两行从 `NUM_ZOMBIE_TYPES,` 之后剪切到之前（`ZOMBIE_FOOTBALL,` 之后）。两个一起移（Task 3 需要 DANCER，分两次移动会让枚举值抖动两回）。移动后立即做 Step 4 的 gamedata 补条目，否则启动校验 `[0,NUM_ZOMBIE_TYPES)` 全员有 JSON 条目会失败退出（exit -6）。

```cpp
	ZOMBIE_DOOR,
	ZOMBIE_FOOTBALL,
	ZOMBIE_DANCER,
	ZOMBIE_BACKUP_DANCER,

	// ↓ 哨兵：置于已实现僵尸之后，……（原注释保留）
	NUM_ZOMBIE_TYPES,
```

- [ ] **Step 4: gamedata.json 两个 preset 各加两条**

在 `"zombies"` 对象的 `"ZOMBIE_FOOTBALL"` 条目后加（**5 字段缺一不可**；两份文件都要）：

```json
  "ZOMBIE_DANCER": {
   "weight": 0,
   "appearWave": 8,
   "survivalRound": 4,
   "offset": [ -50.0, -85.0 ],
   "scale": 1.0
  },
  "ZOMBIE_BACKUP_DANCER": {
   "weight": 0,
   "appearWave": 8,
   "survivalRound": 4,
   "offset": [ -50.0, -85.0 ],
   "scale": 1.0
  }
```

`weight:0` = 波次/生存永远抽不中（伴舞只能被舞王召唤或 AutoTest 直接 spawn）。**ZOMBIE_DANCER 在本 Task 暂设 weight:0**——它此刻已进哨兵前但工厂要 Task 3 才注册，非 0 权重会让生存随机抽取抽到空工厂；Task 3 注册后再升到 1200。offset/scale 先抄普通僵尸，Step 10 截图后微调。

- [ ] **Step 5: 写 BackupDancerZombie.h**

```cpp
#pragma once
#ifndef _BACKUPDANCERZOMBIE_H_
#define _BACKUPDANCERZOMBIE_H_

#include "Zombie.h"

// 伴舞僵尸：只能被舞王召唤（gamedata weight=0）。出土升起 → 随 Board 全局节拍齐舞。
// 领队关系：mLeaderID 指向舞王；领队死亡/自己被魅惑后即为无主（照常跳舞前进，原版行为）。
class BackupDancerZombie : public Zombie {
public:
	using Zombie::Zombie;

	enum class BackupPhase {
		RISING,		// 出土升起中（无粒子无音效——主人指定；不移动不啃食）
		DANCING		// 随全局节拍跳舞前进
	};

	int mLeaderID = NULL_ZOMBIE_ID;	// 舞王 ID（舞王召唤后写入）

	void ZombieUpdate(float scaledTime) override;
	void StartEat(ColliderComponent* other) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	// Zombie_dancer.reanim 无 anim_walk2：稳态“走路”=按全局节拍选 anim_walk/anim_armraise
	void PlayWalkAnimation(float blendTime) override;

	void UpdateDanceTrack(float blendTime);

	BackupPhase mPhase = BackupPhase::RISING;
	float mRiseTimer = 0.0f;		// 已升起时长，达 kRiseDuration 落地
	int mLastBeatBucket = -1;		// 上次应用的节拍段（0=walk 1=armraise），-1=强制刷新
	bool mCharmHandled = false;		// 魅惑边沿标志：置位时清 mLeaderID 脱队
	float mBaseOffsetY = 0.0f;		// gamedata offset 原始 y（升起位移叠加其上）
};

#endif
```

- [ ] **Step 6: 写 BackupDancerZombie.cpp**

```cpp
#include "BackupDancerZombie.h"
#include "../Board.h"
#include "../../ParticleSystem/ParticleSystem.h"

namespace {
	constexpr float kRiseDuration = 2.5f;    // 出土升起耗时（原版 150cs）
	constexpr float kRiseDepth = 145.0f;     // 出生下沉深度（原版 altitude -145）
	constexpr float kDanceAnimSpeed = 1.2f;  // 全队统一动画速度：覆盖 Start() 的随机 1.1~1.4，否则齐舞散拍
	constexpr float kArmraiseClip = 1.8f;    // 举手段 clip（原版 rate18；按截图手感可微调）
}

void BackupDancerZombie::SetupZombie()
{
	mBodyHealth = 270;
	mBodyMaxHealth = 270;
	SetAnimationSpeed(kDanceAnimSpeed);

	if (mIsPreview) { PlayTrack("anim_walk"); return; }

	// 帧号为主人指定：Die@100（anim_death 65~101 内），EatTarget@46/59（anim_eat 32~64 内）
	mAnimator->AddFrameEvent(100, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(46, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(59, [this]() { this->EatTarget(); }, true);

	// 出生沉入地下，升起期间照常随节拍跳舞（引擎无“暂停骨架”安全路径：
	// 若定格动画，升起中被打死/断头会卡在冻结帧无法播 anim_death）。
	mBaseOffsetY = mVisualOffset.y;
	mVisualOffset.y = mBaseOffsetY + kRiseDepth;
	mPhase = BackupPhase::RISING;
	UpdateDanceTrack(0.0f);
}

void BackupDancerZombie::ZombieUpdate(float scaledTime)
{
	// 魅惑边沿：脱离领队（领队随后检测到空位会补人）。基类 StartMindControlled 非虚，故在此检测。
	if (mIsMindControlled && !mCharmHandled) {
		mCharmHandled = true;
		mLeaderID = NULL_ZOMBIE_ID;
	}

	if (mPhase == BackupPhase::RISING) {
		mRiseTimer += scaledTime;
		const float t = mRiseTimer / kRiseDuration;
		if (t >= 1.0f) {
			mVisualOffset.y = mBaseOffsetY;
			mPhase = BackupPhase::DANCING;
		}
		else {
			mVisualOffset.y = mBaseOffsetY + kRiseDepth * (1.0f - t);
		}
	}

	UpdateDanceTrack(0.3f);
}

void BackupDancerZombie::UpdateDanceTrack(float blendTime)
{
	if (!mBoard || mIsDying) return;
	const int bucket = (mBoard->GetDanceBeatFrame() >= 12) ? 1 : 0;
	if (bucket == mLastBeatBucket) return;
	mLastBeatBucket = bucket;
	if (bucket == 1)
		PlayTrack("anim_armraise", kArmraiseClip, blendTime);
	else
		PlayTrack("anim_walk", 0.0f, blendTime);
}

void BackupDancerZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (mPhase == BackupPhase::RISING) return;	// 升起中不推进
	Zombie::ZombieMove(scaledDelta, transform);
}

void BackupDancerZombie::PlayWalkAnimation(float blendTime)
{
	// 啃完回走路/读档恢复的统一入口：回到当前节拍对应的舞蹈轨道
	mLastBeatBucket = -1;
	if (mBoard && mBoard->GetDanceBeatFrame() >= 12)
		PlayTrack("anim_armraise", kArmraiseClip, blendTime);
	else
		PlayTrack("anim_walk", 0.0f, blendTime);
}

void BackupDancerZombie::StartEat(ColliderComponent* other)
{
	if (mPhase == BackupPhase::RISING) return;	// 升起中不啃食
	Zombie::StartEat(other);
}

void BackupDancerZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	mAnimator->SetTrackVisible("anim_earing", false);	// 耳环挂在头上，随头走
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieBackupDancerHeadOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void BackupDancerZombie::ArmDrop()
{
	if (!mHasArm) return;
	// MJ 版 reanim 无残肢轨道：只藏小臂+手，大臂原样保留当残端；无材质替换、无粒子（主人指定）
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void BackupDancerZombie::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_hair", false);
		mAnimator->SetTrackVisible("anim_earing", false);
	}
}

void BackupDancerZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["riseTimer"] = mRiseTimer;
	j["leaderID"] = mLeaderID;
	j["charmHandled"] = mCharmHandled;
}

void BackupDancerZombie::LoadExtraData(const nlohmann::json& j)
{
	mPhase = static_cast<BackupPhase>(j.value("phase", 0));
	mRiseTimer = j.value("riseTimer", 0.0f);
	mLeaderID = j.value("leaderID", NULL_ZOMBIE_ID);
	mCharmHandled = j.value("charmHandled", false);
	// RISING 的 mVisualOffset.y 由 ZombieUpdate 每帧按 mRiseTimer 重算，但 mBaseOffsetY 只在
	// SetupZombie 里记录（SetupZombie 已把 offset.y 抬了 kRiseDepth，读档新建僵尸同样走过 Setup，
	// mBaseOffsetY 已正确）。
	if (mIsEating) return;
	mLastBeatBucket = -1;	// 下帧按当前节拍重刷轨道（覆盖 RestoreAnimState 的帧位=重新入拍，预期行为）
}
```

- [ ] **Step 7: 注册工厂**

`GameDataManager.cpp` 顶部 include 区加：

```cpp
#include "../Zombie/BackupDancerZombie.h"
```

僵尸注册区 `ZOMBIE_FOOTBALL` 条目之后加：

```cpp
	// 伴舞僵尸：gamedata weight=0，只能被舞王召唤（或 AutoTest spawn_zombie 直造）
	RegisterZombie(ZombieType::ZOMBIE_BACKUP_DANCER, "ZOMBIE_BACKUP_DANCER",
		AnimationType::ANIM_DANCERWITH_ZOMBIE,
		"ZombieDancer", &MakeZombie<BackupDancerZombie>);
```

（animName `"ZombieDancer"` 必须与 resources.xml 的 `<Reanimation name="ZombieDancer">` 一致，已预置。）

- [ ] **Step 8: 掉头粒子 XML（两个 preset）**

Create `build/clang-release/resources/particles/config/ZombieBackupDancerHeadOff.xml`（照抄 ZombieHeadOff.xml，改 Name 与 Image）：

```xml
<Emitter>
  <Name>ZombieBackupDancerHeadOff</Name>
  <SpawnMinActive>1</SpawnMinActive>
  <SpawnMaxLaunched>1</SpawnMaxLaunched>
  <ParticleDuration>1.0</ParticleDuration>
  <ParticleAlpha>1 0.9</ParticleAlpha>
  <ParticleScale>0.75 0.75</ParticleScale>
  <LaunchSpeed>[60 100]</LaunchSpeed>
  <RandomLaunchSpin>1</RandomLaunchSpin>
  <ParticleGravity>140</ParticleGravity>
  <ParticleSpinSpeed>[-5 5]</ParticleSpinSpeed>
  <SystemDuration>1.1</SystemDuration>
  <Image>PARTICLE_ZOMBIEBACKUPDANCERHEAD</Image>
  <Field>
    <FieldType>Position</FieldType>
    <X>-10</X>
    <Y>-50</Y>
  </Field>
</Emitter>
```

同内容复制到 `build/msvc-debug/resources/particles/config/ZombieBackupDancerHeadOff.xml`。
（Image 键 `PARTICLE_ZOMBIEBACKUPDANCERHEAD` 是 `ZombieBackupDancerHead.png` 的标准派生键，贴图已在 resources.xml 声明。）

- [ ] **Step 9: 构建**

Run: `cmake --build --preset clang-release`
Expected: 0 error 0 warning（GLOB_RECURSE 自动收编新 .cpp，改了 CMake 输入无需手动 reconfigure——加新文件后若未被编译，先 `cmake --preset clang-release` 重新 configure）。

- [ ] **Step 10: 跑脚本至通过 + 截图目验**

Run（cwd=build\clang-release）: `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_backupdancer.json -Seed 42`
Expected: `$LASTEXITCODE == 0`。
用 Read 看 `autotest/out/smoke_backupdancer/` 截图逐张目验：
- `rising_mid`：伴舞下半身在草坪线以下（升起中，穿模程度记录给主人裁决）
- `dancing`：完整站立、播 walk 或 armraise
- `arm_off`：小臂+手消失、大臂还在、无手臂飞出粒子
- `head_off`：头/下巴/头发/耳环全消失、有头颅粒子飞出
- 站位/影子不对 → 微调 gamedata offset（两份 json 同步改）
`final.json` 里不再有 `ZOMBIE_BACKUP_DANCER`（已流血死透）。注意：脚本总时长约 24 游戏秒 > 关卡第一波 20 秒倒计时，final.json 里出现波次普通僵尸属正常，勿断言 zombies 为空。

- [ ] **Step 11: Commit**

```powershell
git add PlantVsZombies/Game/Zombie/BackupDancerZombie.h PlantVsZombies/Game/Zombie/BackupDancerZombie.cpp PlantVsZombies/Game/Zombie/ZombieType.h PlantVsZombies/Game/Plant/GameDataManager.cpp build/clang-release/resources/gamedata.json build/msvc-debug/resources/gamedata.json build/clang-release/resources/particles/config/ZombieBackupDancerHeadOff.xml build/msvc-debug/resources/particles/config/ZombieBackupDancerHeadOff.xml autotest/scripts/smoke_backupdancer.json
git commit -m "新增伴舞僵尸：出土升起+节拍齐舞+断手藏两轨無粒子+断头四轨(含耳环)专属头粒子(帧事件100/46/59主人指定)"
```

注意：gamedata.json/粒子XML 在 build/ 下——确认 .gitignore 是否放行（此前 gamedata.json 曾入库，同样路径应可 add；若被 ignore 用 `git add -f`）。

---

### Task 3: 舞王僵尸 DancerZombie（状态机 + 十字召唤 + 补位 + smoke）

**Files:**
- Create: `PlantVsZombies/Game/Zombie/DancerZombie.h`
- Create: `PlantVsZombies/Game/Zombie/DancerZombie.cpp`
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`（include + RegisterZombie）
- Create: `build/clang-release/resources/particles/config/ZombieJacksonHeadOff.xml` 与 msvc-debug 同路径
- Create: `autotest/scripts/smoke_dancer_summon.json`

**Interfaces:**
- Consumes: `Board::GetDanceBeatFrame()`（Task 1）；`BackupDancerZombie` 与其 public `mLeaderID`（Task 2）；`Board::CreateZombie(ZombieType, int row, float x)`；`EntityManager::GetZombie(int)`（死亡返回 nullptr）；`Zombie::mZombieID`。
- Produces: `class DancerZombie : public Zombie`（`enum class DancerPhase { DANCING_IN, SNAPPING, HOLD, DANCING }`）。无下游消费者。

- [ ] **Step 1: 先写测试脚本（会失败）**

Create `autotest/scripts/smoke_dancer_summon.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "spawn_zombie", "type": "ZOMBIE_DANCER", "row": 2, "x": 600 },
    { "op": "wait_frames", "value": 30 },
    { "op": "screenshot", "name": "moonwalk_in" },
    { "op": "assert_state", "path": "zombies.0.type", "equals": "ZOMBIE_DANCER" },
    { "op": "wait_seconds", "value": 3 },
    { "op": "screenshot", "name": "after_snap" },
    { "op": "dump_state", "name": "troupe.json" },
    { "op": "assert_state", "path": "zombies.4.type", "equals": "ZOMBIE_BACKUP_DANCER" },
    { "op": "wait_seconds", "value": 4 },
    { "op": "screenshot", "name": "troupe_dancing" },
    { "op": "dump_state", "name": "troupe_late.json" },
    { "op": "quit" }
  ]
}
```

时间轴参考：DANCING_IN 2.0~2.12s → anim_point ~0.35s → 召唤。3 秒处应已有 1 舞王+4 伴舞（`zombies.4` 存在即 5 只）。

- [ ] **Step 2: 跑脚本确认失败**

Run（cwd=build\clang-release）: `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_dancer_summon.json -Seed 42`
Expected: `$LASTEXITCODE == 1`（ZOMBIE_DANCER 工厂未注册）。

- [ ] **Step 3: 写 DancerZombie.h**

```cpp
#pragma once
#ifndef _DANCERZOMBIE_H_
#define _DANCERZOMBIE_H_

#include "Zombie.h"

// 舞王僵尸(MJ版)：月球漫步入场 → 打响指(anim_point)召唤十字 4 伴舞 → 定身跳舞 2s →
// 随全局节拍齐舞前进；伴舞阵亡后在节拍==12 时重新打响指补位（有头且未越过 kDanceLimitX 才补）。
class DancerZombie : public Zombie {
public:
	using Zombie::Zombie;

	enum class DancerPhase {
		DANCING_IN,	// 入场月球漫步（计时；开吃会提前结束）
		SNAPPING,	// 播 anim_point，第 36 帧事件触发召唤
		HOLD,		// 召唤后定身跳舞 2s（不移动）
		DANCING		// 节拍齐舞 + 前进 + 缺位补召
	};

	void ZombieUpdate(float scaledTime) override;
	void StartEat(ColliderComponent* other) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	// Zombie_Jackson.reanim 无 anim_walk2：稳态“走路”按阶段选 moonwalk/point/节拍舞
	void PlayWalkAnimation(float blendTime) override;

private:
	void SummonBackupDancers();
	bool NeedsMoreBackupDancers() const;
	void UpdateDanceTrack(float blendTime);

	DancerPhase mPhase = DancerPhase::DANCING_IN;
	float mPhaseTimer = 0.0f;
	int mFollowerID[4] = { NULL_ZOMBIE_ID, NULL_ZOMBIE_ID, NULL_ZOMBIE_ID, NULL_ZOMBIE_ID };
	int mLastBeatBucket = -1;	// 0=walk 1=armraise，-1=强制刷新
	bool mCharmHandled = false;	// 魅惑边沿标志：置位时清空 mFollowerID（放弃旧伴舞，原版行为）
};

#endif
```

- [ ] **Step 4: 写 DancerZombie.cpp**

```cpp
#include "DancerZombie.h"
#include "BackupDancerZombie.h"
#include "../Board.h"
#include "../../ParticleSystem/ParticleSystem.h"

namespace {
	constexpr float kDancingInDuration = 2.0f;	// 入场月球漫步基础时长（原版 200cs），另加 0~0.12s 随机
	constexpr float kHoldDuration = 2.0f;		// 响指后定身跳舞时长（原版 200cs）
	constexpr float kDanceLimitX = 700.0f;		// 越过此 x 不再补充召唤（原版 dance limit）
	constexpr float kMoonwalkClip = 2.0f;		// 原版 rate24（12fps 的 2 倍）；截图手感可微调
	constexpr float kPointClip = 2.0f;
	constexpr float kArmraiseClip = 1.8f;		// 与伴舞 kArmraiseClip 保持一致，否则举手段散拍
	constexpr float kDanceAnimSpeed = 1.2f;		// 与伴舞 kDanceAnimSpeed 保持一致
	constexpr float kSummonFrontMinX = 130.0f;	// 舞王 x<130 时不放前方位（原版防出左屏）
	constexpr float kSummonSideDist = 100.0f;	// 同行前/后伴舞与舞王的 x 距离
	constexpr int   kPointDoneFrame = 36;		// anim_point 段(27~36)末帧：响指完成点
}

void DancerZombie::SetupZombie()
{
	mBodyHealth = 500;
	mBodyMaxHealth = 500;
	SetAnimationSpeed(kDanceAnimSpeed);

	if (mIsPreview) { PlayTrack("anim_moonwalk", kMoonwalkClip); return; }

	// 帧号为主人指定：Die@146（anim_death 112~147 内），EatTarget@90/99（anim_eat 78~111 内）
	mAnimator->AddFrameEvent(146, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(90, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(99, [this]() { this->EatTarget(); }, true);

	// anim_point 播毕 → 召唤 + 进 HOLD。帧 36 只存在于 point 段内，其他轨道不会经过；
	// repeating=true + phase 守卫：补位召唤会多次经过此帧。
	mAnimator->AddFrameEvent(kPointDoneFrame, [this]() {
		if (mPhase != DancerPhase::SNAPPING) return;
		SummonBackupDancers();
		mPhase = DancerPhase::HOLD;
		mPhaseTimer = kHoldDuration;
		mLastBeatBucket = -1;
	}, true);

	mPhase = DancerPhase::DANCING_IN;
	mPhaseTimer = kDancingInDuration + GameRandom::Range(0, 12) * 0.01f;
	PlayTrack("anim_moonwalk", kMoonwalkClip);
}

void DancerZombie::ZombieUpdate(float scaledTime)
{
	// 魅惑边沿：放弃旧伴舞（它们继续以原阵营跳舞）；此后新召唤的伴舞在 SummonBackupDancers 里继承魅惑
	if (mIsMindControlled && !mCharmHandled) {
		mCharmHandled = true;
		for (int i = 0; i < 4; ++i) mFollowerID[i] = NULL_ZOMBIE_ID;
	}

	switch (mPhase) {
	case DancerPhase::DANCING_IN:
		mPhaseTimer -= scaledTime;
		if (mPhaseTimer <= 0.0f && mHasHead) {
			mPhase = DancerPhase::SNAPPING;
			PlayTrack("anim_point", kPointClip, 0.3f);
		}
		break;
	case DancerPhase::SNAPPING:
		// 等 anim_point 第 36 帧事件收尾
		break;
	case DancerPhase::HOLD:
		mPhaseTimer -= scaledTime;
		UpdateDanceTrack(0.3f);
		if (mPhaseTimer <= 0.0f) {
			mPhase = DancerPhase::DANCING;
		}
		break;
	case DancerPhase::DANCING:
		UpdateDanceTrack(0.3f);
		// 补位：只在节拍 12（walk→armraise 切拍点，原版编排）、有头、未越界、确实缺人时再打响指
		if (mHasHead && mBoard && mBoard->GetDanceBeatFrame() == 12
			&& GetPosition().x < kDanceLimitX && NeedsMoreBackupDancers()) {
			mPhase = DancerPhase::SNAPPING;
			PlayTrack("anim_point", kPointClip, 0.3f);
		}
		break;
	}
}

void DancerZombie::UpdateDanceTrack(float blendTime)
{
	if (!mBoard || mIsDying) return;
	const int bucket = (mBoard->GetDanceBeatFrame() >= 12) ? 1 : 0;
	if (bucket == mLastBeatBucket) return;
	mLastBeatBucket = bucket;
	if (bucket == 1)
		PlayTrack("anim_armraise", kArmraiseClip, blendTime);
	else
		PlayTrack("anim_walk", 0.0f, blendTime);
}

void DancerZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	// 打响指/定身跳舞阶段不移动；入场(moonwalk)与齐舞阶段按轨道 _ground 速度正常推进
	if (mPhase == DancerPhase::SNAPPING || mPhase == DancerPhase::HOLD) return;
	Zombie::ZombieMove(scaledDelta, transform);
}

void DancerZombie::PlayWalkAnimation(float blendTime)
{
	switch (mPhase) {
	case DancerPhase::DANCING_IN:
		PlayTrack("anim_moonwalk", kMoonwalkClip, blendTime);
		return;
	case DancerPhase::SNAPPING:
		PlayTrack("anim_point", kPointClip, blendTime);
		return;
	default:
		mLastBeatBucket = -1;
		if (mBoard && mBoard->GetDanceBeatFrame() >= 12)
			PlayTrack("anim_armraise", kArmraiseClip, blendTime);
		else
			PlayTrack("anim_walk", 0.0f, blendTime);
	}
}

void DancerZombie::StartEat(ColliderComponent* other)
{
	Zombie::StartEat(other);
	// 原版：入场月球漫步中被迫开吃 → 啃完立即打响指（计时清零；啃食期间状态机本就暂停）
	if (mIsEating && mPhase == DancerPhase::DANCING_IN) {
		mPhaseTimer = 0.0f;
	}
}

void DancerZombie::SummonBackupDancers()
{
	if (!mBoard || !mHasHead) return;
	const float x = GetPosition().x;
	const struct { int row; float posX; } slots[4] = {
		{ mRow - 1, x },                      // 上一行同列
		{ mRow + 1, x },                      // 下一行同列
		{ mRow,     x - kSummonSideDist },    // 同行前方（太靠左跳过）
		{ mRow,     x + kSummonSideDist },    // 同行后方
	};
	for (int i = 0; i < 4; ++i) {
		if (mBoard->mEntityManager.GetZombie(mFollowerID[i])) continue;	// 该位还活着
		mFollowerID[i] = NULL_ZOMBIE_ID;
		if (slots[i].row < 0 || slots[i].row >= mBoard->mRows) continue;
		if (i == 2 && x < kSummonFrontMinX) continue;
		Zombie* summoned = mBoard->CreateZombie(ZombieType::ZOMBIE_BACKUP_DANCER,
			slots[i].row, slots[i].posX);
		if (!summoned) continue;
		mFollowerID[i] = summoned->mZombieID;
		if (auto* backup = dynamic_cast<BackupDancerZombie*>(summoned)) {
			backup->mLeaderID = mZombieID;
		}
		if (mIsMindControlled) summoned->StartMindControlled();	// 魅惑领队的新伴舞继承阵营
	}
}

bool DancerZombie::NeedsMoreBackupDancers() const
{
	if (!mBoard) return false;
	for (int i = 0; i < 4; ++i) {
		// 永久不可用的位不算缺：行越界（上/下行）与太靠左的前方位
		//（原版对前方位无此豁免，会在 x<130 时无限重打响指——这里补上防死循环）
		if (i == 0 && mRow - 1 < 0) continue;
		if (i == 1 && mRow + 1 >= mBoard->mRows) continue;
		if (i == 2 && GetPosition().x < kSummonFrontMinX) continue;
		if (mBoard->mEntityManager.GetZombie(mFollowerID[i]) == nullptr) return true;
	}
	return false;
}

void DancerZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieJacksonHeadOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void DancerZombie::ArmDrop()
{
	if (!mHasArm) return;
	// 同伴舞：藏小臂+手、保大臂，无材质替换、无粒子（主人指定；MJ 版 reanim 无残肢轨道）
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void DancerZombie::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
}

void DancerZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["phaseTimer"] = mPhaseTimer;
	j["charmHandled"] = mCharmHandled;
	j["followers"] = nlohmann::json::array({
		mFollowerID[0], mFollowerID[1], mFollowerID[2], mFollowerID[3] });
}

void DancerZombie::LoadExtraData(const nlohmann::json& j)
{
	mPhase = static_cast<DancerPhase>(j.value("phase", 0));
	mPhaseTimer = j.value("phaseTimer", 0.0f);
	mCharmHandled = j.value("charmHandled", false);
	if (j.contains("followers") && j["followers"].is_array()) {
		const auto& arr = j["followers"];
		for (int i = 0; i < 4 && i < static_cast<int>(arr.size()); ++i) {
			mFollowerID[i] = arr[i].get<int>();
		}
	}
	// 读档僵尸 ID 经 CreateZombieWithID 保值，followers 交叉引用安全（死者 GetZombie 返回 null 即空位）
	if (mIsEating) return;
	if (mPhase == DancerPhase::DANCING_IN || mPhase == DancerPhase::SNAPPING) return;
	// HOLD/DANCING：下帧按节拍重刷轨道（RestoreAnimState 的帧位被重新入拍覆盖，预期行为）
	mLastBeatBucket = -1;
}
```

- [ ] **Step 5: 注册工厂**

`GameDataManager.cpp` include 区加：

```cpp
#include "../Zombie/DancerZombie.h"
```

僵尸注册区（伴舞条目之前，保持枚举顺序）加：

```cpp
	// 舞王僵尸（MJ版）：入场后打响指召唤十字 4 伴舞，死伴舞按节拍补位
	RegisterZombie(ZombieType::ZOMBIE_DANCER, "ZOMBIE_DANCER",
		AnimationType::ANIM_DANCE_ZOMBIE,
		"ZombieJackson", &MakeZombie<DancerZombie>);
```

同时把两份 gamedata.json 里 `ZOMBIE_DANCER` 的 `"weight": 0` 改为 `"weight": 1200`（工厂已注册，解除 Task 2 的临时封印，舞王正式进出怪池）。

- [ ] **Step 6: 掉头粒子 XML（两个 preset）**

Create `build/clang-release/resources/particles/config/ZombieJacksonHeadOff.xml`：

```xml
<Emitter>
  <Name>ZombieJacksonHeadOff</Name>
  <SpawnMinActive>1</SpawnMinActive>
  <SpawnMaxLaunched>1</SpawnMaxLaunched>
  <ParticleDuration>1.0</ParticleDuration>
  <ParticleAlpha>1 0.9</ParticleAlpha>
  <ParticleScale>0.75 0.75</ParticleScale>
  <LaunchSpeed>[60 100]</LaunchSpeed>
  <RandomLaunchSpin>1</RandomLaunchSpin>
  <ParticleGravity>140</ParticleGravity>
  <ParticleSpinSpeed>[-5 5]</ParticleSpinSpeed>
  <SystemDuration>1.1</SystemDuration>
  <Image>PARTICLE_ZOMBIEDANCERHEAD</Image>
  <Field>
    <FieldType>Position</FieldType>
    <X>-10</X>
    <Y>-50</Y>
  </Field>
</Emitter>
```

同内容复制到 `build/msvc-debug/resources/particles/config/` 下同名文件。
（`PARTICLE_ZOMBIEDANCERHEAD` = `ZombieDancerHead.png`（MJ 舞王头）的标准派生键；`_disco` 后缀两张贴图是新版 disco 用，不使用。）

- [ ] **Step 7: 构建**

Run: `cmake --build --preset clang-release`
Expected: 0 error 0 warning。

- [ ] **Step 8: 跑脚本至通过 + 截图目验**

Run（cwd=build\clang-release）: `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_dancer_summon.json -Seed 42`
Expected: `$LASTEXITCODE == 0`。
截图目验：
- `moonwalk_in`：舞王月球漫步姿态（倒滑步）
- `after_snap`：十字 4 伴舞已出现/升起中（row1/row3 同 x、row2 前后 ±100）
- `troupe_dancing`：五只同拍（同为 walk 姿势或同为举手姿势——两张截图之间跨 4 秒>2.4s 一个 bucket，抓到不同 bucket 也正常，看单张内是否整齐）
`troupe.json` 里 `zombies` 长度 5、`troupe_late.json` 各僵尸 `track` 字段应全员一致（`anim_walk` 或 `anim_armraise`）。

- [ ] **Step 9: 补位行为快验（同脚本追加或临时脚本）**

在 `smoke_dancer_summon.json` 的 `troupe_late` dump 之后临时验证一次补位：种 4 株豌豆打死 row2 前方伴舞后等 ~6s 再 dump，确认 zombies 数量回到 5（舞王重新打响指补位）。验证通过后此段可精简保留（保留断言：最后 dump 的 zombies 长度 ≥5——用 `assert_state` `zombies.4.type` == "ZOMBIE_BACKUP_DANCER"）。

- [ ] **Step 10: Commit**

```powershell
git add PlantVsZombies/Game/Zombie/DancerZombie.h PlantVsZombies/Game/Zombie/DancerZombie.cpp PlantVsZombies/Game/Plant/GameDataManager.cpp build/clang-release/resources/gamedata.json build/msvc-debug/resources/gamedata.json build/clang-release/resources/particles/config/ZombieJacksonHeadOff.xml build/msvc-debug/resources/particles/config/ZombieJacksonHeadOff.xml autotest/scripts/smoke_dancer_summon.json
git commit -m "新增舞王僵尸：四阶段状态机(月球漫步→响指→定身→节拍齐舞)+十字召唤/节拍12补位(帧事件146/90/99主人指定)"
```

---

### Task 4: 魅惑交互 smoke + 全量回归 + 交主人验收

**Files:**
- Create: `autotest/scripts/smoke_dancer_charm.json`
- （无源码新改动；本 Task 是行为验证与回归，若发现 bug 就地修复并入本 Task 提交）

**Interfaces:**
- Consumes: Task 2/3 的两个僵尸类（魅惑边沿逻辑已在各自 ZombieUpdate 内实现）；AutoTest `charm_zombie` 命令。
- Produces: 全套可重跑的验收脚本。

- [ ] **Step 1: 写魅惑脚本**

Create `autotest/scripts/smoke_dancer_charm.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "spawn_zombie", "type": "ZOMBIE_DANCER", "row": 2, "x": 600 },
    { "op": "wait_seconds", "value": 4 },
    { "op": "dump_state", "name": "before_charm.json" },

    { "op": "charm_zombie", "row": 1, "index": 0 },
    { "op": "wait_frames", "value": 10 },
    { "op": "dump_state", "name": "after_charm_backup.json" },
    { "op": "wait_seconds", "value": 6 },
    { "op": "dump_state", "name": "after_refill.json" },
    { "op": "screenshot", "name": "refill" },

    { "op": "charm_zombie", "row": 2, "index": 0 },
    { "op": "wait_seconds", "value": 8 },
    { "op": "dump_state", "name": "after_charm_leader.json" },
    { "op": "screenshot", "name": "charmed_troupe" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 跑通并核对 dump（-Seed 42 固定，时序确定可复现）**

- `before_charm.json`：5 只；row1 只有 1 只伴舞（index 0 必中它）。
- `after_charm_backup.json`：该伴舞 `mindControlled == true`（脱队）。
- `after_refill.json`：zombies 总数 6（舞王在节拍 12 补召了 row1 新伴舞；被魅惑那只仍在场）。
- `charm_zombie row2 index0`：row2 有舞王+前后伴舞共 3 只——先在 `after_refill.json` 里确认 index 0 是舞王（按 x 值识别：舞王 x≈600 且 type==ZOMBIE_DANCER；若 index 顺序不是舞王，改 index 号重跑）。
- `after_charm_leader.json`：舞王 `mindControlled == true`；等待期内它清空了旧伴舞数组并在节拍 12 重新召唤，新伴舞 `mindControlled == true`（zombies 里应出现 ≥2 只 mindControlled 的 BACKUP_DANCER）；旧伴舞仍 `mindControlled == false` 继续前进。
- 核对通过后，把关键结论固化为 assert_state（至少：`after_charm_backup` 的魅惑标志、`after_refill` 的 `zombies.5.type == "ZOMBIE_BACKUP_DANCER"`）。

- [ ] **Step 3: 全量回归**

依次跑并要求全部 exit 0：
`smoke_backupdancer.json`、`smoke_dancer_summon.json`、`smoke_dancer_charm.json`、`smoke_gameplay.json`、`smoke_charm_basic.json`、`smoke_hypnoshroom.json`。
再次确认 `cmake --build --preset clang-release` 无 warning（本仓库 warning-zero 验证只认 clang preset）。

- [ ] **Step 4: 存档路径静态自查（AutoTest 短路存档，跑不到）**

对照清单逐条核对代码（不跑）：
- Dancer/Backup 的 `SaveExtraData/LoadExtraData` 字段名一一对应；
- `GameInfoSaver` 里 `boardFrame` 存读都在；
- `ZombieItemUpdate` 对 `mHasArm/mHasHead` 的轨道处理与 `ArmDrop/HeadDrop` 完全一致；
- `LoadExtraData` 首行有 `if (mIsEating) return;` 之前不做动画播放。
核对结果写进提交信息。真机读档验证留给主人（存一局有全队+断肢的档再读）。

- [ ] **Step 5: Commit + 汇报主人**

```powershell
git add autotest/scripts/smoke_dancer_charm.json
git commit -m "舞王/伴舞魅惑smoke：伴舞魅惑脱队+领队补位、领队魅惑清旧伴舞+新召继承阵营；全量回归通过"
```

向主人汇报：贴 `troupe_dancing`/`refill`/`charmed_troupe`/`rising_mid`/`arm_off`/`head_off` 截图，请主人真机试玩（重点：升起穿模观感、齐舞节奏、断手视觉、读档），主人拍板后按其反馈调参。

---

## 已知留白（主人已批准的 v1 简化，勿实现）

聚光灯、disco 循环音乐、伴舞出土粒子与音效、冻结/黄油全队节拍冻结、升起期地面裁剪。

## 调参入口速查（主人看效果后可能让改）

| 参数 | 位置 | 现值 |
|---|---|---|
| 齐舞动画速度 | 两 .cpp 的 `kDanceAnimSpeed` | 1.2 |
| 举手段 clip | 两 .cpp 的 `kArmraiseClip`（须两处同改） | 1.8 |
| 月球漫步/响指 clip | DancerZombie.cpp `kMoonwalkClip/kPointClip` | 2.0 |
| 升起时长/深度 | BackupDancerZombie.cpp `kRiseDuration/kRiseDepth` | 2.5s / 145 |
| 补位界限 | DancerZombie.cpp `kDanceLimitX` | 700 |
| 站位/缩放 | 两份 gamedata.json 的 offset/scale | -50,-85 / 1.0 |
| 节拍速度 | Board.h `GetDanceBeatFrame` 的 12 步/拍 | 0.2s/拍 |
