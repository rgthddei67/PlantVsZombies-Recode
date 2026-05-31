# 植物/僵尸注册式工厂 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 GameDataManager 内的数据驱动注册表取代 GameApp 的两个 `Instantiate*` switch，加类型不再改 switch。

**Architecture:** 给 `PlantInfo`/`ZombieInfo` 增 `scale` + 函数指针 `factory` 字段；登记集中在 `GameDataManager::InitializeHardcodedData`（每条带 `&MakePlant<具体类>`）；新增 `CreatePlant`/`CreateZombie` 做"运行期枚举 → 编译期类型"的查表派发；`GameApp` 两个工厂收缩为一行委托。

**Tech Stack:** C++17，函数指针（非 std::function，零捕获零堆分配），现有 `GameObjectManager::CreateGameObjectImmediateAsShared<T>` 模板。

**测试模型：** 本仓库无自动化测试框架，且 CLAUDE.md 禁止 assistant 构建。验证 = 主人在 VS F7 构建 + 实跑（见 Task 4）。Task 1–3 中途**不可单独编译**（接口与实现需配套），统一在 Task 3 末尾构建。提交均由主人驱动；计划内"提交建议"是主人检查点，assistant 不自动提交。

**参考 spec：** `docs/superpowers/specs/2026-05-31-plant-zombie-factory-registry-design.md`

---

## 文件结构

| 文件 | 角色 | 改动 |
|---|---|---|
| `PlantVsZombies/Game/Plant/GameDataManager.h` | 类型元数据 + 工厂接口声明 | 加前向声明、函数指针 typedef、`PlantInfo`/`ZombieInfo` 字段、`CreatePlant`/`CreateZombie` 声明、`RegisterPlant`/`RegisterZombie` 签名扩参 |
| `PlantVsZombies/Game/Plant/GameDataManager.cpp` | 工厂助手 + 登记 + 派发实现 | 加 include、匿名命名空间 `MakePlant`/`MakeZombie`、扩参实现、14 条登记补 `scale`+`factory`、实现 `CreatePlant`/`CreateZombie` |
| `PlantVsZombies/GameApp.cpp` | 薄委托 | 两个 `Instantiate*` 函数体换成一行委托；删多余 include |

---

## Task 1: GameDataManager.h —— 接口与数据结构

**Files:**
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.h`

- [ ] **Step 1: 顶部加前向声明与函数指针 typedef**

在 `#include <vector>`（第 11 行附近）之后、`struct PlantInfo` 之前插入：

```cpp
class Board;
class Plant;
class Zombie;

// 工厂函数指针：运行期枚举 → 编译期具体类型。零捕获，可直接由 MakePlant<T>/MakeZombie<T> 赋值。
using PlantFactoryFn  = std::shared_ptr<Plant>(*)(Board*, PlantType, int, int, AnimationType, float, bool);
using ZombieFactoryFn = std::shared_ptr<Zombie>(*)(Board*, ZombieType, float, float, int, AnimationType, float, bool);
```

并确保文件顶部含 `#include <memory>`（若无则在 `#include <vector>` 后补一行 `#include <memory>`）。

- [ ] **Step 2: PlantInfo 增字段**

在 `struct PlantInfo` 的 `Vector offset;`（成员区末尾，约第 22 行）之后、构造函数之前加：

```cpp
	float scale = 1.0f;          // 创建时的缩放（仅 PotatoMine=0.8，其余=1.0）
	PlantFactoryFn factory = nullptr;  // 具体类的构造工厂
```

> 注意：不改 `PlantInfo` 的多参构造函数——这两个字段由 `RegisterPlant` 在构造后赋值。

- [ ] **Step 3: ZombieInfo 增字段**

在 `struct ZombieInfo` 的 `int appearWave;`（约第 45 行）之后、构造函数之前加：

```cpp
	float scale = 1.0f;
	ZombieFactoryFn factory = nullptr;
```

- [ ] **Step 4: 加 CreatePlant / CreateZombie 公有声明**

在 `class GameDataManager` 的 public 区，`bool HasZombie(...) const;`（约第 222 行）之后加：

```cpp
	/**
	 * @brief 按类型创建植物（注册表派发，取代 GameApp 的 switch）
	 * @return 未注册或缺工厂返回 nullptr
	 */
	std::shared_ptr<Plant> CreatePlant(PlantType type, Board* board, int row, int col, bool isPreview) const;

	/**
	 * @brief 按类型创建僵尸（注册表派发）
	 * @return 未注册或缺工厂返回 nullptr
	 */
	std::shared_ptr<Zombie> CreateZombie(ZombieType type, Board* board, float x, float y, int row, bool isPreview) const;
```

