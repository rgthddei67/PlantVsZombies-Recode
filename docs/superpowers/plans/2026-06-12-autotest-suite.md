# AutoTest 脚本自动驾驶套件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 游戏增加 `-AutoTest script.json` 启动参数，按 JSON 脚本自动进关卡/选卡/种植/出怪/截图/导出状态后退出，实现 Claude「改代码→msbuild→跑脚本→Read 截图验证」全自动闭环。

**Architecture:** 新增单例 `TestDriver`（`Game/AutoTest/`）挂在 `GameAPP::Run` 主循环 `sceneManager.Update()` 之后，每帧顺序消化 JSON 命令队列；截图走 `VulkanRenderer::EndFrame` present 前的 swapchain 回读（交换链 usage 加 `TRANSFER_SRC`）；AutoTest 模式下 `GameInfoSaver` 全部读写短路，保证测试不污染玩家存档、每次进关都是确定性的全新关卡。

**Tech Stack:** C++17 / nlohmann::json（已有依赖）/ SDL2_image `IMG_SavePNG`（已有依赖）/ Vulkan 1.3 / MSBuild x64。

**对应 spec:** `docs/superpowers/specs/2026-06-12-autotest-suite-design.md`

---

## 全局约定（每个 Task 都用到）

**构建命令**（主人已授权 Claude 直接编译；PowerShell，在仓库根 `D:\PVZ\PlantsVsZombies\PlantVsZombies` 执行）：

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild .\PlantsVsZombies.sln /p:Configuration=Release /p:Platform=x64 /m /v:m
```

预期：`0 个错误`，约 30 秒。产物 `x64\Release\PlantsVsZombies.exe`。

**运行命令**（工作目录必须是 exe 所在目录，resources/saves 都是相对路径）：

```powershell
Push-Location x64\Release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本>.json
$LASTEXITCODE   # 0 = 成功
Pop-Location
```

产物落在 `x64\Release\autotest\out\<脚本名>\`（PNG / state JSON / run.log）。

**测试基础设施说明：** 本项目无单元测试框架（历史约束，见 backlog #5）。本计划的"测试"= 用最小脚本驱动真实 exe + 检查退出码/输出文件，这是端到端验证，每个 Task 的验证步骤都可由 Claude 独立完成。

**文件总览：**

| 文件 | 动作 | 职责 |
|---|---|---|
| `PlantVsZombies/Game/AutoTest/TestDriver.h/.cpp` | 新建 | 脚本解析 + 命令分发 + 类型名表 + run.log |
| `PlantVsZombies/main.cpp` | 修改 | `-AutoTest` / `-Seed` 解析，退出码透传 |
| `PlantVsZombies/GameAPP.h` | 修改 | `mAutoTestMode` 标志 + `GetVulkanRenderer()` 访问器 |
| `PlantVsZombies/GameApp.cpp` | 修改 | 主循环挂 `TestDriver::Update()` |
| `PlantVsZombies/GameRandom.h` | 修改 | `SetSeed(uint64)` |
| `PlantVsZombies/GameInfoSaver.cpp` | 修改 | AutoTest 模式短路读写 |
| `PlantVsZombies/Game/GameScene.h` | 修改 | `GetBoard()` / `GetChooseCardUI()` / `IsChooseCardReady()` |
| `PlantVsZombies/Game/ChooseCardUI.h/.cpp` | 修改 | `FindCardByType()` |
| `PlantVsZombies/Renderer/VulkanContext.cpp` | 修改 | 交换链 usage 加 TRANSFER_SRC |
| `PlantVsZombies/Renderer/VulkanRenderer.h/.cpp` | 修改 | `RequestCapture()` + EndFrame 回读 + 写 PNG |
| `PlantVsZombies/PlantsVsZombies.vcxproj` / `.filters` | 修改 | 注册新文件 |
| `autotest/scripts/demo_peashooter.json` | 新建 | 端到端验收脚本 |
| `CLAUDE.md` | 修改 | 构建限制解除 + AutoTest 用法 |

---

### Task 1: AutoTest 模式基建（标志、参数解析、种子、存档隔离）

**Files:**
- Modify: `PlantVsZombies/GameAPP.h:88-90`（static 标志区）
- Modify: `PlantVsZombies/main.cpp`
- Modify: `PlantVsZombies/GameRandom.h:123`（RandomizeSeed 旁）
- Modify: `PlantVsZombies/GameInfoSaver.cpp`（四个函数开头）

- [ ] **Step 1: GameAPP.h 加模式标志**

在 `GameAPP.h` 的 `inline static bool mDisableInstancePath = false;`（90 行附近）后加：

```cpp
	inline static bool mAutoTestMode = false;         // -AutoTest 自动化测试模式：禁存档读写、由 TestDriver 驱动
```

- [ ] **Step 2: GameRandom.h 加 SetSeed**

在 `GameRandom.h` 的 `RandomizeSeed()`（123 行）后加：

```cpp
	// AutoTest：固定种子保证可复现
	static void SetSeed(unsigned long long seed) {
		engine.seed(seed);
	}
```

- [ ] **Step 3: main.cpp 解析 -AutoTest / -Seed（本 Task 先只设标志，TestDriver 在 Task 2 接入）**

`main.cpp` 的参数循环改为（保留原有 -Debug / -NoInstance 分支不动，新增两个分支；注意 `i + 1 < argc` 防越界）：

```cpp
	std::string autoTestScript;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "-Debug" || arg == "-debug")
		{
			GameAPP::mDebugMode = true;
			GameAPP::mShowColliders = true;
			LOG_DEBUG("Main") << "Debug模式已启用";
		}
		else if (arg == "-NoInstance" || arg == "-noinstance")
		{
			GameAPP::mDisableInstancePath = true;
			LOG_DEBUG("Main") << "GPU instance path 已禁用 (A/B baseline)";
		}
		else if ((arg == "-AutoTest" || arg == "-autotest") && i + 1 < argc)
		{
			autoTestScript = argv[++i];
			GameAPP::mAutoTestMode = true;
		}
		else if ((arg == "-Seed" || arg == "-seed") && i + 1 < argc)
		{
			GameRandom::SetSeed(std::stoull(argv[++i]));
		}
	}
