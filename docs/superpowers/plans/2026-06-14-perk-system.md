# 生存模式词条系统 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为生存模式实现一个统一的 `SurvivalPerkManager`，承载 3 个试水词条（植物伤害+10%、僵尸血量+20%、僵尸免伤+5%）的层数/描述/数值效果/存档，并接入三个伤害-血量钟点。词条选择 UI 不在本次范围。

**Architecture:** `Board` 以值成员持有 `SurvivalPerkManager`。管理器对外暴露聚合修正量（空词条时为中性值，令非生存关/未获取词条自动 no-op）。三个钟点经 `mBoard->GetPerkManager()` 读取：僵尸血量并入 `Board::GetZombieHpMultiplier()`、植物伤害在子弹命中与 `CreateBoom` 处放大、僵尸免伤在 `Zombie::TakeDamage` 首行缩减。存档按字符串 key 跟随 `survivalRound`。

**Tech Stack:** C++17、CMake + vcpkg（`GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集新 .cpp，无需改构建文件）、nlohmann/json、SDL2/Vulkan。

> **本仓库的"测试"现实（重要）：** 项目**无单元测试框架**。CLAUDE.md 定义的验证闭环是「`clang-release` 构建零警告 + `-AutoTest` 脚本跑通 + Read `dump_state` / 截图核对数值」。因此本计划的"测试"步骤落地为：(a) 构建零警告；(b) `smoke_perks.json` 跑通后 Read `perks.json` 断言确切数值。词条管理器的纯算术（有逻辑风险的部分）经 `dump_state` 确定性验证；三个钟点的插入是机械单行改动，经构建+审查（+可选 `mShowZombieHP` 截图）验证。存档读写在 AutoTest 模式被短路，无法经 AutoTest 往返验证，靠"镜像现有 `survivalRound` 范式 + 代码审查"保证，主人可在 VS 里手动验证生存存读档。

**构建与运行命令（每个验证步骤复用）：**

```powershell
# 构建（VsDevCmd 环境，新 shell 每次重导）
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --build --preset clang-release 2>&1 | Select-Object -Last 12

# 跑 AutoTest 脚本
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks.json -Seed 42
$LASTEXITCODE   # 0=成功
Pop-Location
# 产物：build\clang-release\autotest\out\smoke_perks\perks.json  ← 用 Read 核对
```

---

## 文件结构

| 文件 | 责任 | 动作 |
|---|---|---|
| `PlantVsZombies/Game/Perk/PerkType.h` | `PerkType` 枚举 + `PerkInfo` 元数据结构 | 创建 |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.h` | 管理器类声明（轻量头，json 用前置声明） | 创建 |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp` | 管理器实现 + 静态元数据表 | 创建 |
| `PlantVsZombies/Game/Board.h` | 持有管理器 + 公有访问器；`GetZombieHpMultiplier` 并入血量词条 | 修改 |
| `PlantVsZombies/Game/Board.cpp` | 生存初始化 `Clear()`；`CreateBoom` 入口放大植物伤害 | 修改 |
| `PlantVsZombies/Game/Bullet/Bullet.cpp` | 子弹命中放大植物伤害 | 修改 |
| `PlantVsZombies/Game/Zombie/Zombie.cpp` | `TakeDamage` 首行缩减（僵尸免伤） | 修改 |
| `PlantVsZombies/GameInfoSaver.cpp` | 存档/读档词条 | 修改 |
| `PlantVsZombies/Game/AutoTest/TestDriver.cpp` | `add_perk` 命令 + `dump_state` 词条块 | 修改 |
| `autotest/scripts/smoke_perks.json` | 词条冒烟脚本 | 创建 |

---

## Task 1: 词条数据模型（PerkType.h + SurvivalPerkManager 类）

**Files:**
- Create: `PlantVsZombies/Game/Perk/PerkType.h`
- Create: `PlantVsZombies/Game/Perk/SurvivalPerkManager.h`
- Create: `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp`

- [ ] **Step 1: 创建 `PerkType.h`**

```cpp
#pragma once