- [ ] **Step 5: 扩 RegisterPlant / RegisterZombie 私有声明**

把 `RegisterPlant` 声明（约第 248–254 行）末尾的 `const Vector& offset);` 改为：

```cpp
		const Vector& offset,
		float scale,
		PlantFactoryFn factory);
```

把 `RegisterZombie` 声明（约第 266–270 行）末尾的 `const Vector& offset, int weight, int appearWave);` 改为：

```cpp
		const Vector& offset, int weight, int appearWave,
		float scale,
		ZombieFactoryFn factory);
```

- [ ] **Step 6: 提交建议（主人驱动）**

```
GameDataManager.h: 注册式工厂接口（scale+factory 字段、CreatePlant/CreateZombie、Register 扩参）
```

---

## Task 2: GameDataManager.cpp —— 助手、登记、派发

**Files:**
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`

- [ ] **Step 1: 补 include**

在现有 `#include "../../ResourceKeys.h"`（第 2 行）之后加入（这批正是 Task 3 将从 GameApp.cpp 删去的同一批）：

```cpp
#include "../GameObjectManager.h"
#include "../RenderOrder.h"
#include "../Board.h"

#include "PeaShooter.h"
#include "SunFlower.h"
#include "CherryBomb.h"
#include "WallNut.h"
#include "PotatoMine.h"
#include "SnowPeaShooter.h"
#include "Chomper.h"
#include "Repeater.h"
#include "PuffShroom.h"

#include "../Zombie/Zombie.h"
#include "../Zombie/ConeZombie.h"
#include "../Zombie/Polevaulter.h"
#include "../Zombie/BucketZombie.h"
#include "../Zombie/FastBucketZombie.h"
```

- [ ] **Step 2: 加匿名命名空间工厂助手**

在 include 之后、`GameDataManager::GameDataManager()`（约第 7 行）之前插入：

```cpp
namespace {
	template<typename T>
	std::shared_ptr<Plant> MakePlant(Board* b, PlantType t, int row, int col,
		AnimationType anim, float scale, bool preview) {
		return GameObjectManager::GetInstance()
			.CreateGameObjectImmediateAsShared<T>(LAYER_GAME_PLANT, b, t, row, col, anim, scale, preview);
	}
	template<typename T>
	std::shared_ptr<Zombie> MakeZombie(Board* b, ZombieType t, float x, float y, int row,
		AnimationType anim, float scale, bool preview) {
		return GameObjectManager::GetInstance()
			.CreateGameObjectImmediateAsShared<T>(LAYER_GAME_ZOMBIE, b, t, x, y, row, anim, scale, preview);
	}
}
```

- [ ] **Step 3: 扩 RegisterPlant 实现并赋值新字段**

把 `RegisterPlant` 实现（约第 182–202 行）的签名与开头改为：

```cpp
void GameDataManager::RegisterPlant(PlantType type,
	int sunCost, float cooldown,
	const std::string& enumName,
	const std::string& textureKey,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset,
	float scale,
	PlantFactoryFn factory) {
	PlantInfo info(type, sunCost, cooldown, enumName, textureKey, animType, animName, offset);
	info.scale = scale;
	info.factory = factory;
	mPlantInfo[type] = info;
```

（其下 `mEnumNameToType[...]` 等其余行保持不变。）

- [ ] **Step 4: 扩 RegisterZombie 实现并赋值新字段**

把 `RegisterZombie` 实现（约第 204–219 行）的签名与开头改为：

```cpp
void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset, int weight, int appearWave,
	float scale,
	ZombieFactoryFn factory) {
	ZombieInfo info(type, enumName, animType, animName, offset, weight, appearWave);
	info.scale = scale;
	info.factory = factory;
	mZombieInfo[type] = info;
```

（其下 `mAnimToString[animType] = animName;` 等保持不变。）

- [ ] **Step 5: 每条 RegisterPlant 调用追加 `scale, &MakePlant<类>`**

`InitializeHardcodedData` 内 9 条 `RegisterPlant(...)`（第 28–116 行）。模式：把每条调用最后一个实参 `offset`（形如 `Vector(-37.6f, -44)`）后补两个尾参数。示例（PEASHOOTER，第 38–46 行）改后：

```cpp
	RegisterPlant(
		PlantType::PLANT_PEASHOOTER,
		100, 7.5f,
		"PLANT_PEASHOOTER",
		ResourceKeys::Textures::IMAGE_PEASHOOTER,
		AnimationType::ANIM_PEASHOOTER,
		ResourceKeys::Reanimations::REANIM_PEASHOOTER,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<PeaShooter>
	);
```

按下表对 9 条逐一追加尾参数（顺序即文件中出现顺序）：