```

（`GameRandom::RandomizeSeed()` 原调用在参数循环之前，保持不动 —— `-Seed` 在其后覆盖即可。）

- [ ] **Step 4: GameInfoSaver 存档隔离**

`GameInfoSaver.cpp` 中四个函数体首行各加一个短路（需 `#include "GameApp.h"`，该 cpp 若已包含则跳过）：

```cpp
bool GameInfoSaver::SavePlayerInfo() {
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：不碰玩家存档
	...
bool GameInfoSaver::LoadPlayerInfo() {
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：全默认状态，保证确定性
	...
bool GameInfoSaver::SaveLevelData(Board* board, CardSlotManager* manager) {
	if (GameAPP::mAutoTestMode) return true;
	...
bool GameInfoSaver::LoadLevelData(Board* board, CardSlotManager* manager) {
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：永远全新关卡（不跳过选卡流程）
	...
```

`DeleteLevelData` 不动（AutoTest 下不会触发重开）。

- [ ] **Step 5: 构建验证**

跑全局约定的 msbuild 命令。预期 `0 个错误`。

- [ ] **Step 6: 运行回归（无参数启动不受影响）**

```powershell
Push-Location x64\Release; Start-Process .\PlantsVsZombies.exe; Start-Sleep -Seconds 5; Stop-Process -Name PlantsVsZombies -Force -Confirm:$false; Pop-Location
```

预期：游戏正常弹窗进主菜单（5 秒后被关闭）。

- [ ] **Step 7: Commit**

```powershell
git add PlantVsZombies/GameAPP.h PlantVsZombies/main.cpp PlantVsZombies/GameRandom.h PlantVsZombies/GameInfoSaver.cpp
git commit -m "feat(autotest): AutoTest 模式标志/-Seed/存档隔离基建"
```

---

### Task 2: TestDriver 骨架 + 基础命令（wait / quit / goto_level / set_timescale）

**Files:**
- Create: `PlantVsZombies/Game/AutoTest/TestDriver.h`
- Create: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Modify: `PlantVsZombies/main.cpp`（接入 LoadScript + 退出码）
- Modify: `PlantVsZombies/GameApp.cpp:346`（主循环挂钩）
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj` / `.filters`
- Test: `autotest/scripts/smoke_quit.json`

- [ ] **Step 1: 写验证脚本（先于实现，作为本 Task 的"失败测试"）**

新建 `autotest/scripts/smoke_quit.json`：

```json
{
  "commands": [
    { "op": "wait_frames", "value": 10 },
    { "op": "goto_level", "level": 1 },
    { "op": "wait_seconds", "value": 1.0 },
    { "op": "quit" }
  ]
}
```

此刻运行 `.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json` 的预期行为：游戏照常进主菜单且**永不退出**（TestDriver 尚不存在）—— 手动关掉即可，这就是"失败状态"。

- [ ] **Step 2: 新建 TestDriver.h**

```cpp
#pragma once
#ifndef _TESTDRIVER_H
#define _TESTDRIVER_H
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

// -AutoTest 脚本自动驾驶：解析 JSON 命令队列，挂在主循环每帧推进。
// 设计文档：docs/superpowers/specs/2026-06-12-autotest-suite-design.md
class TestDriver {
public:
	static TestDriver& GetInstance();

	// 解析脚本并创建输出目录 ./autotest/out/<脚本名>/。失败返回 false（main 直接退出）。
	bool LoadScript(const std::string& path);

	bool IsActive() const { return mActive; }
	int  ExitCode() const { return mExitCode; }

	// 每帧调用（GameAPP::Run 中 sceneManager.Update() 之后）。未激活时立即返回。
	void Update();

	const std::string& OutDir() const { return mOutDir; }

private:
	TestDriver() = default;

	// 执行当前命令。返回 true = 已完成可推进下一条；false = 等待中（下帧重试）。
	bool ExecuteCurrent();

	void Fail(const std::string& reason);   // 记日志、退出码=1、结束游戏循环
	void Finish();                          // 全部命令跑完，正常收尾
	void Log(const std::string& msg);       // 写 run.log（带帧号）并 flush

	bool mActive = false;
	int  mExitCode = 0;
	size_t mIndex = 0;
	std::vector<nlohmann::json> mCommands;
	std::string mOutDir;
	std::ofstream mRunLog;
	unsigned long long mFrame = 0;

	// 等待型命令的逐命令状态（推进到下一条时清零）
	float mWaitAccum = 0.0f;     // wait_seconds 已累计（缩放后游戏时间）
	int   mFramesLeft = -1;      // wait_frames 剩余（-1 = 未初始化）
	float mTimeoutAccum = 0.0f;  // 当前命令已耗时（未缩放，墙钟语义），超 timeout 判失败
	bool  mBreakFrame = false;   // screenshot 等需要"本帧到此为止"的命令置位
};

#endif
```

- [ ] **Step 3: 新建 TestDriver.cpp（骨架 + 4 个基础命令）**

```cpp
#include "TestDriver.h"
#include "../../GameAPP.h"
#include "../../DeltaTime.h"
#include "../../Logger.h"
#include "../SceneManager.h"
#include <filesystem>

TestDriver& TestDriver::GetInstance() {
	static TestDriver instance;
	return instance;
}

