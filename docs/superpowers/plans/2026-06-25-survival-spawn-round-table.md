# 生存模式刷怪轮次表 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把生存模式 `BuildSurvivalSpawnList` 的硬编码轮次闸门数据化进 `GameDataManager`，实现"第1轮只普通/第2轮普通+路障/第3轮起随机子集池缓慢增长"，并复刻原版的杂兵稀释与旗数递减调制。

**Architecture:** 改动收敛在两个有界切口——①"建池层"(`BuildSurvivalSpawnList` 按轮次表 + 随机子集组装 `mSpawnZombieList`)；②"抽中权重侧"(新增 `GetSurvivalPickWeight`，仅 `GetWeightedRandomZombie` 用，避开本仓库 `weight` 字段一物两用 [既是抽中权重又是点数成本] 的坑)。逐波生成主干、点数预算、血量增长、存档全不动。

**Tech Stack:** C++17，CMake + vcpkg(`x64-windows-static`)，clang-release(0 warning 验证)，AutoTest JSON 脚本 + `dump_state` 闭环。

**Spec:** `docs/superpowers/specs/2026-06-25-survival-spawn-round-table-design.md`

---

## Build & Verify(每个任务结束跑此块，期望 0 warning / 0 error)

```powershell
# 1) 导入 VS 开发环境(Installer 先入 PATH 消除 vswhere 噪声)
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

# 2) 构建(clang-release 是唯一报 -Wreorder/-Wunused/-Wswitch 等诊断的配置)
cmake --build --preset clang-release
```

期望末尾：`PlantsVsZombies.exe` 链接成功，无 `warning:`/`error:`。

---

## File Structure

| 文件 | 职责改动 |
|---|---|
| `PlantVsZombies/Game/Zombie/ZombieType.h` | 移动 `NUM_ZOMBIE_TYPES` 到 `ZOMBIE_FASTPAPER` 后(=7) |
| `PlantVsZombies/Game/Plant/GameDataManager.h` | `ZombieInfo.survivalRound` 字段；`RegisterZombie` 加形参；声明 `GetZombieSurvivalRound` |
| `PlantVsZombies/Game/Plant/GameDataManager.cpp` | `RegisterZombie` 体内赋值；7 处调用填 survivalRound；实现 `GetZombieSurvivalRound` |
| `PlantVsZombies/Game/Board.h` | 8 个新常量；public getter `GetSpawnZombieList`；private 声明 `GetSurvivalPickWeight` |
| `PlantVsZombies/Game/Board.cpp` | `<algorithm>`/`<cmath>`；文件静态 `SurvivalCurveLerp`；实现 `GetSurvivalPickWeight`；`GetWeightedRandomZombie` 改用 pick weight；重写 `BuildSurvivalSpawnList` |
| `PlantVsZombies/Game/AutoTest/TestDriver.cpp` | `dump_state` 加 `survivalRound`+`spawnList`；新增 `force_survival_round` 调试 op |
| `PlantVsZombies/autotest/scripts/smoke_survival_spawn_round.json` | 验收脚本(新建) |

依赖顺序：Task 1 → 2 → 3 → 4，Task 5 依赖 Task 4 的 getter，Task 6 依赖 Task 5。

> **测试说明**：本仓库无单元测试框架，验证手段是"clang-release 0 warning 构建 + AutoTest `dump_state` 断言"。Task 1~5 每个以 Build & Verify 为门禁；Task 6 是行为验收(写脚本→跑→Read JSON 断言)。因 `BuildSurvivalSpawnList` 纯由 `round` 派生，Task 5 加的 `force_survival_round` 调试 op 让逐轮断言确定性且无需打通整轮。

---

