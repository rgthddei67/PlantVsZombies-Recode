# GameDataManager 数值 JSON 化（gamedata.json）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `GameDataManager` 注册表中的植物/僵尸数值（cost/cooldown/weight/appearWave/survivalRound/offset/scale）外置到 `resources/gamedata.json`，JSON 为唯一数值来源，缺失/缺字段一律报错拒绝启动。

**Architecture:** 代码保留纯身份注册（枚举/纹理键/动画/工厂指针），随后 `LoadNumbersFromJson()` 按枚举名回填数值；校验收集全部错误后一次性 `LOG_ERROR` + 弹窗（AutoTest 模式不弹），`Initialize()` 返回 false → `GameAPP::Run` 既有 -6 失败路径退出。

**Tech Stack:** C++17、nlohmann/json（经 `FileManager::LoadJsonFile`）、SDL2 消息框、CMake preset 构建、AutoTest 回归。

**Spec:** `docs/superpowers/specs/2026-07-07-gamedata-json-design.md`（已获主人批准）

## Global Constraints

- 资源目录**不在 git、CMake 不拷贝**：`gamedata.json` 必须在 `build\clang-release\resources\` 与 `build\msvc-debug\resources\` 各放一份；负向测试只可改名/改内容后**必须还原**（这是唯一实体，没有源头可再拷）。
- 初始 JSON 值 = 现硬编码值**逐字迁移**，行为零变化；回归以 `-Seed 42` 固定步长逐字节复现为准。
- 构建须在 VS 开发环境内（先把 Installer 目录加 PATH 再导 VsDevCmd，见 CLAUDE.md），验证用 `clang-release`（唯一报警告的配置，须零警告）。
- 严格校验：整数字段须 `is_number_integer()`，浮点字段须 `is_number()`，offset 须双元素数字数组；**任何一个缺失/类型错都记错**。
- 未注册的多余 JSON 键 → `LOG_WARN` 不阻断。
- 中文字符串 UTF-8（源文件 `/utf-8` 编译）。
- 提交信息避免 PowerShell 双引号拆参数坑：用单引号 here-string `@'...'@`。

---

### Task 1: 创建 gamedata.json（两份）+ 采集改造前基线

**Files:**
- Create: `build\clang-release\resources\gamedata.json`
- Create: `build\msvc-debug\resources\gamedata.json`（内容完全相同）
- Baseline: `<scratchpad>\baseline_demo_peashooter\`（改造前 AutoTest 产物快照，Task 3 用来 diff）

**Interfaces:**
- Produces: 运行时 `./resources/gamedata.json`，结构 `{"plants":{<枚举名>:{cost,cooldown,offset,scale}}, "zombies":{<枚举名>:{weight,appearWave,survivalRound,offset,scale}}}`。键 = `PlantInfo::enumName`/`ZombieInfo::enumName` 逐字（注意 `PLANT_SNOWPEASHOOTER` 不是 `PLANT_SNOWPEA`）。

- [ ] **Step 1: 写入 JSON（下面内容一字不差，两个路径各写一份）**

```json
{
  "plants": {
    "PLANT_SUNFLOWER":       { "cost": 50,  "cooldown": 7.5,  "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_PEASHOOTER":      { "cost": 100, "cooldown": 7.5,  "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_CHERRYBOMB":      { "cost": 150, "cooldown": 50.0, "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_WALLNUT":         { "cost": 50,  "cooldown": 25.0, "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_POTATOMINE":      { "cost": 25,  "cooldown": 20.0, "offset": [-30.0, -35.2], "scale": 0.8 },
    "PLANT_SNOWPEASHOOTER":  { "cost": 175, "cooldown": 7.5,  "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_CHOMPER":         { "cost": 150, "cooldown": 7.5,  "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_REPEATER":        { "cost": 200, "cooldown": 7.5,  "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_PUFFSHROOM":      { "cost": 0,   "cooldown": 7.5,  "offset": [-37.6, -28.0], "scale": 1.0 },
    "PLANT_SUNSHROOM":       { "cost": 25,  "cooldown": 7.5,  "offset": [-37.6, -28.0], "scale": 1.0 },
    "PLANT_FUMESHROOM":      { "cost": 100, "cooldown": 7.5,  "offset": [-37.6, -36.0], "scale": 0.92 },
    "PLANT_HYPNOSHROOM":     { "cost": 75,  "cooldown": 25.0, "offset": [-37.6, -36.0], "scale": 0.92 }
  },
  "zombies": {
    "ZOMBIE_NORMAL":       { "weight": 1000, "appearWave": 1, "survivalRound": 1, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_TRAFFIC_CONE": { "weight": 1500, "appearWave": 3, "survivalRound": 2, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_POLEVAULTER":  { "weight": 1700, "appearWave": 5, "survivalRound": 3, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_BUCKET":       { "weight": 2000, "appearWave": 5, "survivalRound": 3, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_FASTBUCKET":   { "weight": 2800, "appearWave": 6, "survivalRound": 6, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_NEWSPAPER":    { "weight": 2000, "appearWave": 2, "survivalRound": 5, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_FASTPAPER":    { "weight": 3000, "appearWave": 6, "survivalRound": 7, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_DOOR":         { "weight": 2100, "appearWave": 3, "survivalRound": 3, "offset": [-50.0, -85.0], "scale": 1.0 },
    "ZOMBIE_FOOTBALL":     { "weight": 2500, "appearWave": 5, "survivalRound": 5, "offset": [-64.0, -94.0], "scale": 1.0 }
  }
}
```

数值来源（校对用）：`PlantVsZombies/Game/Plant/GameDataManager.cpp` 现行 `RegisterPlant`（82-212 行）/`RegisterZombie`（216-322 行）调用。逐项核对一遍再继续。

- [ ] **Step 2: 验证 JSON 可解析**

```powershell
Get-Content build\clang-release\resources\gamedata.json -Raw | ConvertFrom-Json | Out-Null; "parse-ok=$?"
Get-Content build\msvc-debug\resources\gamedata.json -Raw | ConvertFrom-Json | Out-Null; "parse-ok=$?"
```
Expected: 两行都输出 `parse-ok=True`

- [ ] **Step 3: 采集改造前基线（代码未动，此时行为 = 旧硬编码）**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
"exit=$LASTEXITCODE"
Pop-Location
Copy-Item -Recurse -Force build\clang-release\autotest\out\demo_peashooter `
  "$env:LOCALAPPDATA\Temp\claude\D--PVZ-PlantsVsZombies-PlantVsZombies\28339bd3-15f5-4711-b296-a6f7b0480af9\scratchpad\baseline_demo_peashooter"
```
Expected: `exit=0`，基线目录含 PNG + state.json + run.log。
（若 exe 尚未构建，先按 Global Constraints 导 VS 环境并 `cmake --build --preset clang-release`。）

- [ ] **Step 4: 无需 git 提交**（resources 不在 git；基线在 scratchpad）。仅在任务记录里注明两份 JSON 已就位。

---

### Task 2: GameDataManager 身份/数值分离 + 严格校验加载 + GameAPP 接入

**Files:**
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.h`
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`
- Modify: `PlantVsZombies/GameAPP.cpp:179-180`

**Interfaces:**
- Consumes: Task 1 的 `./resources/gamedata.json`（结构见 Task 1 Produces）；`FileManager::LoadJsonFile(const std::string&, nlohmann::json&) -> bool`（`FileManager.h:44`）；`TestDriver::GetInstance().IsActive() -> bool`（`Game/AutoTest/TestDriver.h:18`）。
- Produces: `bool GameDataManager::Initialize()`（false = 校验失败，调用方须中止启动）；`RegisterPlant(PlantType, const std::string&, const std::string&, AnimationType, const std::string&, PlantFactoryFn)`；`RegisterZombie(ZombieType, const std::string&, AnimationType, const std::string&, ZombieFactoryFn)`；私有 `bool LoadNumbersFromJson()`。所有既有 Getter（GetPlantSunCost 等）签名不变。

- [ ] **Step 1: 改 GameDataManager.h**

① 删除 `PlantInfo` 的 8 参构造（37-42 行）和 `ZombieInfo` 的 7 参构造（60-64 行），各自保留 `= default`（数值字段及其默认值不动，仅由 JSON 回填）。

② `void Initialize();` → `bool Initialize();`，注释改为：

```cpp
	/**
	 * @brief 初始化：注册身份数据（硬编码）+ 从 resources/gamedata.json 加载数值
	 * @return false = JSON 缺失/解析失败/缺条目/缺字段（调用方须中止启动）
	 */
	bool Initialize();
```

③ `RegisterPlant`/`RegisterZombie` 声明替换为纯身份签名：

```cpp
	/**
	 * @brief 注册一种植物的身份数据（内部使用）。数值（cost/cooldown/offset/scale）
	 *        一律来自 resources/gamedata.json，由 LoadNumbersFromJson 回填。
	 */
	void RegisterPlant(PlantType type,
		const std::string& enumName,
		const std::string& textureKey,
		AnimationType animType,
		const std::string& animName,
		PlantFactoryFn factory);

	/**
	 * @brief 注册一种僵尸的身份数据（内部使用）。数值（weight/appearWave/
	 *        survivalRound/offset/scale）一律来自 resources/gamedata.json。
	 */
	void RegisterZombie(ZombieType type,
		const std::string& enumName,
		AnimationType animType,
		const std::string& animName,
		ZombieFactoryFn factory);
```

④ `InitializeHardcodedData()` 声明后面加：

```cpp
	/**
	 * @brief 从 ./resources/gamedata.json 回填全部数值字段（JSON 是数值唯一来源）。
	 *        严格校验：文件缺失/解析失败/任一已注册类型缺条目/条目缺任一字段/类型错
	 *        → 收集全部错误统一 LOG_ERROR + 弹窗（AutoTest 模式不弹），返回 false。
	 *        JSON 中未注册的多余键仅 LOG_WARN 不阻断。
	 */
	bool LoadNumbersFromJson();
```

- [ ] **Step 2: 改 GameDataManager.cpp——includes 与 Initialize**

顶部 `#include "../../Logger.h"` 之后追加：

```cpp
#include "../../FileManager.h"
#include "../AutoTest/TestDriver.h"
#include <SDL2/SDL.h>
#include <unordered_set>
```

`Initialize()`（51-64 行）替换为：

```cpp
bool GameDataManager::Initialize() {
	// 清空现有数据
	mPlantInfo.clear();
	mZombieInfo.clear();
	mAnimToString.clear();
	mEnumNameToType.clear();
	mTextureKeyToType.clear();
	mAnimNameToType.clear();

	InitializeHardcodedData();
	if (!LoadNumbersFromJson()) return false;

	LOG_INFO("GameData") << "初始化完成，共注册 "
		<< mPlantInfo.size() << " 种植物，"
		<< mZombieInfo.size() << " 种僵尸（数值来自 gamedata.json）";
	return true;
}
```

- [ ] **Step 3: 重写 InitializeHardcodedData 的注册区（纯身份，21 条全量替换）**

函数头注释（67-79 行）替换为：

```cpp
	// ============================================================
	// 本函数只注册【身份数据】：枚举名/纹理键/动画/构造工厂。
	// 全部【数值】（cost/cooldown/weight/appearWave/survivalRound/offset/scale）
	// 住在 resources/gamedata.json（数值唯一来源，改数值不用重编译）。
	// 【新增植物】共 5 步（不再需要改 GameApp 的 switch —— 已用注册式工厂取代）：
	//   1. PlantType.h    加枚举项
	//   2. Game/Plant/    写植物类（纯头文件即可，构造经 using 继承 Plant/Shooter）
	//   3. 本文件          在下方"植物注册"区加一行 RegisterPlant(..., &MakePlant<新类>)
	//      并在本文件顶部 #include "你的植物.h"
	//   4. resources/gamedata.json 的 "plants" 加同枚举名条目（漏了 → 启动时报错退出，
	//      注意资源目录在 build/<preset>/resources/，两个 preset 都要加）
	//   5. 加 Card 条目
	// 【新增僵尸】同理：ZombieType.h 加枚举 → 写僵尸类 → 下方"僵尸注册"区加一行
	//      RegisterZombie(..., &MakeZombie<新类>) + 顶部 #include
	//      + gamedata.json 的 "zombies" 加条目（weight/appearWave/survivalRound 都在 JSON）。
	// 配套常量：动画类型加在 Reanimation/AnimationTypes.h；资源键加在 ResourceKeys.h。
	// ============================================================
```

植物+僵尸注册区（82-322 行）整体替换为：

```cpp
	// ==================== 植物注册（仅身份，数值见 gamedata.json） ====================
	RegisterPlant(PlantType::PLANT_SUNFLOWER, "PLANT_SUNFLOWER",
		ResourceKeys::Textures::IMAGE_SUNFLOWER,
		AnimationType::ANIM_SUNFLOWER,
		ResourceKeys::Reanimations::REANIM_SUNFLOWER, &MakePlant<SunFlower>);

	RegisterPlant(PlantType::PLANT_PEASHOOTER, "PLANT_PEASHOOTER",
		ResourceKeys::Textures::IMAGE_PEASHOOTER,
		AnimationType::ANIM_PEASHOOTER,
		ResourceKeys::Reanimations::REANIM_PEASHOOTER, &MakePlant<PeaShooter>);

	RegisterPlant(PlantType::PLANT_CHERRYBOMB, "PLANT_CHERRYBOMB",
		ResourceKeys::Textures::IMAGE_CHERRYBOMB,
		AnimationType::ANIM_CHERRYBOMB,
		ResourceKeys::Reanimations::REANIM_CHERRYBOMB, &MakePlant<CherryBomb>);

	RegisterPlant(PlantType::PLANT_WALLNUT, "PLANT_WALLNUT",
		ResourceKeys::Textures::IMAGE_WALLNUT,
		AnimationType::ANIM_WALLNUT,
		ResourceKeys::Reanimations::REANIM_WALLNUT, &MakePlant<WallNut>);

	RegisterPlant(PlantType::PLANT_POTATOMINE, "PLANT_POTATOMINE",
		ResourceKeys::Textures::IMAGE_POTATOMINE,
		AnimationType::ANIM_POTATOMINE,
		ResourceKeys::Reanimations::REANIM_POTATOMINE, &MakePlant<PotatoMine>);

	RegisterPlant(PlantType::PLANT_SNOWPEA, "PLANT_SNOWPEASHOOTER",
		ResourceKeys::Textures::IMAGE_SNOWPEASHOOTER,
		AnimationType::ANIM_SNOWPEASHOOTER,
		ResourceKeys::Reanimations::REANIM_SNOWPEASHOOTER, &MakePlant<SnowPeaShooter>);

	RegisterPlant(PlantType::PLANT_CHOMPER, "PLANT_CHOMPER",
		ResourceKeys::Textures::IMAGE_CHOMPER,
		AnimationType::ANIM_CHOMPER,
		"Chomper", &MakePlant<Chomper>);

	RegisterPlant(PlantType::PLANT_REPEATER, "PLANT_REPEATER",
		ResourceKeys::Textures::IMAGE_REPEATER,
		AnimationType::ANIM_REPEAT,
		"Repeater", &MakePlant<Repeater>);

	RegisterPlant(PlantType::PLANT_PUFFSHROOM, "PLANT_PUFFSHROOM",
		ResourceKeys::Textures::IMAGE_PUFFSHROOM,
		AnimationType::ANIM_PUFFSHROOM,
		"PuffShroom", &MakePlant<PuffShroom>);

	RegisterPlant(PlantType::PLANT_SUNSHROOM, "PLANT_SUNSHROOM",
		ResourceKeys::Textures::IMAGE_SUNSHROOM,
		AnimationType::ANIM_SUNSHROOM,
		"SunShroom", &MakePlant<SunShroom>);

	RegisterPlant(PlantType::PLANT_FUMESHROOM, "PLANT_FUMESHROOM",
		ResourceKeys::Textures::IMAGE_FUMESHROOM,
		AnimationType::ANIM_FUMESHROOM,
		"FumeShroom", &MakePlant<FumeShroom>);

	RegisterPlant(PlantType::PLANT_HYPNOSHROOM, "PLANT_HYPNOSHROOM",
		ResourceKeys::Textures::IMAGE_HYPNOSHROOM,
		AnimationType::ANIM_HYPER,
		"HypnoShroom", &MakePlant<HypnoShroom>);

	// ==================== 僵尸注册（仅身份，数值见 gamedata.json） ====================
	RegisterZombie(ZombieType::ZOMBIE_NORMAL, "ZOMBIE_NORMAL",
		AnimationType::ANIM_NORMAL_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_NORMAL_ZOMBIE, &MakeZombie<Zombie>);

	RegisterZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, "ZOMBIE_TRAFFIC_CONE",
		AnimationType::ANIM_CONE_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_CONE_ZOMBIE, &MakeZombie<ConeZombie>);

	RegisterZombie(ZombieType::ZOMBIE_POLEVAULTER, "ZOMBIE_POLEVAULTER",
		AnimationType::ANIM_POLEVAULTER_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_POLEVAULTER_ZOMBIE, &MakeZombie<Polevaulter>);

	RegisterZombie(ZombieType::ZOMBIE_BUCKET, "ZOMBIE_BUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE, &MakeZombie<BucketZombie>);

	RegisterZombie(ZombieType::ZOMBIE_FASTBUCKET, "ZOMBIE_FASTBUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE, &MakeZombie<FastBucketZombie>);

	RegisterZombie(ZombieType::ZOMBIE_NEWSPAPER, "ZOMBIE_NEWSPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,
		"PaperZombie", &MakeZombie<PaperZombie>);

	// 复用读报僵尸 reanim，仅换报纸贴图
	RegisterZombie(ZombieType::ZOMBIE_FASTPAPER, "ZOMBIE_FASTPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,
		"PaperZombie", &MakeZombie<FastPaperZombie>);

	RegisterZombie(ZombieType::ZOMBIE_DOOR, "ZOMBIE_DOOR",
		AnimationType::ANIM_DOOR_ZOMBIE,
		"DoorZombie", &MakeZombie<DoorZombie>);

	RegisterZombie(ZombieType::ZOMBIE_FOOTBALL, "ZOMBIE_FOOTBALL",
		AnimationType::ANIM_FOOTBALL_ZOMBIE,
		"FootballZombie", &MakeZombie<FootballZombie>);
```

（324-333 行的"非植物/僵尸动画映射"区保持不动。）

- [ ] **Step 4: 重写 RegisterPlant/RegisterZombie 实现（336-379 行整体替换）**

```cpp
void GameDataManager::RegisterPlant(PlantType type,
	const std::string& enumName,
	const std::string& textureKey,
	AnimationType animType,
	const std::string& animName,
	PlantFactoryFn factory) {
	PlantInfo info;
	info.type = type;
	info.enumName = enumName;
	info.textureKey = textureKey;
	info.animType = animType;
	info.animName = animName;
	info.factory = factory;
	mPlantInfo[type] = info;

	mEnumNameToType[enumName] = type;
	mTextureKeyToType[textureKey] = type;
	mAnimNameToType[animName] = type;
	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册植物身份: " << enumName;
}

void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	ZombieFactoryFn factory) {
	ZombieInfo info;
	info.type = type;
	info.enumName = enumName;
	info.animType = animType;
	info.animName = animName;
	info.factory = factory;
	mZombieInfo[type] = info;

	// 记录动画类型->资源名，以便通过 AnimationType 统一查询
	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册僵尸身份: " << enumName;
}
```

- [ ] **Step 5: 实现 LoadNumbersFromJson（加在 RegisterZombie 实现之后）**

```cpp
bool GameDataManager::LoadNumbersFromJson() {
	std::vector<std::string> errors;
	nlohmann::json data;
	if (!FileManager::LoadJsonFile("./resources/gamedata.json", data)) {
		errors.push_back("resources/gamedata.json 缺失或解析失败");
	}
	else {
		// 单字段读取器：缺失/类型不符记错（不中断，攒齐所有错误一次报告）
		auto readInt = [&errors](const nlohmann::json& e, const char* field,
			const std::string& who, int& out) {
			if (!e.contains(field) || !e[field].is_number_integer()) {
				errors.push_back(who + " 缺字段 \"" + field + "\"（须为整数）");
				return;
			}
			out = e[field].get<int>();
		};
		auto readFloat = [&errors](const nlohmann::json& e, const char* field,
			const std::string& who, float& out) {
			if (!e.contains(field) || !e[field].is_number()) {
				errors.push_back(who + " 缺字段 \"" + field + "\"（须为数字）");
				return;
			}
			out = e[field].get<float>();
		};
		auto readOffset = [&errors](const nlohmann::json& e,
			const std::string& who, Vector& out) {
			if (!e.contains("offset") || !e["offset"].is_array() || e["offset"].size() != 2
				|| !e["offset"][0].is_number() || !e["offset"][1].is_number()) {
				errors.push_back(who + " 缺字段 \"offset\"（须为双元素数字数组）");
				return;
			}
			out = Vector(e["offset"][0].get<float>(), e["offset"][1].get<float>());
		};

		const bool hasPlants = data.contains("plants") && data["plants"].is_object();
		const bool hasZombies = data.contains("zombies") && data["zombies"].is_object();
		if (!hasPlants) errors.push_back("缺 \"plants\" 对象");
		if (!hasZombies) errors.push_back("缺 \"zombies\" 对象");

		if (hasPlants) {
			const nlohmann::json& plants = data["plants"];
			for (auto& pair : mPlantInfo) {
				PlantInfo& info = pair.second;
				if (!plants.contains(info.enumName)) {
					errors.push_back("缺 " + info.enumName + " 的条目");
					continue;
				}
				const nlohmann::json& e = plants[info.enumName];
				readInt(e, "cost", info.enumName, info.SunCost);
				readFloat(e, "cooldown", info.enumName, info.Cooldown);
				readOffset(e, info.enumName, info.offset);
				readFloat(e, "scale", info.enumName, info.scale);
			}
			for (auto& item : plants.items()) {
				if (mEnumNameToType.find(item.key()) == mEnumNameToType.end())
					LOG_WARN("GameData") << "gamedata.json 有未注册的植物键: " << item.key();
			}
		}

		if (hasZombies) {
			const nlohmann::json& zombies = data["zombies"];
			std::unordered_set<std::string> registeredNames;
			for (auto& pair : mZombieInfo) {
				ZombieInfo& info = pair.second;
				registeredNames.insert(info.enumName);
				if (!zombies.contains(info.enumName)) {
					errors.push_back("缺 " + info.enumName + " 的条目");
					continue;
				}
				const nlohmann::json& e = zombies[info.enumName];
				readInt(e, "weight", info.enumName, info.weight);
				readInt(e, "appearWave", info.enumName, info.appearWave);
				readInt(e, "survivalRound", info.enumName, info.survivalRound);
				readOffset(e, info.enumName, info.offset);
				readFloat(e, "scale", info.enumName, info.scale);
			}
			for (auto& item : zombies.items()) {
				if (registeredNames.find(item.key()) == registeredNames.end())
					LOG_WARN("GameData") << "gamedata.json 有未注册的僵尸键: " << item.key();
			}
		}
	}

	if (errors.empty()) return true;

	std::string all;
	for (const auto& msg : errors) {
		LOG_ERROR("GameData") << "gamedata.json: " << msg;
		all += msg;
		all += "\n";
	}
	// AutoTest/无头运行不弹模态框（否则负向测试卡死拿不到退出码）
	if (!TestDriver::GetInstance().IsActive()) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"gamedata.json 配置错误", all.c_str(), nullptr);
	}
	return false;
}
```

- [ ] **Step 6: DebugPrintAll 增补生效数值（580-599 行内的两个循环体）**

植物循环里 `纹理键` 行之前加：

```cpp
		LOG_DEBUG("GameData") << "  阳光: " << info.SunCost << "  冷却: " << info.Cooldown
			<< "s  缩放: " << info.scale;
```

僵尸循环里 `动画类型` 行之前加：

```cpp
		LOG_DEBUG("GameData") << "  权重: " << info.weight << "  出现波: " << info.appearWave
			<< "  生存轮: " << info.survivalRound << "  缩放: " << info.scale;
```

首行 `"========== GameDataManager 硬编码数据 =========="` 改为 `"========== GameDataManager 数据（数值来自 gamedata.json） =========="`。

- [ ] **Step 7: GameAPP.cpp 接入失败路径（179-180 行替换）**

```cpp
	GameDataManager& plantMgr = GameDataManager::GetInstance();
	if (!plantMgr.Initialize()) {
		LOG_ERROR("GameApp") << "GameDataManager 初始化失败：gamedata.json 缺失或校验未通过";
		return false;
	}
```

（`InitializeResourceManager` 返回 false → `Run()` 既有 -6 清理路径接管，无需新代码。）

- [ ] **Step 8: 构建（clang-release，须零警告零错误）**

```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --build --preset clang-release
```
Expected: 构建成功，无任何 warning（尤其 `-Wunused-*`：若报 PlantInfo/ZombieInfo 构造残留未用，回 Step 1 确认已删）。

- [ ] **Step 9: 提交**

```powershell
git add PlantVsZombies/Game/Plant/GameDataManager.h PlantVsZombies/Game/Plant/GameDataManager.cpp PlantVsZombies/GameAPP.cpp
git commit -m @'
GameDataManager数值外置gamedata.json：代码只留身份注册，JSON唯一数值来源+严格校验快速失败

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 3: 回归验证 + 负向测试

**Files:**
- 只读/临时改动：`build\clang-release\resources\gamedata.json`（负向测试改名/改内容，**结束必须还原**）
- 对照：Task 1 的基线 `<scratchpad>\baseline_demo_peashooter\`

**Interfaces:**
- Consumes: Task 2 构建产物 `build\clang-release\PlantsVsZombies.exe`；Task 1 基线。
- Produces: 回归结论（基线逐字节一致 + 4 个 smoke 全绿 + 2 个负向用例按预期拒启动）。

- [ ] **Step 1: 基线回归（同 Seed 逐字节复现）**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
"exit=$LASTEXITCODE"
Pop-Location
$base = "$env:LOCALAPPDATA\Temp\claude\D--PVZ-PlantsVsZombies-PlantVsZombies\28339bd3-15f5-4711-b296-a6f7b0480af9\scratchpad\baseline_demo_peashooter"
$new  = "build\clang-release\autotest\out\demo_peashooter"
Get-ChildItem $base -File | ForEach-Object {
  $b = Get-FileHash $_.FullName; $n = Get-FileHash (Join-Path $new $_.Name)
  "{0}  {1}" -f $_.Name, ($(if ($b.Hash -eq $n.Hash) {'SAME'} else {'DIFF'}))
}
```
Expected: `exit=0`；除 `run.log` 外全部 `SAME`（run.log 含真实耗时戳允许 DIFF；state.json 和所有 PNG 必须 SAME——固定步长 + 同 Seed 的确定性保证）。若 state.json DIFF → 数值迁移抄错了，diff 内容找出哪一项。

- [ ] **Step 2: smoke 覆盖外置数值的路径**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json -Seed 42; "gameplay=$LASTEXITCODE"
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_survival_spawn_round.json -Seed 42; "survival=$LASTEXITCODE"
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_hypnoshroom.json -Seed 42; "hypno=$LASTEXITCODE"
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks.json -Seed 42 -develop; "perks=$LASTEXITCODE"
Pop-Location
```
Expected: 四行全 `=0`。（survival_spawn_round 直接消费外置的 weight/survivalRound；hypnoshroom 消费 cost 75/冷却 25；perks 须带 `-develop`。）

- [ ] **Step 3: 负向测试①——文件缺失拒启动**

```powershell
Push-Location build\clang-release
Rename-Item resources\gamedata.json gamedata.json.bak
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json -Seed 42
"exit=$LASTEXITCODE"
Rename-Item resources\gamedata.json.bak gamedata.json
Pop-Location
```
Expected: `exit=-6`（InitializeResourceManager 失败路径），**且窗口一闪即退、无模态弹窗卡住**（AutoTest 模式守卫生效）。结束后确认 `resources\gamedata.json` 已还原。

- [ ] **Step 4: 负向测试②——缺单个字段拒启动且指名道姓**

```powershell
Push-Location build\clang-release
Copy-Item resources\gamedata.json resources\gamedata.json.bak
$j = Get-Content resources\gamedata.json -Raw -Encoding utf8 | ConvertFrom-Json
$j.plants.PLANT_CHOMPER.PSObject.Properties.Remove('cooldown')
$j | ConvertTo-Json -Depth 5 | Out-File resources\gamedata.json -Encoding utf8
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json -Seed 42
"exit=$LASTEXITCODE"
Move-Item resources\gamedata.json.bak resources\gamedata.json -Force
Pop-Location
```
Expected: `exit=-6`，stderr/日志含 `PLANT_CHOMPER 缺字段 "cooldown"`。结束后确认文件已还原（内容与 Task 1 版本一致，可再跑一次 Step 1 的 hash 对比兜底）。

- [ ] **Step 5: msvc-debug 侧顺手确认**

```powershell
cmake --build --preset msvc-debug
Push-Location build\msvc-debug
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json -Seed 42; "msvc=$LASTEXITCODE"
Pop-Location
```
Expected: `msvc=0`（验证 msvc-debug 那份 gamedata.json 也就位、Debug 配置可跑）。

- [ ] **Step 6: 如有修正则提交修正**

若 Step 1-5 全绿且无代码改动，本任务无提交；若有修正，按 Task 2 Step 9 格式提交。

---

## 完成定义

- [ ] 两个 preset 的 `resources/gamedata.json` 就位且解析通过
- [ ] `clang-release` 零警告构建
- [ ] demo_peashooter 基线除 run.log 外逐字节一致
- [ ] 4 个 smoke 退出码 0
- [ ] 负向①（缺文件）与负向②（缺字段）都以 -6 拒启动，②的错误信息含枚举名+字段名
- [ ] 负向测试后 gamedata.json 已还原