bool TestDriver::LoadScript(const std::string& path) {
	std::ifstream f(path);
	if (!f) {
		LOG_ERROR("AutoTest") << "无法打开脚本: " << path;
		return false;
	}
	nlohmann::json j;
	try {
		f >> j;
	}
	catch (const std::exception& e) {
		LOG_ERROR("AutoTest") << "脚本 JSON 解析失败: " << e.what();
		return false;
	}
	if (!j.contains("commands") || !j["commands"].is_array() || j["commands"].empty()) {
		LOG_ERROR("AutoTest") << "脚本缺少非空 commands 数组";
		return false;
	}
	for (const auto& c : j["commands"]) mCommands.push_back(c);

	std::error_code ec;
	mOutDir = (std::filesystem::path("./autotest/out") /
		std::filesystem::path(path).stem()).string();
	std::filesystem::create_directories(mOutDir, ec);
	mRunLog.open(mOutDir + "/run.log", std::ios::trunc);

	mActive = true;
	Log("script loaded: " + path + " (" + std::to_string(mCommands.size()) + " commands)");
	return true;
}

void TestDriver::Log(const std::string& msg) {
	if (mRunLog.is_open()) {
		mRunLog << "[f" << mFrame << "] " << msg << "\n";
		mRunLog.flush();
	}
	LOG_INFO("AutoTest") << msg;   // Release 编译期裁掉，run.log 才是权威记录
}

void TestDriver::Fail(const std::string& reason) {
	const std::string op = (mIndex < mCommands.size())
		? mCommands[mIndex].value("op", "?") : "?";
	Log("FAIL at cmd#" + std::to_string(mIndex) + " (" + op + "): " + reason);
	LOG_ERROR("AutoTest") << "FAIL at cmd#" << mIndex << " (" << op << "): " << reason;
	mExitCode = 1;
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Finish() {
	Log("script finished OK");
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Update() {
	if (!mActive) return;
	++mFrame;
	mBreakFrame = false;
	int guard = 0;
	while (mActive && !mBreakFrame && mIndex < mCommands.size()) {
		if (!ExecuteCurrent()) break;          // 等待中，下帧重试
		Log("done cmd#" + std::to_string(mIndex) + " (" +
			mCommands[mIndex].value("op", "?") + ")");
		++mIndex;
		mWaitAccum = 0.0f;
		mFramesLeft = -1;
		mTimeoutAccum = 0.0f;
		if (++guard > 64) break;               // 单帧推进上限，防脚本自旋
	}
	if (mActive && mIndex >= mCommands.size()) Finish();
}

bool TestDriver::ExecuteCurrent() {
	const auto& cmd = mCommands[mIndex];
	const std::string op = cmd.value("op", "");

	// 等待型命令的超时看门狗（墙钟语义，不受 timescale 影响）
	mTimeoutAccum += DeltaTime::GetUnscaledDeltaTime();
	const float timeout = cmd.value("timeout", 15.0f);
	if (mTimeoutAccum > timeout) {
		Fail("timeout (" + std::to_string(timeout) + "s)");
		return false;
	}

	if (op == "wait_seconds") {
		mWaitAccum += DeltaTime::GetDeltaTime();
		return mWaitAccum >= cmd.value("value", 0.0f);
	}
	if (op == "wait_frames") {
		if (mFramesLeft < 0) mFramesLeft = cmd.value("value", 0);
		return --mFramesLeft < 0;
	}
	if (op == "goto_level") {
		if (!cmd.contains("level")) { Fail("goto_level 缺 level 字段"); return false; }
		auto& sm = SceneManager::GetInstance();
		sm.SetGlobalData("EnterLevel", std::to_string(cmd["level"].get<int>()));
		if (!sm.SwitchTo("GameScene")) { Fail("SwitchTo(GameScene) 失败"); return false; }
		return true;
	}
	if (op == "set_timescale") {
		DeltaTime::SetTimeScale(cmd.value("value", 1.0f));
		return true;
	}
	if (op == "quit") {
		Finish();
		return false;   // Finish 已停机，不再推进
	}

	Fail("未知命令 op=\"" + op + "\"");
	return false;
}
```

（玩法/截图/状态命令在 Task 3-5 往 `ExecuteCurrent` 里追加分支。）

- [ ] **Step 4: main.cpp 接入**

`main.cpp` 头部加 `#include "./Game/AutoTest/TestDriver.h"`；参数循环之后、`Run()` 之前加：

```cpp
	if (GameAPP::mAutoTestMode) {
		if (!TestDriver::GetInstance().LoadScript(autoTestScript)) {
			CrashHandler::Cleanup();
			return 100;   // 脚本解析失败
		}
	}
```

`Run()` 之后、`return result;` 之前加：

```cpp
	if (GameAPP::mAutoTestMode && result == 0) {
		result = TestDriver::GetInstance().ExitCode();
	}
```

- [ ] **Step 5: GameApp.cpp 主循环挂钩**

`GameApp.cpp` 头部加 `#include "./Game/AutoTest/TestDriver.h"`。`Run()` 循环里（346-347 行附近）：

```cpp
			sceneManager.Update();
			CursorManager::GetInstance().Update();
```

之后（仍在 `PROFILE_SCOPE("B.SceneUpdate_total")` 块内）加：

```cpp
			if (TestDriver::GetInstance().IsActive()) {
				TestDriver::GetInstance().Update();
			}
```

- [ ] **Step 6: 注册进工程**

`PlantsVsZombies.vcxproj`：在 `<ClCompile Include="GameMonitor.cpp" />`（27 行）同一 ItemGroup 内加：

```xml
    <ClCompile Include="Game\AutoTest\TestDriver.cpp" />
```

在 `<ClInclude Include="GameMonitor.h" />`（109 行）同一 ItemGroup 内加：

```xml
    <ClInclude Include="Game\AutoTest\TestDriver.h" />
```

`PlantsVsZombies.vcxproj.filters`：参照 `Game\Board.cpp` 的写法（85-87 / 327-329 行）：

```xml
    <ClCompile Include="Game\AutoTest\TestDriver.cpp">
      <Filter>Code\Game</Filter>
    </ClCompile>
```
```xml
    <ClInclude Include="Game\AutoTest\TestDriver.h">
      <Filter>Code\Game</Filter>
    </ClInclude>
```

- [ ] **Step 7: 构建 + 跑 smoke 脚本**

msbuild（预期 0 错误），然后：

```powershell
Push-Location x64\Release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json
"exit=$LASTEXITCODE"
Get-Content .\autotest\out\smoke_quit\run.log
Pop-Location
```

预期：游戏自动进关卡 1（能看到开场镜头）、约 1 秒后自动退出；`exit=0`；run.log 含 4 条 `done cmd#` + `script finished OK`。

另跑一次反例：`.\PlantsVsZombies.exe -AutoTest 不存在.json` → 预期 `exit=100`。

- [ ] **Step 8: Commit**

```powershell
git add PlantVsZombies/Game/AutoTest PlantVsZombies/main.cpp PlantVsZombies/GameApp.cpp PlantVsZombies/PlantsVsZombies.vcxproj PlantVsZombies/PlantsVsZombies.vcxproj.filters autotest/scripts/smoke_quit.json
git commit -m "feat(autotest): TestDriver 骨架 + wait/goto_level/set_timescale/quit"
```

---

### Task 3: 玩法命令（choose_cards / set_sun / plant / spawn_zombie / wait_state）

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.h`（public 区，55 行 `ChooseCardComplete` 附近）
- Modify: `PlantVsZombies/Game/ChooseCardUI.h`（public 区）/ `ChooseCardUI.cpp`
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Test: `autotest/scripts/smoke_gameplay.json`

- [ ] **Step 1: 写验证脚本**

新建 `autotest/scripts/smoke_gameplay.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER", "PLANT_SUNFLOWER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "set_sun", "value": 2000 },
    { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 2, "col": 1 },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 2, "x": 900 },
    { "op": "wait_seconds", "value": 3.0 },
    { "op": "quit" }
  ]
}
```

此刻运行预期失败：`exit=1`，run.log 报 `未知命令 op="choose_cards"`。

- [ ] **Step 2: GameScene.h 加访问器**

`GameScene.h` public 区（`void ChooseCardComplete();` 之后）加：

```cpp
	// ---- AutoTest 钩子（只读访问，不改变任何流程）----
	Board* GetBoard() const { return mBoard.get(); }
	ChooseCardUI* GetChooseCardUI() const { return mChooseCardUI; }
	// 选卡界面是否就绪（卡牌已铺开、"一起摇滚吧"可点）
	bool IsChooseCardReady() const {
		return mCurrentStage == IntroStage::COMPLETE && mChooseCardUI != nullptr;
	}