| RegisterPlant（PlantType） | 追加尾参数 |
|---|---|
| `PLANT_SUNFLOWER` | `1.0f, &MakePlant<SunFlower>` |
| `PLANT_PEASHOOTER` | `1.0f, &MakePlant<PeaShooter>` |
| `PLANT_CHERRYBOMB` | `1.0f, &MakePlant<CherryBomb>` |
| `PLANT_WALLNUT` | `1.0f, &MakePlant<WallNut>` |
| `PLANT_POTATOMINE` | `0.8f, &MakePlant<PotatoMine>` |
| `PLANT_SNOWPEA` | `1.0f, &MakePlant<SnowPeaShooter>` |
| `PLANT_CHOMPER` | `1.0f, &MakePlant<Chomper>` |
| `PLANT_REPEATER` | `1.0f, &MakePlant<Repeater>` |
| `PLANT_PUFFSHROOM` | `1.0f, &MakePlant<PuffShroom>` |

> PotatoMine 必须是 `0.8f`（迁移自原 switch 的唯一非默认 scale）；其余 `1.0f`。

- [ ] **Step 6: 每条 RegisterZombie 调用追加 `scale, &MakeZombie<类>`**

`InitializeHardcodedData` 内 5 条 `RegisterZombie(...)`（第 120–168 行）。同样在最后一个实参 `appearWave`（形如 `1`）后补两个尾参数。示例（ZOMBIE_NORMAL，第 120–128 行）改后：

```cpp
	RegisterZombie(
		ZombieType::ZOMBIE_NORMAL,
		"ZOMBIE_NORMAL",
		AnimationType::ANIM_NORMAL_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_NORMAL_ZOMBIE,
		Vector(-50, -85),
		1000,
		1,
		1.0f, &MakeZombie<Zombie>
	);
```

按下表对 5 条逐一追加（顺序即文件中出现顺序）：

| RegisterZombie（ZombieType） | 追加尾参数 |
|---|---|
| `ZOMBIE_NORMAL` | `1.0f, &MakeZombie<Zombie>` |
| `ZOMBIE_TRAFFIC_CONE` | `1.0f, &MakeZombie<ConeZombie>` |
| `ZOMBIE_POLEVAULTER` | `1.0f, &MakeZombie<Polevaulter>` |
| `ZOMBIE_BUCKET` | `1.0f, &MakeZombie<BucketZombie>` |
| `ZOMBIE_FASTBUCKET` | `1.0f, &MakeZombie<FastBucketZombie>` |

> FastBucket 用 `&MakeZombie<FastBucketZombie>`（与 Bucket 不同类，但共用 `ANIM_BUCKET_ZOMBIE` 动画——动画已在登记数据里，无需变）。

- [ ] **Step 7: 实现 CreatePlant / CreateZombie**

在 `RegisterZombie` 实现之后（约第 219 行之后）插入：

```cpp
std::shared_ptr<Plant> GameDataManager::CreatePlant(PlantType type, Board* board, int row, int col, bool isPreview) const {
	auto it = mPlantInfo.find(type);
	if (it == mPlantInfo.end() || !it->second.factory) {
		std::cout << "[GameDataManager] 未注册或缺工厂的植物类型: " << static_cast<int>(type) << std::endl;
		return nullptr;
	}
	const PlantInfo& i = it->second;
	return i.factory(board, type, row, col, i.animType, i.scale, isPreview);
}

std::shared_ptr<Zombie> GameDataManager::CreateZombie(ZombieType type, Board* board, float x, float y, int row, bool isPreview) const {
	auto it = mZombieInfo.find(type);
	if (it == mZombieInfo.end() || !it->second.factory) {
		std::cout << "[GameDataManager] 未注册或缺工厂的僵尸类型: " << static_cast<int>(type) << std::endl;
		return nullptr;
	}
	const ZombieInfo& i = it->second;
	return i.factory(board, type, x, y, row, i.animType, i.scale, isPreview);
}
```

- [ ] **Step 8: 提交建议（主人驱动）**

```
GameDataManager.cpp: 工厂助手 + 14 条登记补 scale/factory + CreatePlant/CreateZombie 派发
```

---

## Task 3: GameApp.cpp —— 收缩为薄委托并构建验证

**Files:**
- Modify: `PlantVsZombies/GameApp.cpp`

- [ ] **Step 1: 把 InstantiatePlant 函数体换成一行委托**

把整个 `InstantiatePlant`（第 559–603 行，从 `std::shared_ptr<Plant> GameAPP::InstantiatePlant(` 起到对应右花括号止）替换为：

