# 开发者模式（-develop）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 命令行 `-develop` 启用开发者模式：按 D 呼出面板，提供无冷却种植/无视阳光种植开关、任意跳关、点草坪召唤指定僵尸、立即下一波。

**Architecture:** 三个 `GameAPP` inline static bool 承载模式与作弊开关；作弊在收费点（`CardComponent` 冷却、`CardSlotManager` 阳光）加双条件守卫；面板照 `BeginSurvivalPerkSelect` 的 `GameMessageBox` 纯色面板模式，状态变化即整体重建（autoClose=true 帧末安全销毁）；跳关走 `SetGlobalData("EnterLevel")+SwitchTo("GameScene")` 且延迟到 Update 尾部执行（同 `mReadyToBackMenu` 模式）；下一波 = `mZombieCountDown = 0`。

**Tech Stack:** C++17 / 既有 UI（GameMessageBox, InputHandler）/ AutoTest JSON 验证。

## Global Constraints

- 无 `-develop` 时零行为变化：所有作弊守卫写成 `GameAPP::mDevelopMode && GameAPP::mDevXxx` 双条件。
- 作弊开关不进存档。
- 中文 UTF-8 字符串、`/utf-8` 编码；新 .h 首行 `#pragma once`（pre-commit 钩子强制）。
- 构建验证一律用 `cmake --build --preset clang-release`（警告零基线在 clang）。
- 面板/放置模式仅 GameScene 生效；与暂停菜单、词条选择、词条查看互斥。
- UI 布局坐标以本计划为准（主人已授权布局全权裁量），smoke 脚本点击坐标依赖这些数值，改布局须同步改脚本。

## 面板布局基准（逻辑坐标，SCENE 1100x600，盒中心 550,300，尺寸 520x400）

| 元素 | pos (左上) | size | 点击中心 |
|---|---|---|---|
| 标题「开发者面板」 | 居中 y=120 | font 22 | — |
| 【无冷却种植：开/关】 | (340,160) | 200x36 | (440,178) |
| 【无视阳光：开/关】 | (340,206) | 200x36 | (440,224) |
| 僵尸【◀】 | (340,252) | 40x36 | (360,270) |
| 僵尸类型名文字 | (395,260) | font 14 | — |
| 僵尸【▶】 | (560,252) | 40x36 | (580,270) |
| 【召唤】 | (620,252) | 90x36 | (665,270) |
| 关卡【-】 | (340,302) | 40x36 | (360,320) |
| 「关卡 N」文字 | (420,310) | font 16 | — |
| 关卡【+】 | (560,302) | 40x36 | (580,320) |
| 【进入】 | (620,302) | 90x36 | (665,320) |
| 【无尽1000】 | (340,348) | 110x32 | (395,364) |
| 【夜无尽1001】 | (460,348) | 130x32 | (525,364) |
| 【下一波】 | (360,420) | 120x40 | (420,440) |
| 【关闭】 | (600,420) | 120x40 | (660,440) |

---

### Task 1: `-develop` 启动参数与 GameAPP 状态

**Files:**
- Modify: `PlantVsZombies/GameApp.h:88-91`（statics 区）
- Modify: `PlantVsZombies/main.cpp:34-38`（参数循环）

**Interfaces:**
- Produces: `GameAPP::mDevelopMode`、`GameAPP::mDevNoCooldown`、`GameAPP::mDevFreePlant`（inline static bool，后续所有任务引用）。

- [ ] **Step 1: GameApp.h 加三个静态开关**

在 `inline static bool mAutoTestMode = false;`（GameApp.h:91）后追加：

```cpp
	inline static bool mDevelopMode = false;      // -develop 开发者模式（D 键面板）
	inline static bool mDevNoCooldown = false;    // 开发者作弊：无冷却种植（面板内切换）
	inline static bool mDevFreePlant = false;     // 开发者作弊：无视阳光种植（面板内切换）
```

- [ ] **Step 2: main.cpp 参数分支**

在 `-NoInstance` 分支（main.cpp:34-38）后追加：

```cpp
		else if (arg == "-Develop" || arg == "-develop")
		{
			GameAPP::mDevelopMode = true;
			LOG_DEBUG("Main") << "开发者模式已启用 (-develop)";
		}
```