```

- [ ] **Step 3: ChooseCardUI 加 FindCardByType**

`ChooseCardUI.h` public 区加声明：

```cpp
	// 按植物类型查找选卡界面中的卡牌（AutoTest 程序化选卡用）；找不到返回 nullptr
	Card* FindCardByType(PlantType type);
```

`ChooseCardUI.cpp` 加实现（`AddAllCard` 之后）：

```cpp
Card* ChooseCardUI::FindCardByType(PlantType type) {
	for (auto* card : mCards) {
		if (!card) continue;
		auto* comp = card->GetCardComponent();
		if (comp && comp->GetPlantType() == type) return card;
	}
	return nullptr;
}
```

- [ ] **Step 4: TestDriver.cpp 加类型名表 + 玩法命令**

头部追加 include：

```cpp
#include "../GameScene.h"
#include "../ChooseCardUI.h"
#include "../Board.h"
#include "../Card.h"
#include "../CardComponent.h"
#include "../Plant/PlantType.h"
#include "../Zombie/ZombieType.h"
#include <unordered_map>
```

文件内（匿名 namespace）加名表与辅助函数。**名表 = 枚举标识符原文**，新增植物/僵尸时照抄枚举名加一行即可：

```cpp
namespace {
#define PT(n) { #n, PlantType::n }
	const std::unordered_map<std::string, PlantType> kPlantNames = {
		PT(PLANT_PEASHOOTER), PT(PLANT_SUNFLOWER), PT(PLANT_CHERRYBOMB), PT(PLANT_WALLNUT),
		PT(PLANT_POTATOMINE), PT(PLANT_SNOWPEA), PT(PLANT_CHOMPER), PT(PLANT_REPEATER),
		PT(PLANT_PUFFSHROOM), PT(PLANT_SUNSHROOM), PT(PLANT_FUMESHROOM), PT(PLANT_GRAVEBUSTER),
		PT(PLANT_HYPNOSHROOM), PT(PLANT_SCAREDYSHROOM), PT(PLANT_ICESHROOM), PT(PLANT_DOOMSHROOM),
		PT(PLANT_LILYPAD), PT(PLANT_SQUASH), PT(PLANT_THREEPEATER), PT(PLANT_TANGLEKELP),
		PT(PLANT_JALAPENO), PT(PLANT_SPIKEWEED), PT(PLANT_TORCHWOOD), PT(PLANT_TALLNUT),
		PT(PLANT_SEASHROOM), PT(PLANT_PLANTERN), PT(PLANT_CACTUS), PT(PLANT_BLOVER),
		PT(PLANT_SPLITPEA), PT(PLANT_STARFRUIT), PT(PLANT_PUMPKINSHELL), PT(PLANT_MAGNETSHROOM),
		PT(PLANT_CABBAGEPULT), PT(PLANT_FLOWERPOT), PT(PLANT_KERNELPULT), PT(PLANT_INSTANT_COFFEE),
		PT(PLANT_GARLIC), PT(PLANT_UMBRELLA), PT(PLANT_MARIGOLD), PT(PLANT_MELONPULT),
		PT(PLANT_GATLINGPEA), PT(PLANT_TWINSUNFLOWER), PT(PLANT_GLOOMSHROOM), PT(PLANT_CATTAIL),
		PT(PLANT_WINTERMELON), PT(PLANT_GOLD_MAGNET), PT(PLANT_SPIKEROCK), PT(PLANT_COBCANNON),
		PT(PLANT_IMITATER), PT(PLANT_EXPLODE_O_NUT), PT(PLANT_GIANT_WALLNUT), PT(PLANT_SPROUT),
		PT(PLANT_LEFTPEATER),
	};
#undef PT
#define ZT(n) { #n, ZombieType::n }
	const std::unordered_map<std::string, ZombieType> kZombieNames = {
		ZT(ZOMBIE_NORMAL), ZT(ZOMBIE_TRAFFIC_CONE), ZT(ZOMBIE_POLEVAULTER), ZT(ZOMBIE_BUCKET),
		ZT(ZOMBIE_FASTBUCKET), ZT(ZOMBIE_NEWSPAPER), ZT(ZOMBIE_FASTPAPER), ZT(ZOMBIE_DOOR),
		ZT(ZOMBIE_FOOTBALL), ZT(ZOMBIE_DANCER), ZT(ZOMBIE_BACKUP_DANCER), ZT(ZOMBIE_DUCKY_TUBE),
		ZT(ZOMBIE_SNORKEL), ZT(ZOMBIE_ZAMBONI), ZT(ZOMBIE_BOBSLED), ZT(ZOMBIE_DOLPHIN_RIDER),
		ZT(ZOMBIE_JACK_IN_THE_BOX), ZT(ZOMBIE_BALLOON), ZT(ZOMBIE_DIGGER), ZT(ZOMBIE_POGO),
		ZT(ZOMBIE_YETI), ZT(ZOMBIE_BUNGEE), ZT(ZOMBIE_LADDER), ZT(ZOMBIE_CATAPULT),
		ZT(ZOMBIE_GARGANTUAR), ZT(ZOMBIE_IMP), ZT(ZOMBIE_BOSS), ZT(ZOMBIE_PEA_HEAD),
		ZT(ZOMBIE_WALLNUT_HEAD), ZT(ZOMBIE_JALAPENO_HEAD), ZT(ZOMBIE_GATLING_HEAD),
		ZT(ZOMBIE_SQUASH_HEAD), ZT(ZOMBIE_TALLNUT_HEAD), ZT(ZOMBIE_REDEYE_GARGANTUAR),
	};
#undef ZT
	const std::unordered_map<std::string, BoardState> kBoardStateNames = {
		{ "CHOOSE_CARD", BoardState::CHOOSE_CARD }, { "GAME", BoardState::GAME },
		{ "LOSE_GAME", BoardState::LOSE_GAME }, { "WIN", BoardState::WIN },
		{ "NONE", BoardState::NONE },
	};