// 生存模式词条类型。新增词条：在此加枚举项（COUNT 之前），并在 SurvivalPerkManager.cpp
// 的 kPerks 表对应位置补一行（static_assert 会强制一一对应）。
enum class PerkType {
    PLANT_DAMAGE_UP,        // 全体植物伤害 +10%/层
    ZOMBIE_HEALTH_UP,       // 僵尸血量 +20%/层
    ZOMBIE_DAMAGE_RESIST,   // 僵尸免伤 +5%/层（最高 50%）
    COUNT
};

struct PerkInfo {
    const char* key;        // 存档稳定键名（不随 enum 顺序变）
    const char* nameZh;     // 显示名（UI 用）
    const char* descZh;     // 每层效果描述（UI 用）
    float       perStack;   // 每层数值（0.10 / 0.20 / 0.05）
    int         maxStacks;  // 每词条独立上限（=1 即一次性词条）
};
```

- [ ] **Step 2: 创建 `SurvivalPerkManager.h`（轻量头：json 用前置声明，避免拖慢 Board.h 编译扇出）**

```cpp
#pragma once
#include <array>
#include <nlohmann/json_fwd.hpp>   // 仅前置声明 nlohmann::json，Save/Load 取引用参数足够
#include "PerkType.h"

// 生存模式词条聚合管理器。Board 以值成员持有；非生存关恒空，聚合 getter 返回中性值。
class SurvivalPerkManager {
public:
    bool AddPerk(PerkType type);          // +1 层，到 maxStacks 截断；已满返回 false
    int  GetStacks(PerkType type) const;
    void Clear();                         // 进新生存局时清空

    // 聚合效果——空词条时为中性值（乘法单位元 / 原值），三处钟点自动 no-op
    double GetPlantDamageMultiplier() const;       // 1 + 0.10 * stacks
    double GetZombieHealthMultiplier() const;      // 1 + 0.20 * stacks
    double GetZombieDamageTakenMultiplier() const; // 1 - 0.05 * stacks，夹底 0.5

    int ScalePlantDamage(int base) const;          // round(base * 植物伤害倍率)，base>=1 时结果>=1
    int ScaleDamageToZombie(int base) const;       // round(base * 免伤倍率)，base>=1 时结果>=1

    static const PerkInfo& GetInfo(PerkType type); // 静态元数据表（UI 用）

    void Save(nlohmann::json& j) const;            // 仅写 stacks>0 的项，按 key 字符串
    void Load(const nlohmann::json& j);            // 缺 key→0；越界→夹回 [0,maxStacks]

private:
    std::array<int, static_cast<size_t>(PerkType::COUNT)> mStacks{};  // 全 0 初始
};
```

- [ ] **Step 3: 创建 `SurvivalPerkManager.cpp`**

```cpp
#include "SurvivalPerkManager.h"
#include <nlohmann/json.hpp>

namespace {
    // 顺序必须与 PerkType 枚举一一对应（static_assert 强制）
    const PerkInfo kPerks[] = {
        { "PLANT_DAMAGE_UP",      u8"全体植物伤害", u8"每层使全体植物伤害 +10%",            0.10f, 10 },
        { "ZOMBIE_HEALTH_UP",     u8"僵尸血量",     u8"每层使僵尸血量 +20%",                0.20f, 10 },
        { "ZOMBIE_DAMAGE_RESIST", u8"僵尸免伤",     u8"每层使僵尸受到伤害 -5%（最高 50%）", 0.05f, 10 },
    };
    static_assert(sizeof(kPerks) / sizeof(kPerks[0]) == static_cast<size_t>(PerkType::COUNT),
                  "kPerks 必须与 PerkType 一一对应");

    int RoundScale(int base, double mult) {
        if (base <= 0) return base;
        int r = static_cast<int>(static_cast<double>(base) * mult + 0.5);
        return r < 1 ? 1 : r;   // 防 50% 免伤把 1 点伤害抹成 0
    }
}