- [ ] **Step 3: 构建验证**

Run: `cmake --build --preset clang-release`（在已导入 VS 环境的 shell 中）
Expected: 构建成功，零新警告。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/GameApp.h PlantVsZombies/main.cpp
git commit -m "feat(dev): -develop 启动参数与开发者作弊开关状态"
```

---

### Task 2: 作弊生效点——无冷却 / 无视阳光

**Files:**
- Modify: `PlantVsZombies/Game/CardComponent.cpp`（Update:86-105、StartCooldown:138-148）
- Modify: `PlantVsZombies/Game/CardSlotManager.h:47`（CanAfford 内联改声明）
- Modify: `PlantVsZombies/Game/CardSlotManager.cpp`（CanAfford 定义 + SpendSun:158-170）

**Interfaces:**
- Consumes: Task 1 的三个 GameAPP statics。
- Produces: `bool CardSlotManager::CanAfford(int cost) const`（签名不变，移到 .cpp）。行为契约：开发者+无视阳光时 CanAfford 恒 true、SpendSun 不扣阳光；开发者+无冷却时卡片永不进入冷却、已在冷却立即清零。

- [ ] **Step 1: CardComponent.cpp 冷却守卫**

确认文件顶部已 `#include`（直接或间接）`GameApp.h`，缺则补 `#include "../GameApp.h"`。

`Update()`（原 86 行起）冷却分支开头插入：

```cpp
	if (mIsCooldown) {
		// 开发者作弊：无冷却——已在冷却中的卡立即清零（双条件守卫，无 -develop 恒不生效）
		if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) {
			mIsCooldown = false;
			mCooldownTimer = 0;
			ForceStateUpdate();
			return;
		}
		mCooldownTimer -= DeltaTime::GetDeltaTime();
		...（原逻辑不动）
```

`StartCooldown()`（原 138 行）开头插入：

```cpp
	if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) return;   // 开发者作弊：不进入冷却
```

- [ ] **Step 2: CardSlotManager CanAfford 移 .cpp 并加守卫**

CardSlotManager.h:47 改为声明：

```cpp
	bool CanAfford(int cost) const;
```

CardSlotManager.cpp（SpendSun 前）加定义（确认 `#include "../GameApp.h"`，缺则补）：

```cpp
bool CardSlotManager::CanAfford(int cost) const {
	if (GameAPP::mDevelopMode && GameAPP::mDevFreePlant) return true;   // 开发者作弊：无视阳光
	return mBoard ? mBoard->GetSun() >= cost : false;
}
```

- [ ] **Step 3: SpendSun 不扣阳光**

`CardSlotManager::SpendSun`（.cpp:158）在 `if (CanAfford(cost))` 前插入：

```cpp
	if (GameAPP::mDevelopMode && GameAPP::mDevFreePlant) {
		UpdateAllCardsState();
		return true;                                   // 开发者作弊：视为支付成功但不扣阳光
	}
```

- [ ] **Step 4: 构建验证**

Run: `cmake --build --preset clang-release`
Expected: 成功。（功能验证在 Task 6 smoke 脚本，此时开关尚无 UI 入口。）

- [ ] **Step 5: Commit**

```bash
git add PlantVsZombies/Game/CardComponent.cpp PlantVsZombies/Game/CardSlotManager.h PlantVsZombies/Game/CardSlotManager.cpp
git commit -m "feat(dev): 无冷却/无视阳光作弊在卡片冷却与阳光收费点生效"
```

---