	std::string PlantTypeName(PlantType t) {
		for (const auto& [k, v] : kPlantNames) if (v == t) return k;
		return "UNKNOWN_PLANT_" + std::to_string(static_cast<int>(t));
	}
	std::string ZombieTypeName(ZombieType t) {
		for (const auto& [k, v] : kZombieNames) if (v == t) return k;
		return "UNKNOWN_ZOMBIE_" + std::to_string(static_cast<int>(t));
	}
	std::string BoardStateName(BoardState s) {
		for (const auto& [k, v] : kBoardStateNames) if (v == s) return k;
		return "UNKNOWN";
	}

	GameScene* CurrentGameScene() {
		return dynamic_cast<GameScene*>(SceneManager::GetInstance().GetCurrentScene());
	}
}
```

`ExecuteCurrent` 在 `if (op == "quit")` 之前追加分支：

```cpp
	if (op == "choose_cards") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->IsChooseCardReady()) return false;   // 等开场动画铺完卡
		ChooseCardUI* ui = gs->GetChooseCardUI();
		for (const auto& nameJson : cmd.value("cards", nlohmann::json::array())) {
			const std::string name = nameJson.get<std::string>();
			auto it = kPlantNames.find(name);
			if (it == kPlantNames.end()) { Fail("未知植物类型: " + name); return false; }
			Card* card = ui->FindCardByType(it->second);
			if (!card) {                       // 玩家未拥有该卡：AutoTest 直接补一张
				ui->AddCard(it->second);
				card = ui->FindCardByType(it->second);
			}
			if (!card) { Fail("选卡失败（AddCard 后仍找不到）: " + name); return false; }
			if (!ui->IsCardSelected(card)) ui->ToggleCardSelection(card);
		}
		gs->ChooseCardComplete();
		return true;
	}
	if (op == "wait_state") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) return false;
		auto it = kBoardStateNames.find(cmd.value("state", ""));
		if (it == kBoardStateNames.end()) { Fail("未知 BoardState: " + cmd.value("state", "")); return false; }
		return gs->GetBoard()->mBoardState == it->second;
	}
	if (op == "set_sun") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("set_sun: 不在 GameScene 或 Board 为空"); return false; }
		gs->GetBoard()->mSun = std::min(cmd.value("value", 0), MAX_SUN);
		return true;
	}
	if (op == "plant") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("plant: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kPlantNames.find(cmd.value("type", ""));
		if (it == kPlantNames.end()) { Fail("未知植物类型: " + cmd.value("type", "")); return false; }
		Plant* p = gs->GetBoard()->CreatePlant(it->second,
			cmd.value("row", 0), cmd.value("col", 0));
		if (!p) { Fail("CreatePlant 返回空（格子非法或被占？）"); return false; }
		return true;
	}
	if (op == "spawn_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("spawn_zombie: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kZombieNames.find(cmd.value("type", ""));
		if (it == kZombieNames.end()) { Fail("未知僵尸类型: " + cmd.value("type", "")); return false; }
		Zombie* z = gs->GetBoard()->CreateZombie(it->second,
			cmd.value("row", 0), cmd.value("x", 900.0f));
		if (!z) { Fail("CreateZombie 返回空"); return false; }
		return true;
	}
```

并在头部 include 区补 `#include "../Plant/Plant.h"`、`#include "../Zombie/Zombie.h"`、`#include <algorithm>`。

- [ ] **Step 5: 构建 + 跑 smoke_gameplay**

msbuild（预期 0 错误），然后：