### Task 1: 移动 NUM_ZOMBIE_TYPES 到 FASTPAPER 之后

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/ZombieType.h:39-67`

**背景**：枚举前 7 个值(索引 0~6)恰好是全部已实现僵尸；`ZOMBIE_DOOR`(7) 起均未实现。移动后 `NUM_ZOMBIE_TYPES==7`，随机抽取用 `[0, NUM_ZOMBIE_TYPES)` 永不碰未实现僵尸。已核实 `NUM_ZOMBIE_TYPES` 全部用法均为哨兵/边界检查，无数组定长依赖；图鉴走 `GetAllZombieTypes()` 不受影响。

- [ ] **Step 1: 在 ZOMBIE_FASTPAPER 后插入 NUM_ZOMBIE_TYPES**

把第 39 行改为(在其后加一行)：

```cpp
	ZOMBIE_FASTPAPER,	// 加强版读报僵尸

	// ↓ 哨兵：置于已实现僵尸(0~6)之后，使 [0,NUM_ZOMBIE_TYPES) 只覆盖已实现类型，
	//   生存模式随机抽取据此绝不会抽到下方未实现僵尸。
	NUM_ZOMBIE_TYPES,
	ZOMBIE_DOOR,
```

即：`ZOMBIE_FASTPAPER,` 这一行之后、`ZOMBIE_DOOR,` 之前插入 `NUM_ZOMBIE_TYPES,`(连同上方注释)。

- [ ] **Step 2: 删除枚举末尾原有的 NUM_ZOMBIE_TYPES**

把原第 66-67 行：

```cpp
	ZOMBIE_REDEYE_GARGANTUAR,
	NUM_ZOMBIE_TYPES,
};
```

改为：

```cpp
	ZOMBIE_REDEYE_GARGANTUAR,
};
```

- [ ] **Step 3: Build & Verify**

跑 Build & Verify 块。期望 0 warning / 0 error。(此步仅移动枚举值，`ZOMBIE_DOOR`=8 起合法，无逻辑变化。)

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/Zombie/ZombieType.h
git commit -m "refactor(zombie): NUM_ZOMBIE_TYPES 移到 FASTPAPER 后，限定枚举遍历到已实现僵尸"
```

---

### Task 2: GameDataManager 加 survivalRound 轮次表

**Files:**
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.h`(ZombieInfo 结构 ~49-70、RegisterZombie 声明 ~293-299、访问器声明 ~215)
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`(RegisterZombie 实现 ~314-331、7 处调用 ~201-276、访问器实现 ~498-504)

- [ ] **Step 1: ZombieInfo 加字段(.h)**

在 `struct ZombieInfo` 内，`float scale = 1.0f;`(第 57 行)之后加：

```cpp
	float scale = 1.0f;          // 创建时的缩放
	int survivalRound = 0;       // 生存模式最早出场轮(1起；0=不进生存，安全默认)
```

(无需改 ZombieInfo 的两个构造函数——`survivalRound` 用类内默认 0，由 RegisterZombie 构造后赋值，与 `scale`/`factory` 同风格。)

- [ ] **Step 2: RegisterZombie 声明加形参(.h)**

把声明(第 293-299 行)：

```cpp
	void RegisterZombie(ZombieType type,
		const std::string& enumName,
		AnimationType animType,
		const std::string& animName,
		const Vector& offset, int weight, int appearWave,
		float scale,
		ZombieFactoryFn factory);
```

改为(在 `appearWave` 后插入 `int survivalRound`)：

```cpp
	void RegisterZombie(ZombieType type,
		const std::string& enumName,
		AnimationType animType,
		const std::string& animName,
		const Vector& offset, int weight, int appearWave,
		int survivalRound,
		float scale,
		ZombieFactoryFn factory);
```

并在其上方 doc 注释(第 291 行 `@param appearWave ...` 之后)加一行：

```cpp
	 * @param survivalRound 生存模式最早出场轮(1起；0=不进生存)
```

- [ ] **Step 3: 声明访问器(.h)**

在 `int GetZombieAppearWave(ZombieType zombieType) const;`(第 215 行)之后加：

```cpp
	int GetZombieSurvivalRound(ZombieType zombieType) const;
```

- [ ] **Step 4: RegisterZombie 实现加形参 + 赋值(.cpp)**