### Task 3: D 键 + 开发者面板（作弊开关切换、僵尸/关卡选择器 UI）

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.h`（成员与方法声明）
- Modify: `PlantVsZombies/Game/GameScene.cpp`（Update 的 D 键处理 + 面板实现）

**Interfaces:**
- Consumes: GameAPP statics（Task 1）；`mUIManager.CreateMessageBox(...)`、`GameMessageBox::ButtonConfig{text,pos,size,fontsize,callback,texture,autoClose}`、`TextConfig{pos,size,text,color,font}`。
- Produces（Task 4/5 依赖）：
  - 成员：`bool mDevPanelActive`、`bool mDevSpawnMode`、`int mDevZombieIndex`、`int mDevLevelSel`、`int mDevPendingLevel`、`std::weak_ptr<GameMessageBox> mDevPanelBox`、`bool mDevHintRegistered`
  - 方法：`void OpenDevPanel()`、`void CloseDevPanel()`、`void RenderDevPanel()`
  - GameScene.cpp 文件级静态表 `kDevZombieTable`（`std::vector<std::pair<ZombieType, const char*>>`）。

- [ ] **Step 1: GameScene.h 声明**

private 成员区（mPerkViewPage 之后）追加：

```cpp
	// ---- 开发者模式（-develop，D 键面板）----
	std::weak_ptr<GameMessageBox> mDevPanelBox;
	bool mDevPanelActive   = false;   // 面板打开中（守卫暂停叠态）
	bool mDevSpawnMode     = false;   // 召唤放置模式（Task 4）
	bool mDevHintRegistered = false;  // 放置模式提示绘制命令已注册
	int  mDevZombieIndex   = 0;       // kDevZombieTable 下标
	int  mDevLevelSel      = 1;       // 面板选中的关卡号
	int  mDevPendingLevel  = -1;      // >=0 时 Update 尾部执行跳关（Task 5）
```

private 方法区追加：

```cpp
	void OpenDevPanel();
	void CloseDevPanel();
	void RenderDevPanel();
```

- [ ] **Step 2: GameScene.cpp 僵尸类型表**

文件级匿名命名空间（或文件顶部 static）：

```cpp
namespace {
#define DEVZ(n) { ZombieType::n, #n }
	// 面板循环切换表；显示枚举标识符（简陋版足够）。与 TestDriver kZombieNames 同集合。
	const std::vector<std::pair<ZombieType, const char*>> kDevZombieTable = {
		DEVZ(ZOMBIE_NORMAL), DEVZ(ZOMBIE_TRAFFIC_CONE), DEVZ(ZOMBIE_POLEVAULTER), DEVZ(ZOMBIE_BUCKET),
		DEVZ(ZOMBIE_FASTBUCKET), DEVZ(ZOMBIE_NEWSPAPER), DEVZ(ZOMBIE_FASTPAPER), DEVZ(ZOMBIE_DOOR),
		DEVZ(ZOMBIE_FOOTBALL), DEVZ(ZOMBIE_DANCER), DEVZ(ZOMBIE_BACKUP_DANCER), DEVZ(ZOMBIE_DUCKY_TUBE),
		DEVZ(ZOMBIE_SNORKEL), DEVZ(ZOMBIE_ZAMBONI), DEVZ(ZOMBIE_BOBSLED), DEVZ(ZOMBIE_DOLPHIN_RIDER),
		DEVZ(ZOMBIE_JACK_IN_THE_BOX), DEVZ(ZOMBIE_BALLOON), DEVZ(ZOMBIE_DIGGER), DEVZ(ZOMBIE_POGO),
		DEVZ(ZOMBIE_YETI), DEVZ(ZOMBIE_BUNGEE), DEVZ(ZOMBIE_LADDER), DEVZ(ZOMBIE_CATAPULT),
		DEVZ(ZOMBIE_GARGANTUAR), DEVZ(ZOMBIE_IMP), DEVZ(ZOMBIE_BOSS), DEVZ(ZOMBIE_PEA_HEAD),
		DEVZ(ZOMBIE_WALLNUT_HEAD), DEVZ(ZOMBIE_JALAPENO_HEAD), DEVZ(ZOMBIE_GATLING_HEAD),
		DEVZ(ZOMBIE_SQUASH_HEAD), DEVZ(ZOMBIE_TALLNUT_HEAD), DEVZ(ZOMBIE_REDEYE_GARGANTUAR),
	};
#undef DEVZ
}
```

（`ZombieType.h` 已被 GameScene.cpp 传递包含；若编译报未定义则补 `#include "Zombie/ZombieType.h"`。）

- [ ] **Step 3: Open/Close/Render 实现**