```powershell
Push-Location x64\Release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json
"exit=$LASTEXITCODE"
Get-Content .\autotest\out\smoke_gameplay\run.log
Pop-Location
```

预期：自动进关卡、自动选两张卡并点"摇滚"、镜头回移、种下豌豆射手、右侧出普通僵尸、3 秒后退出；`exit=0`；run.log 8 条 done。肉眼可顺带确认窗口里流程跑对。

- [ ] **Step 6: Commit**

```powershell
git add PlantVsZombies/Game/GameScene.h PlantVsZombies/Game/ChooseCardUI.h PlantVsZombies/Game/ChooseCardUI.cpp PlantVsZombies/Game/AutoTest/TestDriver.cpp autotest/scripts/smoke_gameplay.json
git commit -m "feat(autotest): choose_cards/wait_state/set_sun/plant/spawn_zombie 玩法命令"
```

---

### Task 4: Vulkan 截图（swapchain 回读 → PNG）

**Files:**
- Modify: `PlantVsZombies/Renderer/VulkanContext.cpp:315`
- Modify: `PlantVsZombies/Renderer/VulkanRenderer.h` / `VulkanRenderer.cpp:214-273`（EndFrame）
- Modify: `PlantVsZombies/GameAPP.h`（renderer 访问器）
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（screenshot 命令）
- Test: `autotest/scripts/smoke_screenshot.json`

- [ ] **Step 1: 写验证脚本**

新建 `autotest/scripts/smoke_screenshot.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "wait_seconds", "value": 1.0 },
    { "op": "screenshot", "name": "menu_into_level.png" },
    { "op": "wait_frames", "value": 5 },
    { "op": "quit" }
  ]
}
```

此刻运行预期失败：`exit=1`，run.log 报 `未知命令 op="screenshot"`。

- [ ] **Step 2: 交换链 usage 加 TRANSFER_SRC**

`VulkanContext.cpp:315`：

```cpp
		sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
```

改为：

```cpp
		// TRANSFER_SRC：AutoTest 截图回读用（核心规范保证 color attachment 可加 transfer 用途）
		sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
```

- [ ] **Step 3: VulkanRenderer.h 加捕获接口**

头部加 `#include <string>`。public 区（`EndFrame()` 声明后）加：

```cpp
		// AutoTest 截图：登记后下一次 EndFrame 在 present 之前把 swapchain 图像回读并写 PNG。
		// 截图帧会同步等待本帧 fence（约十几 ms 停顿），仅用于测试场景。
		void RequestCapture(const std::string& pngPath) { mCapturePath = pngPath; }
```

private 区（`mSwapchainNeedsRebuild` 后）加：

```cpp
		std::string mCapturePath;   // 非空 = 本帧 EndFrame 执行回读
		void WriteCapturePng(const void* bgraPixels, uint32_t w, uint32_t h);
```

- [ ] **Step 4: VulkanRenderer.cpp 实现回读**

头部追加：

```cpp
#include "VulkanBuffer.h"
#include <SDL2/SDL_image.h>
#include <vector>
#include <cstring>
```

`EndFrame()`（214 行起）中，把原来的单个 barrier 调用：

```cpp
	ImageBarrier2(frame.cmdBuffer, swapImage,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
```

替换为：

```cpp
	// AutoTest 截图：present 之后图像归显示引擎所有，回读必须发生在 present 之前。
	VulkanBuffer captureBuf;   // 函数内 RAII：submit 后等 fence 再读，函数尾析构
	const bool capturing = !mCapturePath.empty();
	const VkExtent2D captureExt = mCtx->SwapchainExtent();
	if (capturing) {
		ImageBarrier2(frame.cmdBuffer, swapImage,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		if (captureBuf.Create(mCtx,
			VkDeviceSize(captureExt.width) * captureExt.height * 4,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT, /*hostVisible=*/true)) {
			VkBufferImageCopy region{};   // bufferRowLength=0 → 行紧密排列
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent = { captureExt.width, captureExt.height, 1 };
			vkCmdCopyImageToBuffer(frame.cmdBuffer, swapImage,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, captureBuf.Handle(), 1, &region);
		}
		else {
			LOG_ERROR("VulkanRenderer") << "截图 buffer 创建失败，本次截图跳过";
		}
		ImageBarrier2(frame.cmdBuffer, swapImage,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	else {
		ImageBarrier2(frame.cmdBuffer, swapImage,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
```

再在 present 错误处理之后、`mFrameActive = false;`（269 行）之前加：

```cpp
	if (capturing) {
		// 等本帧 GPU 完成（fence 不 reset —— BeginFrame 自己管理 wait+reset）
		vkWaitForFences(mCtx->Device(), 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
		if (captureBuf.MappedPtr()) {
			WriteCapturePng(captureBuf.MappedPtr(), captureExt.width, captureExt.height);
		}
		mCapturePath.clear();
	}
```

文件末尾（`CurrentCmdBuffer` 后）加：

```cpp
	void VulkanRenderer::WriteCapturePng(const void* bgraPixels, uint32_t w, uint32_t h) {
		// swapchain alpha 不参与合成、值不可靠：强制 255，避免 PNG 透明
		std::vector<uint8_t> pixels(size_t(w) * h * 4);
		std::memcpy(pixels.data(), bgraPixels, pixels.size());
		for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 0xFF;

		// VK_FORMAT_B8G8R8A8_UNORM 内存序 = 小端 ARGB8888
		SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
			pixels.data(), (int)w, (int)h, 32, (int)w * 4, SDL_PIXELFORMAT_ARGB8888);
		if (!surf) {
			LOG_ERROR("VulkanRenderer") << "截图 surface 创建失败: " << SDL_GetError();
			return;
		}
		if (IMG_SavePNG(surf, mCapturePath.c_str()) != 0) {
			LOG_ERROR("VulkanRenderer") << "IMG_SavePNG 失败: " << IMG_GetError();
		}
		else {
			LOG_INFO("VulkanRenderer") << "截图已保存: " << mCapturePath;
		}
		SDL_FreeSurface(surf);
	}
```