把实现(第 314-323 行)：

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
```

改为：

```cpp
void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset, int weight, int appearWave,
	int survivalRound,
	float scale,
	ZombieFactoryFn factory) {
	ZombieInfo info(type, enumName, animType, animName, offset, weight, appearWave);
	info.scale = scale;
	info.factory = factory;
	info.survivalRound = survivalRound;
```

- [ ] **Step 5: 7 处 RegisterZombie 调用填 survivalRound(.cpp)**

每处调用的尾行 `\t\t1.0f, &MakeZombie<TYPE>`(即 `scale, factory` 那行)之前，插入一行 `\t\t<N>,    // survivalRound`。逐个对应：

| 锚点行(scale,factory) | 插入行 |
|---|---|
| `		1.0f, &MakeZombie<Zombie>` | `		1,    // survivalRound: 普通` |
| `		1.0f, &MakeZombie<ConeZombie>` | `		2,    // survivalRound: 路障` |
| `		1.0f, &MakeZombie<Polevaulter>` | `		3,    // survivalRound: 撑杆` |
| `		1.0f, &MakeZombie<BucketZombie>` | `		4,    // survivalRound: 铁桶` |
| `		1.0f, &MakeZombie<FastBucketZombie>` | `		5,    // survivalRound: 快速铁桶` |
| `		1.0f, &MakeZombie<PaperZombie>` | `		3,    // survivalRound: 读报` |
| `		1.0f, &MakeZombie<FastPaperZombie>` | `		6,    // survivalRound: 加强读报` |

例：普通僵尸调用从

```cpp
		1000,
		1,
		1.0f, &MakeZombie<Zombie>
	);
```

变为

```cpp
		1000,
		1,
		1,    // survivalRound: 普通
		1.0f, &MakeZombie<Zombie>
	);
```

(插入位置使实参顺序变为 `weight, appearWave, survivalRound, scale, factory`，与新签名一致。)

- [ ] **Step 6: 实现访问器(.cpp)**

在 `GetZombieAppearWave` 实现(第 498-504 行)之后加：

```cpp
int GameDataManager::GetZombieSurvivalRound(ZombieType zombieType) const
{
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.survivalRound;
	return 0;
}
```

- [ ] **Step 7: Build & Verify**

跑 Build & Verify 块。期望 0 warning / 0 error。若漏改某处 RegisterZombie 调用，clang 会报实参数量不匹配——据此补齐。

- [ ] **Step 8: Commit**

```bash
git add PlantVsZombies/Game/Plant/GameDataManager.h PlantVsZombies/Game/Plant/GameDataManager.cpp
git commit -m "feat(survival): GameDataManager 加 survivalRound 轮次表(普通1/路障2/.../加报6)"
```

---

### Task 3: Board 常量 + 调制助手(SurvivalCurveLerp / GetSurvivalPickWeight)

**Files:**
- Modify: `PlantVsZombies/Game/Board.h`(常量 ~55-57、private 声明 ~122、public getter ~147)
- Modify: `PlantVsZombies/Game/Board.cpp`(includes ~18-19、文件静态函数、`GetWeightedRandomZombie` ~475-492)

- [ ] **Step 1: 加 8 个可调常量(Board.h)**

在 `constexpr float SURVIVAL_HP_GROWTH     = 0.05f;`(第 57 行)之后加：

```cpp
// 生存模式出怪池子组装(见 BuildSurvivalSpawnList)
constexpr int SURVIVAL_RANDOM_POOL_START_ROUND = 3;   // 第几轮起改为"普通+随机子集"
constexpr int SURVIVAL_POOL_BASE_EXTRA         = 1;   // 第3轮的随机种类数(除普通外)
constexpr int SURVIVAL_POOL_GROWTH_EVERY       = 2;   // 每多少轮 +1 种(缓慢增长)
// 旗数递减(复刻原版 TodAnimateCurve(18,50,flags,0,15))：深局提前解锁强僵尸；
// 当前阵容(survivalRound 最高6、18旗才起步)下休眠，为未来高 survivalRound 僵尸预留。
constexpr int SURVIVAL_UNLOCK_REDUCE_START_FLAG = 18;
constexpr int SURVIVAL_UNLOCK_REDUCE_END_FLAG   = 50;
constexpr int SURVIVAL_UNLOCK_REDUCE_MAX        = 15;
// 杂兵稀释(复刻原版 Normal→base/10、Cone→base/4，TodAnimateCurve(10,50,...))：仅作用于抽中权重。
constexpr int SURVIVAL_DILUTE_START_FLAG = 10;
constexpr int SURVIVAL_DILUTE_END_FLAG   = 50;
```

- [ ] **Step 2: private 声明 GetSurvivalPickWeight(Board.h)**

在 `inline ZombieType GetCheapestZombie();`(第 122 行)之后加：

```cpp
	// 生存模式"抽中权重"：对 NORMAL/CONE 随轮稀释(仅供 GetWeightedRandomZombie；成本侧仍用 GetZombieWeight)
	int GetSurvivalPickWeight(ZombieType type) const;
```

- [ ] **Step 3: public getter GetSpawnZombieList(Board.h)**

在 `SetZombieSpawnList(...)`(第 147-149 行)之后加：

```cpp
	const std::vector<ZombieType>& GetSpawnZombieList() const { return mSpawnZombieList; }
```

- [ ] **Step 4: 加 includes(Board.cpp)**

把第 18-19 行：

```cpp
#include <unordered_set>
#include <climits>
```

改为：

```cpp
#include <unordered_set>
#include <climits>
#include <algorithm>   // std::max, std::swap
#include <cmath>       // std::lround
```

- [ ] **Step 5: 文件静态 SurvivalCurveLerp(Board.cpp)**

在第 19 行(includes 结束)之后、`Board::Board(...)` 之前插入：

```cpp
// 复刻原版 TodCommon.TodAnimateCurve(..., TodCurves.Linear)：把 round 在 [startRound,endRound]
// 归一化并钳到 [0,1]，在 [fromVal,toVal] 间线性插值，四舍五入取整。
static int SurvivalCurveLerp(int startRound, int endRound, int round,
                             int fromVal, int toVal)
{
	if (endRound <= startRound) return toVal;            // 防 0 除
	float t = static_cast<float>(round - startRound)
	        / static_cast<float>(endRound - startRound);
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return static_cast<int>(std::lround(
		static_cast<float>(fromVal) + t * static_cast<float>(toVal - fromVal)));
}
```

- [ ] **Step 6: 实现 GetSurvivalPickWeight(Board.cpp)**

在 `GetWeightedRandomZombie` 实现(第 475 行)之前插入：

```cpp
int Board::GetSurvivalPickWeight(ZombieType type) const
{
	int base = GameDataManager::GetInstance().GetZombieWeight(type);
	if (!mIsSurvival) return base;
	int flags = mSurvivalRound - 1;   // 已完成轮数(对应原版 survivalFlagsCompleted)
	if (type == ZombieType::ZOMBIE_NORMAL)
		return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
		                         flags, base, base / 10);   // 原版 Normal: base → base/10
	if (type == ZombieType::ZOMBIE_TRAFFIC_CONE)
		return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
		                         flags, base, base / 4);     // 原版 Cone: base → base/4
	return base;
}
```

- [ ] **Step 7: GetWeightedRandomZombie 改用 pick weight(Board.cpp)**

把 `GetWeightedRandomZombie`(第 475-492 行)中两处 `GameDataManager::GetInstance().GetZombieWeight(type)` 改为 `GetSurvivalPickWeight(type)`：

```cpp
inline ZombieType Board::GetWeightedRandomZombie()
{
	if (mSpawnZombieList.empty()) return ZombieType::ZOMBIE_NORMAL;

	int totalWeight = 0;
	for (ZombieType type : mSpawnZombieList)
		totalWeight += GetSurvivalPickWeight(type);

	if (totalWeight <= 0) return mSpawnZombieList[0];

	int randVal = GameRandom::Range(0, totalWeight - 1);
	for (ZombieType type : mSpawnZombieList)
	{
		randVal -= GetSurvivalPickWeight(type);
		if (randVal < 0) return type;
	}
	return mSpawnZombieList[0];
}
```

> 注意：`PickZombieType`(第 511 行)与 `TrySummonZombie`(第 530 行)里的 `GetZombieWeight` 是**点数成本**，**保持不变**——绝不能换成 pick weight，否则普通僵尸成本被稀释、每波数量异常膨胀。

- [ ] **Step 8: Build & Verify**

跑 Build & Verify 块。期望 0 warning / 0 error。

- [ ] **Step 9: Commit**

```bash
git add PlantVsZombies/Game/Board.h PlantVsZombies/Game/Board.cpp
git commit -m "feat(survival): 加池子常量+SurvivalCurveLerp+杂兵稀释(仅抽中权重侧)"
```

---

### Task 4: 重写 BuildSurvivalSpawnList

**Files:**
- Modify: `PlantVsZombies/Game/Board.cpp:675-688`

- [ ] **Step 1: 替换 BuildSurvivalSpawnList 整个函数体**

把(第 675-688 行)：

```cpp
void Board::BuildSurvivalSpawnList(int round)
{
	// 仅使用本作已实现的可生成僵尸类型（0~5）
	mSpawnZombieList.clear();
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);                 // 普通
	if (round >= 2) mSpawnZombieList.push_back(ZombieType::ZOMBIE_TRAFFIC_CONE); // 路障
	if (round >= 3) {
		mSpawnZombieList.push_back(ZombieType::ZOMBIE_POLEVAULTER);       // 撑杆
		mSpawnZombieList.push_back(ZombieType::ZOMBIE_NEWSPAPER);         // 读报
	}
	if (round >= 4) mSpawnZombieList.push_back(ZombieType::ZOMBIE_BUCKET);       // 铁桶
	if (round >= 5) mSpawnZombieList.push_back(ZombieType::ZOMBIE_FASTBUCKET);   // 快速铁桶
	if (round >= 6) mSpawnZombieList.push_back(ZombieType::ZOMBIE_FASTPAPER);    // 加强读报
}
```

整体替换为：

```cpp
void Board::BuildSurvivalSpawnList(int round)
{
	mSpawnZombieList.clear();
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);   // 普通：每轮必有，先固定放入

	// 1) 收集本轮合格候选(排除普通)。旗数递减下调每种僵尸的"有效最早轮次"。
	int flagsCompleted = round - 1;
	int reduction = SurvivalCurveLerp(SURVIVAL_UNLOCK_REDUCE_START_FLAG,
	                                  SURVIVAL_UNLOCK_REDUCE_END_FLAG,
	                                  flagsCompleted, 0, SURVIVAL_UNLOCK_REDUCE_MAX);
	std::vector<ZombieType> candidates;
	for (int i = 0; i < static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES); ++i)
	{
		ZombieType t = static_cast<ZombieType>(i);
		if (t == ZombieType::ZOMBIE_NORMAL) continue;
		int base = GameDataManager::GetInstance().GetZombieSurvivalRound(t);
		if (base < 1) continue;                              // 0 = 不进生存
		int eff = std::max(base - reduction, 1);
		if (eff <= round) candidates.push_back(t);
	}

	// 2) 第 1~2 轮：候选全放(确定性)→ 自然得到 {普通} / {普通,路障}
	if (round < SURVIVAL_RANDOM_POOL_START_ROUND)
	{
		for (ZombieType t : candidates) mSpawnZombieList.push_back(t);
		return;
	}

	// 3) 第 3 轮起：随机抽 extra 种(无放回，部分 Fisher-Yates，绝不死循环)。
	//    等价于"随机抽、抽到未解锁就重抽"，但先排除不合格者再无放回选取。
	int extra = SURVIVAL_POOL_BASE_EXTRA
	          + (round - SURVIVAL_RANDOM_POOL_START_ROUND) / SURVIVAL_POOL_GROWTH_EVERY;
	if (extra > static_cast<int>(candidates.size()))
		extra = static_cast<int>(candidates.size());
	for (int k = 0; k < extra; ++k)
	{
		int j = GameRandom::Range(k, static_cast<int>(candidates.size()) - 1);
		std::swap(candidates[k], candidates[j]);
		mSpawnZombieList.push_back(candidates[k]);
	}
}
```

- [ ] **Step 2: Build & Verify**

跑 Build & Verify 块。期望 0 warning / 0 error。

- [ ] **Step 3: Commit**

```bash
git add PlantVsZombies/Game/Board.cpp
git commit -m "feat(survival): BuildSurvivalSpawnList 改轮次表+随机子集池+旗数递减"
```

---

### Task 5: dump_state 导出 + force_survival_round 调试 op

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`(dump_state ~331-333、op 派发 ~261-266)

- [ ] **Step 1: dump_state 加 survivalRound + spawnList**

在 dump_state 分支中，`out["zombieNumber"] = board->mZombieNumber;`(第 332 行)之后加：

```cpp
		out["zombieNumber"] = board->mZombieNumber;

		out["survivalRound"] = board->mSurvivalRound;
		out["spawnList"] = nlohmann::json::array();
		for (ZombieType t : board->GetSpawnZombieList())
			out["spawnList"].push_back(ZombieTypeName(t));
```

- [ ] **Step 2: 新增 force_survival_round op**

在 `set_sun` 分支(第 261-266 行结束 `}`)之后插入：

```cpp
	if (op == "force_survival_round") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("force_survival_round: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		if (!board->mIsSurvival) { Fail("force_survival_round: 非生存模式关卡"); return false; }
		int round = cmd.value("round", 1);
		if (round < 1) round = 1;
		board->mSurvivalRound = round;
		board->BuildSurvivalSpawnList(round);   // 公有方法，直接按轮重建出怪池
		return true;
	}