```cpp
void GameScene::OpenDevPanel()
{
	if (!GameAPP::mDevelopMode || !mBoard) return;
	if (DeltaTime::IsPaused()) return;
	// 多向守卫：暂停菜单 / 词条选择 / 词条查看 / 自身已开，均不叠开
	if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive || mDevPanelActive) return;

	mDevPanelActive = true;
	DeltaTime::SetPaused(true);
	RenderDevPanel();
}

void GameScene::CloseDevPanel()
{
	if (auto box = mDevPanelBox.lock())
		GameObjectManager::GetInstance().DestroyGameObject(box);
	mDevPanelBox.reset();
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
}

void GameScene::RenderDevPanel()
{
	// 状态变化即整体重建：旧盒由被点按钮 autoClose=true 帧末自毁，这里只管建新盒
	const float cx = static_cast<float>(SCENE_WIDTH) / 2.0f;    // 550
	const float cy = static_cast<float>(SCENE_HEIGHT) / 2.0f;   // 300
	const Vector boxSize(520.0f, 400.0f);
	const glm::vec4 titleColor{ 245, 214, 127, 255 };
	const glm::vec4 textColor { 230, 230, 230, 255 };

	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig>   texts;

	texts.push_back({ Vector(cx - 70.0f, 110.0f), 22.0f, u8"开发者面板", titleColor });

	auto toggleText = [](const char* name, bool on) {
		return std::string(name) + (on ? u8"：开" : u8"：关");
	};

	// 作弊开关（点击翻转后重建面板刷新文字）
	buttons.push_back({ toggleText(u8"无冷却种植", GameAPP::mDevNoCooldown),
		Vector(340.0f, 160.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevNoCooldown = !GameAPP::mDevNoCooldown; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ toggleText(u8"无视阳光", GameAPP::mDevFreePlant),
		Vector(340.0f, 206.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevFreePlant = !GameAPP::mDevFreePlant; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	// 僵尸类型选择行
	const int zn = static_cast<int>(kDevZombieTable.size());
	buttons.push_back({ u8"◀", Vector(340.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + zn - 1) % zn; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	texts.push_back({ Vector(395.0f, 260.0f), 14.0f,
		kDevZombieTable[mDevZombieIndex].second, textColor });
	buttons.push_back({ u8"▶", Vector(560.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + 1) % zn; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"召唤", Vector(620.0f, 252.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->BeginDevSpawnMode(); },        // Task 4 实现；本任务先占位为空函数
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	// 关卡选择行
	buttons.push_back({ u8"-", Vector(340.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { if (mDevLevelSel > 1) --mDevLevelSel; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	texts.push_back({ Vector(420.0f, 310.0f), 16.0f,
		std::string(u8"关卡 ") + std::to_string(mDevLevelSel), textColor });
	buttons.push_back({ u8"+", Vector(560.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { ++mDevLevelSel; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"进入", Vector(620.0f, 302.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->DevJumpToLevel(); },           // Task 5 实现；本任务先占位为空函数
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"无尽1000", Vector(340.0f, 348.0f), Vector(110.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1000; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"夜无尽1001", Vector(460.0f, 348.0f), Vector(130.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1001; RenderDevPanel(); },
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	// 底部：下一波 / 关闭
	buttons.push_back({ u8"下一波", Vector(360.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { this->DevTriggerNextWave(); },       // Task 5 实现；本任务先占位为空函数
		ResourceKeys::Textures::IMAGE_BUTTONBIG, true });
	buttons.push_back({ u8"关闭", Vector(600.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { mDevPanelActive = false; DeltaTime::SetPaused(false); mDevPanelBox.reset(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

	mDevPanelBox = mUIManager.CreateMessageBox(
		Vector(cx, cy), "", buttons, sliders, texts, "", 1.0f, "", boxSize);
}
```

同时在 GameScene.h 声明三个占位方法（Task 4/5 填实现，本任务给空体防链接错误）：

```cpp
	void BeginDevSpawnMode();     // Task 4
	void DevJumpToLevel();        // Task 5
	void DevTriggerNextWave();    // Task 5
```

GameScene.cpp 本任务给空实现：

```cpp
void GameScene::BeginDevSpawnMode() {}
void GameScene::DevJumpToLevel() {}
void GameScene::DevTriggerNextWave() {}
```

- [ ] **Step 4: Update 里接 D 键**

在 GameScene.cpp:424 `auto& input = ...GetInputHandler();` 之后、SPACE/ESC 菜单块之前插入：