- [ ] **Step 5: GameAPP.h 加 renderer 访问器**

public 区（`GetGraphics()` 后）加（`pvz::VulkanRenderer` 已有前置声明，`unique_ptr::get()` 不要求完整类型）：

```cpp
	// AutoTest 截图入口
	pvz::VulkanRenderer* GetVulkanRenderer() const { return m_vulkanRenderer.get(); }
```

- [ ] **Step 6: TestDriver.cpp 加 screenshot 命令**

头部加 `#include "../../Renderer/VulkanRenderer.h"`。`ExecuteCurrent` 追加分支：

```cpp
	if (op == "screenshot") {
		const std::string name = cmd.value("name", "shot.png");
		auto* renderer = GameAPP::GetInstance().GetVulkanRenderer();
		if (!renderer) { Fail("screenshot: renderer 为空"); return false; }
		renderer->RequestCapture(mOutDir + "/" + name);
		// 本帧到此为止：截图在本帧 EndFrame 落盘，避免同帧第二个 screenshot 覆盖请求
		mBreakFrame = true;
		return true;
	}
```

- [ ] **Step 7: 构建 + 跑 smoke_screenshot + 读图验证**

msbuild（预期 0 错误），然后：

```powershell
Push-Location x64\Release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_screenshot.json
"exit=$LASTEXITCODE"
Get-ChildItem .\autotest\out\smoke_screenshot\
Pop-Location
```

预期：`exit=0`，目录里有 `menu_into_level.png`（约 1100x600）。**用 Read 工具直接打开该 PNG**：应看到关卡 1 开场画面（草坪/镜头右移中），非全黑非全透明。

- [ ] **Step 8: Commit**

```powershell
git add PlantVsZombies/Renderer/VulkanContext.cpp PlantVsZombies/Renderer/VulkanRenderer.h PlantVsZombies/Renderer/VulkanRenderer.cpp PlantVsZombies/GameAPP.h PlantVsZombies/Game/AutoTest/TestDriver.cpp autotest/scripts/smoke_screenshot.json
git commit -m "feat(autotest): Vulkan swapchain 回读截图 (screenshot 命令)"
```

---

### Task 5: dump_state 状态导出

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Test: 复用 Task 6 的 demo 脚本（本 Task 仅构建验证 + 下一 Task 端到端验证）

- [ ] **Step 1: ExecuteCurrent 追加 dump_state 分支**

依赖的字段全部 public：`Zombie::mBodyHealth/mHelmHealth/mShieldHealth/mRow`（Zombie.h:21-38）、`Plant::mPlantHealth/mRow/mColumn`（Plant.h:24-28）、`AnimatedObject::GetCurrentTrackName()`（AnimatedObject.h:87）、`Zombie::GetPosition()`。

```cpp
	if (op == "dump_state") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("dump_state: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		nlohmann::json out;
		out["boardState"] = BoardStateName(board->mBoardState);
		out["sun"] = board->mSun;
		out["wave"] = board->mCurrentWave;
		out["zombieNumber"] = board->mZombieNumber;

		out["zombies"] = nlohmann::json::array();
		for (int id : board->mEntityManager.GetAllZombieIDs()) {
			Zombie* z = board->mEntityManager.GetZombie(id);
			if (!z) continue;
			const Vector pos = z->GetPosition();
			out["zombies"].push_back({
				{ "id", id },
				{ "type", ZombieTypeName(z->mZombieType) },
				{ "row", z->mRow },
				{ "x", pos.x }, { "y", pos.y },
				{ "bodyHealth", z->mBodyHealth }, { "bodyMaxHealth", z->mBodyMaxHealth },
				{ "helmHealth", z->mHelmHealth }, { "shieldHealth", z->mShieldHealth },
				{ "hasHead", z->HasHead() }, { "hasArm", z->HasArm() },
				{ "slowCooldown", z->GetCooldownTimer() },
				{ "track", z->GetCurrentTrackName() },
			});
		}

		out["plants"] = nlohmann::json::array();
		for (int id : board->mEntityManager.GetAllPlantIDs()) {
			Plant* p = board->mEntityManager.GetPlant(id);
			if (!p) continue;
			out["plants"].push_back({
				{ "id", id },
				{ "type", PlantTypeName(p->mPlantType) },
				{ "row", p->mRow }, { "col", p->mColumn },
				{ "health", p->mPlantHealth }, { "maxHealth", p->mPlantMaxHealth },
				{ "track", p->GetCurrentTrackName() },
			});
		}

		const std::string name = cmd.value("name", "state.json");
		std::ofstream of(mOutDir + "/" + name, std::ios::trunc);
		if (!of) { Fail("dump_state: 无法写 " + name); return false; }
		of << out.dump(2);
		return true;
	}
```

（`Vector` 由已 include 的游戏头间接提供；若编译报未定义，补 `#include "../Definit.h"`。）

- [ ] **Step 2: 构建验证**

msbuild，预期 `0 个错误`。

- [ ] **Step 3: Commit**

```powershell
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m "feat(autotest): dump_state 导出僵尸/植物状态 JSON"
```

---

### Task 6: demo 脚本 + 端到端验收

**Files:**
- Create: `autotest/scripts/demo_peashooter.json`