```

(`mSurvivalRound`、`mIsSurvival`、`BuildSurvivalSpawnList` 均为 Board public 成员。)

- [ ] **Step 3: Build & Verify**

跑 Build & Verify 块。期望 0 warning / 0 error。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m "test(autotest): dump_state 导出 survivalRound/spawnList + force_survival_round 调试 op"
```

---

### Task 6: AutoTest 验收脚本 + 跑 + 断言

**Files:**
- Create: `PlantVsZombies/autotest/scripts/smoke_survival_spawn_round.json`

- [ ] **Step 1: 写验收脚本**

```json
{
  "commands": [
    { "op": "goto_level", "level": 1000 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "wait_frames", "value": 30 },

    { "op": "force_survival_round", "round": 1 },
    { "op": "dump_state", "name": "round1.json" },
    { "op": "force_survival_round", "round": 2 },
    { "op": "dump_state", "name": "round2.json" },
    { "op": "force_survival_round", "round": 3 },
    { "op": "dump_state", "name": "round3.json" },
    { "op": "force_survival_round", "round": 6 },
    { "op": "dump_state", "name": "round6.json" },
    { "op": "force_survival_round", "round": 13 },
    { "op": "dump_state", "name": "round13.json" },

    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 跑脚本**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_survival_spawn_round.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```