const PerkInfo& SurvivalPerkManager::GetInfo(PerkType type) {
    return kPerks[static_cast<size_t>(type)];
}

bool SurvivalPerkManager::AddPerk(PerkType type) {
    int& s = mStacks[static_cast<size_t>(type)];
    if (s >= GetInfo(type).maxStacks) return false;
    ++s;
    return true;
}

int SurvivalPerkManager::GetStacks(PerkType type) const {
    return mStacks[static_cast<size_t>(type)];
}

void SurvivalPerkManager::Clear() {
    mStacks.fill(0);
}

double SurvivalPerkManager::GetPlantDamageMultiplier() const {
    return 1.0 + GetInfo(PerkType::PLANT_DAMAGE_UP).perStack
               * GetStacks(PerkType::PLANT_DAMAGE_UP);
}

double SurvivalPerkManager::GetZombieHealthMultiplier() const {
    return 1.0 + GetInfo(PerkType::ZOMBIE_HEALTH_UP).perStack
               * GetStacks(PerkType::ZOMBIE_HEALTH_UP);
}

double SurvivalPerkManager::GetZombieDamageTakenMultiplier() const {
    double reduction = GetInfo(PerkType::ZOMBIE_DAMAGE_RESIST).perStack
                     * GetStacks(PerkType::ZOMBIE_DAMAGE_RESIST);
    double mult = 1.0 - reduction;
    return mult < 0.5 ? 0.5 : mult;   // maxStacks=10 → 恰好 0.5；夹底防越界
}

int SurvivalPerkManager::ScalePlantDamage(int base) const {
    return RoundScale(base, GetPlantDamageMultiplier());
}

int SurvivalPerkManager::ScaleDamageToZombie(int base) const {
    return RoundScale(base, GetZombieDamageTakenMultiplier());
}

void SurvivalPerkManager::Save(nlohmann::json& j) const {
    for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
        if (mStacks[i] > 0) j[kPerks[i].key] = mStacks[i];
    }
}

void SurvivalPerkManager::Load(const nlohmann::json& j) {
    for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
        int v = j.value(kPerks[i].key, 0);
        if (v < 0) v = 0;
        if (v > kPerks[i].maxStacks) v = kPerks[i].maxStacks;
        mStacks[i] = v;
    }
}
```

- [ ] **Step 4: 构建验证（新 .cpp 自动入 glob）**

Run: 上方"构建"命令块。
Expected: `Linking CXX executable PlantsVsZombies.exe`，零 warning / 零 error。（管理器尚未被引用，仅验证自身编译通过。）

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/Perk/PerkType.h PlantVsZombies/Game/Perk/SurvivalPerkManager.h PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp
git commit -m "feat(perk): 新增 SurvivalPerkManager 数据模型与 3 个试水词条"
```

---

## Task 2: 接入 Board（持有 + 访问器 + 生存重置）

**Files:**
- Modify: `PlantVsZombies/Game/Board.h`
- Modify: `PlantVsZombies/Game/Board.cpp:39-44`（生存初始化分支）

- [ ] **Step 1: `Board.h` 顶部加 include**

在 `Board.h` 现有 include 区（与其它 `#include "..."` 同处）加一行：

```cpp
#include "Perk/SurvivalPerkManager.h"
```

- [ ] **Step 2: `Board.h` 加成员与公有访问器**

在 `Board` 类 `public:` 区（紧邻 `GetZombieHpMultiplier()` 即可）加访问器：

```cpp
    SurvivalPerkManager&       GetPerkManager()       { return mPerkManager; }
    const SurvivalPerkManager& GetPerkManager() const { return mPerkManager; }
```

在私有成员区（与 `mIsSurvival` / `mSurvivalRound` 同处，`Board.h:91-92` 附近）加值成员：

```cpp
    SurvivalPerkManager mPerkManager;   // 生存模式词条（非生存关恒空）
```

- [ ] **Step 3: `Board.cpp` 生存初始化清空词条**

定位 `Board.cpp:39-44` 的生存分支：