```cpp
	// 开发者模式：D 键呼出/关闭面板（放置模式时按 D 回面板——Task 4 生效）
	if (GameAPP::mDevelopMode && input.IsKeyPressed(SDLK_d)
		&& !mOpenMenu && !mSurvivalPerkSelectActive && !mPerkViewActive) {
		if (mDevSpawnMode)          { mDevSpawnMode = false; OpenDevPanel(); }
		else if (mDevPanelActive)   { CloseDevPanel(); }
		else                        { OpenDevPanel(); }
	}
```

注意：面板打开时 `DeltaTime::IsPaused()==true`，而 OpenDevPanel 有 IsPaused 守卫——关闭走 CloseDevPanel 分支不受影响；从放置模式回面板时游戏未暂停，OpenDevPanel 正常通过。

- [ ] **Step 5: 构建 + AutoTest 冒烟（面板可见性）**

写临时脚本 `autotest/scripts/tmp_devpanel.json`：

```json
{ "commands": [
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "wait_frames", "value": 40 },
  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "screenshot", "name": "devpanel" },
  { "op": "quit" }
] }
```

Run:
```powershell
cmake --build --preset clang-release
Push-Location build\clang-release
.\PlantsVsZombies.exe -develop -AutoTest ..\..\autotest\scripts\tmp_devpanel.json -Seed 42
Pop-Location
```
Expected: 退出码 0；Read `build\clang-release\autotest\out\tmp_devpanel\devpanel.png` 可见居中面板与全部按钮文字。验证后删除临时脚本。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/Game/GameScene.h PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(dev): D 键开发者面板——作弊开关/僵尸与关卡选择器 UI 骨架"
```

---

### Task 4: 召唤放置模式（点草坪在 (row,x) 生成僵尸）

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`（BeginDevSpawnMode 实现 + Update 放置模式处理 + ESC 菜单条件修改）

**Interfaces:**
- Consumes: `Board::CreateZombie(ZombieType, int row, float x)`（Board.h:182，后两参默认）、`Board::GetZombieSpawnY(int row)`、`Board::mRows`（public）、`InputHandler::GetMouseWorldPosition()`、`CardSlotManager::DeselectCard()`、`kDevZombieTable`/`mDevZombieIndex`（Task 3）。
- Produces: `mDevSpawnMode` 生命周期完整（进入/点击生成/ESC 或 D 退出）。

- [ ] **Step 1: BeginDevSpawnMode 实现（替换 Task 3 空体）**

```cpp
void GameScene::BeginDevSpawnMode()
{
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
	mDevPanelBox.reset();          // 盒子由按钮 autoClose 自毁
	mDevSpawnMode = true;
	if (mCardSlotManager) mCardSlotManager->DeselectCard();   // 防手持卡与召唤点击叠加种植

	// 顶部提示（一次注册，靠 mDevSpawnMode 守卫显隐）
	if (!mDevHintRegistered) {
		RegisterDrawCommand("DevSpawnHint",
			[this](Graphics* g) {
				if (!mDevSpawnMode) return;
				const std::string tip = std::string(u8"召唤模式：")
					+ kDevZombieTable[mDevZombieIndex].second + u8"（左键放置，ESC 退出，D 回面板）";
				GameAPP::GetInstance().DrawText(tip,
					g->LogicalToWorld(300, 30), { 255, 90, 90, 255 },
					ResourceKeys::Fonts::FONT_FZCQ, 18);
			},
			LAYER_UI + 100000);
		SortDrawCommands();
		mDevHintRegistered = true;
	}
}
```

- [ ] **Step 2: Update 放置模式处理（插在 Task 3 的 D 键块之后、菜单键块之前）**

```cpp
	// 开发者：召唤放置模式——独占 ESC 与左键（须在暂停菜单键处理之前）
	bool devConsumedEsc = false;
	if (mDevSpawnMode && mBoard && !DeltaTime::IsPaused()) {
		if (input.IsKeyPressed(SDLK_ESCAPE)) {
			mDevSpawnMode = false;
			devConsumedEsc = true;
		}
		else if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
			const Vector mp = input.GetMouseWorldPosition();
			int bestRow = 0;
			float bestDist = 1e9f;
			for (int r = 0; r < mBoard->mRows; ++r) {
				const float d = std::abs(mp.y - mBoard->GetZombieSpawnY(r));
				if (d < bestDist) { bestDist = d; bestRow = r; }
			}
			mBoard->CreateZombie(kDevZombieTable[mDevZombieIndex].first, bestRow, mp.x);
			LOG_DEBUG("DevMode") << "召唤 " << kDevZombieTable[mDevZombieIndex].second
				<< " row=" << bestRow << " x=" << mp.x;
		}
	}
```