- [ ] **Step 1: 写 demo 脚本（即 spec 的验收场景）**

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER", "PLANT_SUNFLOWER"], "timeout": 20 },
    { "op": "wait_state", "state": "GAME" },
    { "op": "set_sun", "value": 2000 },
    { "op": "plant", "type": "PLANT_PEASHOOTER", "row": 2, "col": 1 },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 2, "x": 900 },
    { "op": "wait_seconds", "value": 0.5 },
    { "op": "screenshot", "name": "t1_planted.png" },
    { "op": "wait_seconds", "value": 2.5 },
    { "op": "screenshot", "name": "t2_shooting.png" },
    { "op": "wait_seconds", "value": 3.0 },
    { "op": "screenshot", "name": "t3_hit.png" },
    { "op": "dump_state", "name": "state.json" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 运行 + 全套验收**

```powershell
Push-Location x64\Release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
"exit=$LASTEXITCODE"
Get-ChildItem .\autotest\out\demo_peashooter\
Get-Content .\autotest\out\demo_peashooter\state.json
Pop-Location
```

验收清单（全部满足才算过）：
1. `exit=0`；
2. 三张 PNG 存在，用 Read 工具逐张目检：t1 = 豌豆射手已种下 + 僵尸刚出场；t2 = 僵尸前进中、应有豌豆飞行/射击姿态；t3 = 僵尸更靠近、血量被打掉一截；
3. `state.json`：`plants` 数组含 1 个 `PLANT_PEASHOOTER`（row=2, col=1, track 非空）；`zombies` 数组含 1 个 `ZOMBIE_NORMAL`（row=2，`bodyHealth < bodyMaxHealth` 证明被豌豆打过，x 比 900 小证明在前进）；
4. `run.log` 以 `script finished OK` 结尾；
5. 复跑一次带同样 `-Seed 42`，state.json 中僵尸 x 应基本一致（确定性抽查）。

- [ ] **Step 3: Commit**

```powershell
git add autotest/scripts/demo_peashooter.json
git commit -m "feat(autotest): demo 验收脚本（豌豆射手 vs 普通僵尸）"
```

---

### Task 7: CLAUDE.md 更新（构建限制解除 + AutoTest 用法）

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: 改写 Build & Run 一节**

把 CLAUDE.md 中这两块删掉：
1. `> **Important:** Do **not** use the MSVC-Debug-MCP server to build/compile/generate projects...` 引用块整段；
2. Build 工具列表里的 `- **Build (disabled by default):** ... 否则不要调用 ...（含 Async note 时间戳交叉核对注意）` 条目。

替换为（插在 `- **Run:**` 之后）：

```markdown
- **Build (Claude 可自主执行):** 通过 MSBuild 命令行构建，全量约 30 秒，无需人工介入、无需核对产物时间戳：

  ```powershell
  $msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
  & $msbuild .\PlantsVsZombies.sln /p:Configuration=Release /p:Platform=x64 /m /v:m
  ```

  MSVC-Debug-MCP 的 `build_solution` / `build_project` / `clean_solution` 同样可用。
```

MCP 工具描述里 Debug/Operate 两条保留不动。

- [ ] **Step 2: 新增 AutoTest 套件说明一节**

在 "Architecture Overview" 之前插入：

```markdown
## AutoTest 自动化测试套件

`-AutoTest <script.json>` 启动参数让游戏按 JSON 脚本自动执行（进关/选卡/种植/出怪/截图/状态导出后退出），Claude 可独立完成「改代码→构建→跑脚本→Read 截图验证」闭环，无需人工游戏内截图。

- **脚本位置:** `autotest/scripts/*.json`（纯数据，不进 vcxproj；改脚本无需重编译）
- **运行（工作目录必须是 exe 目录）:**
  ```powershell
  Push-Location x64\Release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
  $LASTEXITCODE   # 0=成功；1=命令失败/超时；100=脚本解析失败
  Pop-Location
  ```
- **产物:** `x64\Release\autotest\out\<脚本名>\` 下的 PNG（用 Read 工具直接看）、`state.json`、`run.log`（每条命令的执行轨迹；Release 下 Logger INFO 被裁掉，run.log 是权威记录）
- **命令集:** `goto_level` / `choose_cards` / `wait_state` / `set_sun` / `plant` / `spawn_zombie` / `wait_seconds` / `wait_frames` / `set_timescale` / `screenshot` / `dump_state` / `quit`。等待型命令支持 `timeout`（默认 15s）。植物/僵尸类型用枚举标识符原文（如 `PLANT_PEASHOOTER`、`ZOMBIE_FASTPAPER`），新类型须在 `Game/AutoTest/TestDriver.cpp` 的名表加一行。
- **隔离性:** AutoTest 模式下存档读写全部短路（不读不写 `saves/`），每次进关都是确定性全新关卡；`-Seed N` 固定随机种子。
- **范例:** `autotest/scripts/demo_peashooter.json`（验收脚本），`smoke_*.json`（各子系统冒烟）
```

- [ ] **Step 3: Commit**

```powershell
git add CLAUDE.md
git commit -m "docs: 解除构建限制 + AutoTest 套件用法"
```

---

## Self-Review 记录

- **Spec 覆盖:** 命令集 v1 全部 12 个 op 有任务（Task 2: 5 个基础 + Task 3: 5 个玩法 + Task 4 screenshot + Task 5 dump_state）；截图管线两处改动 = Task 4 Step 2/4；存档隔离 = Task 1 Step 4；`-Seed` = Task 1；demo 验收 = Task 6；CLAUDE.md = Task 7。无遗漏。
- **类型一致性:** `IsChooseCardReady/GetChooseCardUI/GetBoard`（Task 3 Step 2 定义、Step 4 使用）、`RequestCapture/WriteCapturePng`（Task 4 Step 3 声明、Step 4/6 使用）、`kPlantNames/kZombieNames/kBoardStateNames`（Task 3 定义、Task 5 复用）签名一致。
- **已知风险点（执行时注意）:**
  - `Board::CreatePlant` 是否校验格子占用未核实——demo 只种一棵，不触发；若返回空即 Fail，行为可接受。
  - `VK_CHECK` 宏在截图路径失败时的提前 return 会让 captureBuf 正常析构（RAII），无泄漏。
  - `GameInfoSaver.cpp` 是否已 include `GameApp.h` 执行时确认，没有就补。
  - Logger 在 Release 裁到 WARN+ERROR：所以 TestDriver 一切关键輸出都走 run.log（已设计）。