```cpp
	if (mIsSurvival)
	{
		mSurvivalRound = 1;
```

改为（在 `mSurvivalRound = 1;` 后加一行）：

```cpp
	if (mIsSurvival)
	{
		mSurvivalRound = 1;
		mPerkManager.Clear();   // 新生存局：词条清零（读档时由 Load 覆盖）
```

- [ ] **Step 4: 构建验证**

Run: 构建命令块。
Expected: 零 warning / 零 error。`Board.h` 现以值成员持有管理器，`SurvivalPerkManager.h` 已被包含。

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/Board.h PlantVsZombies/Game/Board.cpp
git commit -m "feat(perk): Board 持有 SurvivalPerkManager + 生存初始化清零"
```

---

## Task 3: AutoTest 注入与观测（add_perk 命令 + dump_state 词条块）

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（名表区 + 命令分派区 + dump_state 处理）

- [ ] **Step 1: 加 `kPerkNames` 名表**

在 `kZombieNames` / `kPlantNames` 等名表附近（同文件，`PerkType` 经 `Board.h` 已可见）加：

```cpp
	const std::unordered_map<std::string, PerkType> kPerkNames = {
		{ "PLANT_DAMAGE_UP",      PerkType::PLANT_DAMAGE_UP },
		{ "ZOMBIE_HEALTH_UP",     PerkType::ZOMBIE_HEALTH_UP },
		{ "ZOMBIE_DAMAGE_RESIST", PerkType::ZOMBIE_DAMAGE_RESIST },
	};
```

- [ ] **Step 2: 加 `add_perk` 命令分派**

在 `spawn_zombie` 处理块之后（`TestDriver.cpp:279` 附近）插入：

```cpp
	if (op == "add_perk") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("add_perk: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kPerkNames.find(cmd.value("type", ""));
		if (it == kPerkNames.end()) { Fail("未知词条类型: " + cmd.value("type", "")); return false; }
		int count = cmd.value("count", 1);
		for (int i = 0; i < count; ++i) gs->GetBoard()->GetPerkManager().AddPerk(it->second);
		return true;
	}
```

- [ ] **Step 3: dump_state 追加词条块**

在 `dump_state` 处理中、写出 `out` 之前（`TestDriver.cpp:329` 之后、`const std::string name = ...` 之前）插入：

```cpp
		{
			SurvivalPerkManager& pm = board->GetPerkManager();
			nlohmann::json stacks;
			stacks["PLANT_DAMAGE_UP"]      = pm.GetStacks(PerkType::PLANT_DAMAGE_UP);
			stacks["ZOMBIE_HEALTH_UP"]     = pm.GetStacks(PerkType::ZOMBIE_HEALTH_UP);
			stacks["ZOMBIE_DAMAGE_RESIST"] = pm.GetStacks(PerkType::ZOMBIE_DAMAGE_RESIST);
			nlohmann::json perks;
			perks["stacks"]              = stacks;
			perks["zombieHealthMult"]    = pm.GetZombieHealthMultiplier();
			perks["plantDamageOn100"]    = pm.ScalePlantDamage(100);
			perks["damageToZombieOn100"] = pm.ScaleDamageToZombie(100);
			out["perks"] = perks;
		}
```

- [ ] **Step 4: 构建验证**

Run: 构建命令块。
Expected: 零 warning / 零 error。

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m "test(perk): AutoTest add_perk 命令 + dump_state 词条块"
```

---

## Task 4: 冒烟脚本 + 管理器算术验证（确定性测试门）

**Files:**
- Create: `autotest/scripts/smoke_perks.json`

- [ ] **Step 1: 创建 `smoke_perks.json`**