```cpp
std::shared_ptr<Plant> GameAPP::InstantiatePlant(PlantType plantType, Board* board, int row, int column, bool isPreview)
{
	return GameDataManager::GetInstance().CreatePlant(plantType, board, row, column, isPreview);
}
```

- [ ] **Step 2: 把 InstantiateZombie 函数体换成一行委托**

把整个 `InstantiateZombie`（第 605–633 行）替换为：

```cpp
std::shared_ptr<Zombie> GameAPP::InstantiateZombie(ZombieType zombieType, Board* board, float x, float y, int row, bool isPreview)
{
	return GameDataManager::GetInstance().CreateZombie(zombieType, board, x, y, row, isPreview);
}
```

- [ ] **Step 3: 删除 13 个具体子类 include，保留/补充基类**

> 关键：`Board.h` 对 `Plant`/`Zombie` 只做**前向声明**（`Board.h:19-20`），不带完整定义。`GameApp.cpp` 两个 `Instantiate*` 的返回类型是 `std::shared_ptr<Plant>`/`<Zombie>`，当前靠这批头文件间接带入完整定义。因此只删**具体子类**，保留基类 `Zombie.h`，并**显式补基类 `Plant.h`**（原先无单独 include，靠植物子类间接带入）。

删除第 23–31 行的 9 个植物**子类**：

```cpp
#include "./Game/Plant/PeaShooter.h"
#include "./Game/Plant/SunFlower.h"
#include "./Game/Plant/CherryBomb.h"
#include "./Game/Plant/WallNut.h"
#include "./Game/Plant/PotatoMine.h"
#include "./Game/Plant/SnowPeaShooter.h"
#include "./Game/Plant/Chomper.h"
#include "./Game/Plant/Repeater.h"
#include "./Game/Plant/PuffShroom.h"
```

删除第 34–37 行的 4 个僵尸**子类**（注意：**不要**删第 33 行的基类 `Zombie.h`）：

```cpp
#include "./Game/Zombie/ConeZombie.h"
#include "./Game/Zombie/Polevaulter.h"
#include "./Game/Zombie/BucketZombie.h"
#include "./Game/Zombie/FastBucketZombie.h"
```

保留第 33 行基类 `#include "./Game/Zombie/Zombie.h"`，并在其后新增基类 Plant：

```cpp
#include "./Game/Zombie/Zombie.h"
#include "./Game/Plant/Plant.h"
```

> 同时保留 `#include "./Game/Plant/GameDataManager.h"`（第 20 行）与 `#include "./Game/Board.h"`（第 39 行）——委托调用与参数类型需要它们。

- [ ] **Step 4: 主人 VS F7 构建（assistant 不构建）**

预期：编译链接通过，无 LNK/编译错误。（Plant/Zombie 完整类型已由 Step 3 保留的基类 `Zombie.h`/`Plant.h` 保证。）

- [ ] **Step 5: 提交建议（主人驱动）**

```
GameApp.cpp: InstantiatePlant/InstantiateZombie 收缩为 GameDataManager 委托，删冗余 include
```

---

## Task 4: 实跑验证（主人）

**Files:** 无（行为验证）

- [ ] **Step 1: 全类型创建**

启动游戏，进入一关，逐一种植 9 种植物（向日葵/豌豆/樱桃/坚果/土豆雷/寒冰射手/食人花/双发/小喷菇），确认动画、位置、攻击正常；推进波次确认 5 种僵尸（普通/路障/撑杆/铁桶/快速铁桶）正常生成与行走。

- [ ] **Step 2: PotatoMine 视觉回归（重点）**

种土豆雷，确认其显示尺寸与改造前一致（scale 0.8 已由数据携带）。

- [ ] **Step 3: 预览路径**

进入选卡界面，确认卡片预览僵尸（`isPreview=true`）正常显示。

- [ ] **Step 4: 读档路径**

存档后读档，确认 `CreatePlantWithID`/`CreateZombieWithID`（仍走 `InstantiatePlant`/`InstantiateZombie` 委托）能正确恢复植物/僵尸。

- [ ] **Step 5: 收尾**

确认控制台无 `[GameDataManager] 未注册或缺工厂` 日志。验证通过后，可清理本计划/spec 的勾选状态。

---

## 验证清单回顾（spec §7 对照）

- spec §4.1 数据结构 → Task 1 Step 1–3 ✓
- spec §4.2 助手与登记 → Task 2 Step 1–6 ✓
- spec §4.3 派发入口 → Task 2 Step 7 ✓
- spec §4.4 GameApp 收口 → Task 3 ✓
- spec §7 验证 → Task 4 ✓
- spec §3 PotatoMine scale 迁移 → Task 2 Step 5 表（0.8f）+ Task 4 Step 2 ✓