- [ ] **Step 3: 菜单 ESC 条件加守卫**

GameScene.cpp:425 的条件（`if (mBoard->mBoardState != BoardState::LOSE_GAME && !this->mOpenRestartMenu && (input.IsKeyPressed(SDLK_SPACE) || input.IsKeyPressed(SDLK_ESCAPE)))`）追加 `&& !devConsumedEsc`，防止退出放置模式的同一帧 ESC 又弹出暂停菜单。

- [ ] **Step 4: 构建 + AutoTest 验证（生成位置正确）**

临时脚本 `autotest/scripts/tmp_devspawn.json`：

```json
{ "commands": [
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "wait_frames", "value": 40 },
  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 665, "y": 270 },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 600, "y": 340 },
  { "op": "wait_frames", "value": 10 },
  { "op": "dump_state", "name": "after_spawn" },
  { "op": "screenshot", "name": "spawned" },
  { "op": "quit" }
] }
```

Run: 同 Task 3 的运行方式（`-develop -AutoTest ... -Seed 42`）。
Expected: 退出码 0；`after_spawn.json` 中 zombies 数组含 1 个 ZOMBIE_NORMAL，row≈2（y=340 最近行）、x≈点击世界坐标；截图确认僵尸站位与提示文字。若 x 有固定偏移（逻辑/世界坐标之差），把 `GetMouseWorldPosition` 换 `GetMousePosition` 对照后取正确者。验证后删临时脚本。

- [ ] **Step 5: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(dev): 召唤放置模式——选类型后点草坪按 (row,x) 生成僵尸"
```

---

### Task 5: 任意跳关 + 下一波

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`（DevJumpToLevel / DevTriggerNextWave 实现 + Update 尾部 pending 跳关）

**Interfaces:**
- Consumes: `SceneManager::SetGlobalData("EnterLevel", ...)` + `SwitchTo("GameScene")`（同 TestDriver goto_level:222-228 与 GameSelectScene:142-151 的既有跳关契约）；`Board::mZombieCountDown`、`Board::mBoardState`、`Board::mCurrentWave/mMaxWave`。
- Produces: 面板【进入】【无尽1000】【夜无尽1001】【下一波】按钮全部生效。

- [ ] **Step 1: 两个方法实现（替换 Task 3 空体）**

```cpp
void GameScene::DevJumpToLevel()
{
	// 不能在按钮回调（本帧 Update 中段）直接 SwitchTo 销毁自身——
	// 与 mReadyToBackMenu 同理，置 pending 由 Update 尾部统一执行
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
	mDevPanelBox.reset();
	mDevPendingLevel = mDevLevelSel;
}

void GameScene::DevTriggerNextWave()
{
	if (mBoard && mBoard->mBoardState == BoardState::GAME
		&& mBoard->mCurrentWave < mBoard->mMaxWave) {
		mBoard->mZombieCountDown = 0.0f;   // 出波倒计时清零，Board::Update 下帧即发下一波
	}
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
	mDevPanelBox.reset();
}
```

- [ ] **Step 2: Update 尾部执行 pending 跳关**

在 GameScene.cpp:652 `if (mReadyToBackMenu) {...}` 块之前插入（同款延迟切场景模式）：

```cpp
	if (mDevPendingLevel >= 0) {
		const int lv = mDevPendingLevel;
		mDevPendingLevel = -1;
		GameAPP::GetInstance().GetGraphics().SetCameraPosition(0, 0);
		auto& sm = SceneManager::GetInstance();
		sm.SetGlobalData("EnterLevel", std::to_string(lv));
		sm.SwitchTo("GameScene");   // 重建 GameScene，不检查存档解锁
		return;
	}
```

- [ ] **Step 3: 构建 + AutoTest 验证**

临时脚本 `autotest/scripts/tmp_devjump.json`：