> 设计：row0 僵尸在加词条**前**生成（基线血量），row1 僵尸在加 `ZOMBIE_HEALTH_UP`×1 **后**生成（应为基线×1.2）。`ZOMBIE_DAMAGE_RESIST` 加 15 次以验证在 10 层截断。

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 0, "x": 900 },
    { "op": "add_perk", "type": "ZOMBIE_HEALTH_UP" },
    { "op": "add_perk", "type": "PLANT_DAMAGE_UP", "count": 3 },
    { "op": "add_perk", "type": "ZOMBIE_DAMAGE_RESIST", "count": 15 },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 1, "x": 900 },
    { "op": "dump_state", "name": "perks.json" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 运行脚本**

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: `$LASTEXITCODE` = 0。

- [ ] **Step 3: Read 产物并断言**

Read: `build/clang-release/autotest/out/smoke_perks/perks.json`

断言（用 Read 人工核对）：
- `perks.stacks.PLANT_DAMAGE_UP` == 3
- `perks.stacks.ZOMBIE_HEALTH_UP` == 1
- `perks.stacks.ZOMBIE_DAMAGE_RESIST` == 10 ← **加了 15 次但截断在 10**（验证 AddPerk 上限）
- `perks.plantDamageOn100` == 130 ← 100×(1+0.10×3) 四舍五入（验证植物伤害算术）
- `perks.damageToZombieOn100` == 50 ← 100×(1−0.05×10)（验证免伤算术 + 50% 封顶）
- `zombies` 数组里 row0 与 row1 两只僵尸：`row1.bodyHealth` == round(`row0.bodyHealth` × 1.2) 且 `bodyMaxHealth` 同步 ← **end-to-end 验证血量词条在出生点生效**（此步同时覆盖 Task 5）

> 若 row1 血量未被放大：说明 Task 5 的 `GetZombieHpMultiplier` 改动尚未做——本步骤会在 Task 5 完成后才全绿；Task 4 本步先验证 `perks.*` 四项（与 Task 5 无关，此刻应已正确）。

- [ ] **Step 4: 提交脚本**

```bash
git add autotest/scripts/smoke_perks.json
git commit -m "test(perk): smoke_perks 冒烟脚本（验证层数截断与伤害/免伤算术）"
```

---

## Task 5: 接入①——僵尸血量 +20%

**Files:**
- Modify: `PlantVsZombies/Game/Board.h:230`（`GetZombieHpMultiplier`）

- [ ] **Step 1: 并入词条血量倍率**

当前（Task 加固后）：

```cpp
	double GetZombieHpMultiplier() const {
		double multiplier = 1.0;
		if (mIsSurvival)
			multiplier *= (1.0 + SURVIVAL_HP_GROWTH * static_cast<double>(mSurvivalRound - 1));
		return multiplier;
	}
```

改为（在 `if (mIsSurvival)` 块**之后**、`return` 之前加一行，置于 if 外以保持"空词条=中性"的无条件语义）：

```cpp
	double GetZombieHpMultiplier() const {
		double multiplier = 1.0;
		if (mIsSurvival)
			multiplier *= (1.0 + SURVIVAL_HP_GROWTH * static_cast<double>(mSurvivalRound - 1));
		multiplier *= mPerkManager.GetZombieHealthMultiplier();   // 词条：僵尸血量（空时=1.0）
		return multiplier;
	}
```

- [ ] **Step 2: 构建**

Run: 构建命令块。Expected: 零 warning / 零 error。

- [ ] **Step 3: 重跑 smoke_perks 并核对血量**

Run: Task 4 Step 2 的运行命令。
Read: `build/clang-release/autotest/out/smoke_perks/perks.json`
Expected: `zombies` 中 row1 僵尸 `bodyHealth` == round(row0 `bodyHealth` × 1.2)，`bodyMaxHealth` 同步放大。（出生缩放单点 `Board.cpp:168` 生效。）

- [ ] **Step 4: 提交**

```bash
git add PlantVsZombies/Game/Board.h
git commit -m "feat(perk): 僵尸血量词条并入 GetZombieHpMultiplier"
```

---

## Task 6: 接入②——全体植物伤害 +10%

**Files:**
- Modify: `PlantVsZombies/Game/Bullet/Bullet.cpp:29`（子弹命中）
- Modify: `PlantVsZombies/Game/Board.cpp:75`（`CreateBoom` 入口）

- [ ] **Step 1: 子弹命中放大**

`Bullet.cpp:29` 当前：

```cpp
					zombie->TakeDamage(this->GetBulletDamage());
```

改为：

```cpp
					zombie->TakeDamage(mBoard->GetPerkManager().ScalePlantDamage(this->GetBulletDamage()));
```

（`mBoard` 在子弹命中回调上下文中必非空——构造时 `if(!mBoard) return;` 已保证有效实例才注册碰撞回调。）

- [ ] **Step 2: `CreateBoom` 入口放大（含瞬时伤害的秒杀阈值，保持一致）**

`Board.cpp:75` 当前：

```cpp
void Board::CreateBoom(const Vector& position, int damage)
{
	g_particleSystem->EmitEffect("CherryBomb", position);
```

改为（入口处把 `damage` 缩放一次，令 `Charred` 秒杀阈值判定 `:87` 与实扣血 `:93` 用同一加成后值）：

```cpp
void Board::CreateBoom(const Vector& position, int damage)
{
	damage = mPerkManager.ScalePlantDamage(damage);   // 词条：全体植物伤害（含瞬时伤害）
	g_particleSystem->EmitEffect("CherryBomb", position);
```

- [ ] **Step 3: 构建**

Run: 构建命令块。Expected: 零 warning / 零 error。

- [ ] **Step 4: 验证（算术已在 Task 4 确定性覆盖；此处验证钟点未破坏现有玩法）**

Run: 既有回归脚本，确认无 crash：
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
说明：植物伤害的钟点接入是单行机械改动；其算术正确性由 Task 4 的 `perks.plantDamageOn100 == 130` 确定性验证。端到端伤害数值受射击节奏影响、不做脆弱断言；主人可在游戏内开 `mShowZombieHP` 截图目视确认。

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/Bullet/Bullet.cpp PlantVsZombies/Game/Board.cpp
git commit -m "feat(perk): 全体植物伤害词条接入子弹命中与 CreateBoom"
```

---

## Task 7: 接入③——僵尸免伤 +5%

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp:359`（`TakeDamage` 首行）

- [ ] **Step 1: TakeDamage 首行缩减**

`Zombie.cpp:359` 当前：

```cpp
void Zombie::TakeDamage(int damage)
{
	if (damage <= 0) return;

	SetGlowingTimer(0.1f);
```

改为（在首个 `if (damage <= 0) return;` 之后插入一行）：

```cpp
void Zombie::TakeDamage(int damage)
{
	if (damage <= 0) return;

	// 词条：僵尸免伤（生存专用；空词条/非生存关倍率=1，无副作用）。单点覆盖一切伤害来源。
	if (mBoard) damage = mBoard->GetPerkManager().ScaleDamageToZombie(damage);

	SetGlowingTimer(0.1f);
```

（`ScaleDamageToZombie` 对 `base>=1` 返回 `>=1`，故无需追加 `<=0` 检查。下游 `remainingDamage = damage;` 自然取缩减后值。）

- [ ] **Step 2: 构建**

Run: 构建命令块。Expected: 零 warning / 零 error。

- [ ] **Step 3: 验证**

算术正确性由 Task 4 的 `perks.damageToZombieOn100 == 50`（50% 封顶）确定性验证。钟点为单行插入，构建通过 + 回归脚本无 crash 即可：

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```

- [ ] **Step 4: 提交**

```bash
git add PlantVsZombies/Game/Zombie/Zombie.cpp
git commit -m "feat(perk): 僵尸免伤词条接入 Zombie::TakeDamage"
```

---

## Task 8: 存档/读档

**Files:**
- Modify: `PlantVsZombies/GameInfoSaver.cpp:79`（写）与 `:286`（读）附近

- [ ] **Step 1: 写存档**

在 `GameInfoSaver.cpp` 写 `j["survivalRound"] = board->mSurvivalRound;`（`:79`）之后插入：

```cpp
		if (board->mIsSurvival) board->GetPerkManager().Save(j["perks"]);
```

- [ ] **Step 2: 读存档**

在读 `board->mSurvivalRound = j.value("survivalRound", 1);`（`:286`）之后、`if (board->mIsSurvival) {` 块内（`BuildSurvivalSpawnList` 之前或之后均可）插入：

```cpp
			if (j.contains("perks")) board->GetPerkManager().Load(j["perks"]);
```

> 注：`Board.h:67` 已 `friend GameInfoSaver`，故 `board->mPerkManager` 直接访问亦可；此处用公有 `GetPerkManager()` 保持一致。`Save` 为 const、`Load` 非 const，`GetPerkManager()` 两个重载均满足。

- [ ] **Step 3: 构建**

Run: 构建命令块。Expected: 零 warning / 零 error。

- [ ] **Step 4: 验证（AutoTest 存档被短路，靠审查 + 范式对照）**

AutoTest 模式下 `saves/` 读写被短路，无法经脚本往返验证。核对要点：
- 写：仅生存模式写 `j["perks"]`，键名来自 `PerkInfo::key`（字符串稳定）。
- 读：`j.contains("perks")` 守卫 → 旧档无此字段天然兼容；`Load` 内夹回 `[0,maxStacks]`。
- 与既有 `survivalRound` 存读档范式逐行对照一致。
- （可选，主人手动）VS 里进生存模式、加几个词条、存档→重进→读档，确认层数恢复。

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/GameInfoSaver.cpp
git commit -m "feat(perk): 词条随 survivalRound 存档/读档（按字符串 key）"
```

---

## Task 9: 终验与回归

**Files:** 无（验证 + 收尾）

- [ ] **Step 1: 全量构建零警告**

Run: 构建命令块（看完整输出，非仅末尾）。
Expected: `clang-release` 零 warning / 零 error。

- [ ] **Step 2: 词条冒烟全绿**

Run: Task 4 Step 2。Read `perks.json`，Task 4 Step 3 全部断言 + Task 5 血量断言均满足。

- [ ] **Step 3: 既有脚本回归（中性默认 → 行为不变）**

Run（逐个，`$LASTEXITCODE` 均应为 0）：
```powershell
Push-Location build\clang-release
foreach ($s in @("smoke_gameplay","demo_peashooter","smoke_synthetic_input")) {
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\$s.json -Seed 42
  Write-Output "$s -> $LASTEXITCODE"
}
Pop-Location
```
Expected: 全部 0。无词条时三个钟点中性，既有脚本数值/行为不变。

- [ ] **Step 4:（可选）游戏内目视确认**

主人可在游戏菜单开启"显示僵尸血量"（`mShowZombieHP`），进生存模式加词条后观察血量数字放大、僵尸更耐打——与 `dump_state` 数值互证。

- [ ] **Step 5: 收尾提交（若有未提交的脚本/杂项）**

```bash
git status --short   # 确认无遗漏；预存的 ChooseCardUI.h 不在本功能范围，勿动
```

---

## Self-Review 记录

- **Spec 覆盖**：①数据模型=Task1；②三词条数值=Task1 kPerks 表；③Board 持有+生命周期=Task2；④三钟点接入=Task5/6/7；⑤存档=Task8；⑥UI=明确不做。全覆盖。
- **占位符**：无 TBD/TODO；每个改动均给出完整代码与确切命令/期望。
- **类型一致**：`PerkType` / `PerkInfo` / `SurvivalPerkManager` 方法签名（`AddPerk`/`GetStacks`/`Clear`/`ScalePlantDamage`/`ScaleDamageToZombie`/`GetZombieHealthMultiplier`/`Save`/`Load`/`GetInfo`）在 Task1 定义后，Task2–8 引用处逐一对齐。
- **测试现实**：已显式声明本仓库无单元框架，验证=构建+AutoTest dump 断言；存档往返因 AutoTest 短路无法自动验证，已注明靠范式对照+审查+可选手动。