- [ ] **Step 3: Read 各轮 dump 并断言**

产物在 `build\clang-release\autotest\out\smoke_survival_spawn_round\`。用 Read 工具逐个看 `roundN.json` 的 `spawnList` 字段，断言：

| 文件 | 断言(`extra = 1 + (round-3)/2`，第1~2轮全放) |
|---|---|
| `round1.json` | `spawnList == ["ZOMBIE_NORMAL"]` |
| `round2.json` | `spawnList == ["ZOMBIE_NORMAL","ZOMBIE_TRAFFIC_CONE"]` |
| `round3.json` | 含 `ZOMBIE_NORMAL`；size==2(普通+1)；非普通成员 survivalRound≤3(即 ∈ {CONE,POLEVAULTER,NEWSPAPER}) |
| `round6.json` | 含 `ZOMBIE_NORMAL`；size==3(普通+2)；所有非普通成员 survivalRound≤6 |
| `round13.json` | 含 `ZOMBIE_NORMAL`；size==7(extra=6 钳到候选数=全部)；含全部 7 种已实现僵尸；**无任何索引≥7 的未实现僵尸** |

全部满足即通过。任一不符 → 回到对应 Task 修正。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/autotest/scripts/smoke_survival_spawn_round.json
git commit -m "test(autotest): 生存出怪池逐轮断言验收脚本"
```

---

## 验证回顾(实现完成后自检)

- [ ] clang-release 全程 0 warning / 0 error(每个 Task 的 Build & Verify)。
- [ ] Task 6 五轮断言全过。
- [ ] 未碰逐波生成主干(`PickZombieType`/`TrySummonZombie` 的成本侧 `GetZombieWeight` 原样)、点数预算、血量增长、存档。
- [ ] 主人 WIP 的 `PeaBullet.h`/`PuffBullet.h`/`SnowPea.h` 未被本次改动牵连。