```json
{ "commands": [
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "wait_frames", "value": 40 },
  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 420, "y": 440 },
  { "op": "wait_seconds", "value": 2 },
  { "op": "dump_state", "name": "after_wave" },
  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 525, "y": 364 },
  { "op": "wait_frames", "value": 5 },
  { "op": "click", "x": 665, "y": 320 },
  { "op": "wait_frames", "value": 60 },
  { "op": "dump_state", "name": "after_jump" },
  { "op": "screenshot", "name": "night_endless" },
  { "op": "quit" }
] }
```

Expected: 退出码 0；`after_wave.json` 的 `wave >= 1`（下一波已触发）；`after_jump.json` 的关卡字段为 1001（dump_state 若无 level 字段，看截图为黑夜背景+选卡界面即可）。验证后删临时脚本。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(dev): 面板任意跳关(含1000/1001)与下一波立即触发"
```

---

### Task 6: dump_state 暴露 dev 开关 + 正式 smoke 脚本 + 收尾

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（dump_state 处，~362 行 `out["wave"]` 附近）
- Create: `autotest/scripts/smoke_develop.json`

**Interfaces:**
- Consumes: 前五个任务全部产物。
- Produces: `state.json` 新增 `devNoCooldown` / `devFreePlant` 字段；一条覆盖全功能的 smoke 脚本。

- [ ] **Step 1: dump_state 加两字段**

在 `out["wave"] = board->mCurrentWave;`（TestDriver.cpp:362）后追加：

```cpp
		out["devNoCooldown"] = GameAPP::mDevNoCooldown;
		out["devFreePlant"]  = GameAPP::mDevFreePlant;
```

- [ ] **Step 2: 正式 smoke 脚本**

Create `autotest/scripts/smoke_develop.json`（合并前面临时脚本，全功能一遍）：

```json
{ "commands": [
  { "op": "goto_level", "level": 1 },
  { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
  { "op": "wait_state", "state": "GAME" },
  { "op": "wait_frames", "value": 40 },

  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "screenshot", "name": "panel" },
  { "op": "click", "x": 440, "y": 178 },
  { "op": "wait_frames", "value": 5 },
  { "op": "click", "x": 440, "y": 224 },
  { "op": "wait_frames", "value": 5 },
  { "op": "dump_state", "name": "toggles_on" },

  { "op": "click", "x": 580, "y": 270 },
  { "op": "wait_frames", "value": 5 },
  { "op": "click", "x": 665, "y": 270 },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 600, "y": 340 },
  { "op": "wait_frames", "value": 10 },
  { "op": "dump_state", "name": "spawned" },
  { "op": "key", "name": "escape" },
  { "op": "wait_frames", "value": 5 },

  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 420, "y": 440 },
  { "op": "wait_seconds", "value": 2 },
  { "op": "dump_state", "name": "next_wave" },

  { "op": "key", "name": "d" },
  { "op": "wait_frames", "value": 10 },
  { "op": "click", "x": 525, "y": 364 },
  { "op": "wait_frames", "value": 5 },
  { "op": "click", "x": 665, "y": 320 },
  { "op": "wait_frames", "value": 60 },
  { "op": "screenshot", "name": "jumped_1001" },
  { "op": "dump_state", "name": "jumped" },
  { "op": "quit" }
] }
```

- [ ] **Step 3: 运行并逐项核对**

```powershell
cmake --build --preset clang-release
Push-Location build\clang-release
.\PlantsVsZombies.exe -develop -AutoTest ..\..\autotest\scripts\smoke_develop.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected（Read `build\clang-release\autotest\out\smoke_develop\` 下产物）：
1. 退出码 0
2. `toggles_on.json`：`devNoCooldown=true`、`devFreePlant=true`
3. `spawned.json`：zombies 含 1 个 ZOMBIE_TRAFFIC_CONE（▶ 切过一次），row≈2
4. `next_wave.json`：`wave >= 1`
5. `jumped_1001.json` 截图为黑夜背景选卡界面
6. 无 `-develop` 回归：`.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42` 退出码 0（D 键无效果、行为不变）

- [ ] **Step 4: 收尾 Commit**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp autotest/scripts/smoke_develop.json
git commit -m "feat(dev): dump_state 暴露 dev 开关 + smoke_develop 全功能冒烟脚本"
```
