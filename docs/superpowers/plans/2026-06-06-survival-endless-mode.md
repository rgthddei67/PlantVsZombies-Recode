# 生存模式（无尽）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 `Board`/`GameScene` 上实现可无限循环的"生存模式：无尽"，每轮固定波数、轮间回选卡（植物/阳光保留）、僵尸逐轮变强，并正确处理轮间 `CHOOSE_CARD` 状态的存读档。

**Architecture:** 采用方案 A——`Board` 加 `mIsSurvival` 标志（由特殊 level 号 `1000` 触发）+ `mSurvivalRound` 轮次字段，复用现有 Intro 状态机、Card 选卡流程、存档系统，按标志走分支。轮间转场分两条路径：同会话轮间走轻量路径（只重选卡、铲子保留）；存档重进走完整正常流程（重播过场 + `StartGame`）。

**Tech Stack:** C++17 / Visual Studio 2026 / SDL2 / Vulkan / nlohmann::json。**无自动化测试框架**：每个任务的"验证"= 主人在 VS 按 F7 编译（0 错误）+ 指定的手动运行检查。**构建由主人执行**，本计划执行者不得调用 MSVC build 工具。提交为主人手动确认（本仓库 commits user-driven）。

> 设计依据：`docs/superpowers/specs/2026-06-06-survival-endless-mode-design.md`

---

## 文件结构

| 文件 | 职责 | 改动 |
|---|---|---|
| `PlantVsZombies/Game/Board.h` | 关卡管理器声明 | 新增 survival 常量、字段、方法声明 |
| `PlantVsZombies/Game/Board.cpp` | 关卡逻辑 | 构造分支、出怪表、难度、`OnSurvivalRoundClear`、`PickZombieType` 旁路 |
| `PlantVsZombies/Game/Zombie/Zombie.cpp` | 僵尸逻辑 | `CheckWin` 增加 survival 分支 |
| `PlantVsZombies/Game/GameScene.h` | 游戏场景声明 | `BeginSurvivalCardSelect`、轮间标志、UI 注册标志 |
| `PlantVsZombies/Game/GameScene.cpp` | 场景/过场/选卡流程 | 轮间选卡子流程、`ChooseCardComplete` 分支、`OnExit` 存档门槛 |
| `PlantVsZombies/Game/CardSlotManager.h/.cpp` | 卡槽管理 | 新增 `ClearAllCards()` 供轮间空槽重选 |
| `PlantVsZombies/GameInfoSaver.cpp` | 存读档 | `SaveLevelData` 门槛放宽 + survival 字段；`LoadLevelData` 恢复字段 |
| `PlantVsZombies/Game/MainMenuScene.cpp` | 主菜单（仅测试入口接线） | survival 按钮回调进入生存模式（主人可改/替换 UI） |

---

## Task 1: Board 生存模式常量与字段

**Files:**
- Modify: `PlantVsZombies/Game/Board.h`

- [ ] **Step 1: 在 Board.h 顶部常量区新增生存模式常量**

在 `Board.h` 现有 `constexpr int MAX_ZOMBIES_PER_WAVE = 120;`（第 48 行附近）之后新增：

```cpp
// ===== 生存模式（无尽）=====
constexpr int   SURVIVAL_ENDLESS_LEVEL = 1000;   // 生存模式专用 level 号（> 50，避开冒险关推进逻辑）
constexpr int   SURVIVAL_WAVES_PER_ROUND = 10;   // 每轮（每面旗）波数，10 与"第10波=一大波"逻辑对齐
constexpr float SURVIVAL_BUDGET_GROWTH = 0.35f;  // 每轮单波点数预算增长系数（可调）
```

- [ ] **Step 2: 在 Board 类 public 字段区新增生存模式状态字段**

在 `Board.h` 的 `bool mTrophySpawned = false;`（第 83 行附近）之后新增：

```cpp
	bool mIsSurvival = false;     // 是否为生存模式（无尽）
	int  mSurvivalRound = 1;      // 当前第几面旗（轮次，从 1 起）
```

- [ ] **Step 3: 在 Board 类声明区新增方法声明**

在 `Board.h` 的 `void GameOver();`（第 206 行附近）之后新增：

```cpp
	// 生存模式：一轮（一面旗）清空后推进到下一轮并回到选卡
	void OnSurvivalRoundClear();

	// 生存模式：根据轮次重建出怪表
	void BuildSurvivalSpawnList(int round);

	// 生存模式：根据当前轮次刷新关卡名（"生存模式：无尽 第N面旗"）
	void UpdateSurvivalLevelName();
```

- [ ] **Step 4: 验证编译**

请主人在 VS 按 F7 编译。
预期：0 错误（仅新增声明，未使用，应通过；若报"未定义"是因方法体在 Task 2/3 实现，本步只加声明不应引用，故应通过）。

- [ ] **Step 5: 提交（主人确认后）**

```
feat(survival): 新增生存模式常量与 Board 状态字段

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 2: Board 构造分支、出怪表与逐轮难度

**Files:**
- Modify: `PlantVsZombies/Game/Board.cpp`

- [ ] **Step 1: 构造函数增加 survival 分支**

`Board.cpp` 构造函数当前为（第 21~43 行）：

```cpp
Board::Board(GameScene* gameScene, Background background, int level)
{
	mGameScene = gameScene;
	mLevel = level;
	mBackGround = background;
	if (mLevel >= 1)
	{
		mLevelName.clear();
		int mBigLevel = (mLevel - 1) / 9 + 1;
		int mSmallLevel = (mLevel - 1) % 9 + 1;
		mLevelName = u8"关卡 " + std::to_string(mBigLevel) + u8"-" + std::to_string(mSmallLevel);
	}
	mSpawnZombieList.reserve(32);
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);
	mPreviewZombieList.reserve(32);
	if (mLevel > 0)
	{
		LoadSpawnListFromJson();
	}
	CreatePreviewZombies();
	InitializeCell();
	InitializeRows();
}
```

替换为（加入 `mIsSurvival` 判定与 survival 专用初始化）：

```cpp
Board::Board(GameScene* gameScene, Background background, int level)
{
	mGameScene = gameScene;
	mLevel = level;
	mBackGround = background;
	mIsSurvival = (level == SURVIVAL_ENDLESS_LEVEL);

	if (mLevel >= 1)
	{
		mLevelName.clear();
		int mBigLevel = (mLevel - 1) / 9 + 1;
		int mSmallLevel = (mLevel - 1) % 9 + 1;
		mLevelName = u8"关卡 " + std::to_string(mBigLevel) + u8"-" + std::to_string(mSmallLevel);
	}
	mSpawnZombieList.reserve(32);
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);
	mPreviewZombieList.reserve(32);

	if (mIsSurvival)
	{
		mSurvivalRound = 1;
		mMaxWave = SURVIVAL_WAVES_PER_ROUND;
		BuildSurvivalSpawnList(mSurvivalRound);
		UpdateSurvivalLevelName();
	}
	else if (mLevel > 0)
	{
		LoadSpawnListFromJson();
	}

	CreatePreviewZombies();
	InitializeCell();
	InitializeRows();
}
```

- [ ] **Step 2: 实现 BuildSurvivalSpawnList / UpdateSurvivalLevelName**

在 `Board.cpp` 的 `LoadSpawnListFromJson()` 函数（第 576 行附近）之前或之后新增两个函数定义（注意只用本作已实现的僵尸类型 0~5）：

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
}

void Board::UpdateSurvivalLevelName()
{
	mLevelName = u8"生存模式：无尽 第" + std::to_string(mSurvivalRound) + u8"面旗";
}
```

- [ ] **Step 3: 难度逐轮递增——修改 CalculateWaveZombiePoints**

`Board.cpp` 的 `CalculateWaveZombiePoints()`（第 498 行起）当前为：

```cpp
inline int Board::CalculateWaveZombiePoints() const
{
	// 基础点数
	float points = (static_cast<float>(mCurrentWave) / 3 + 1.0f) * 1000.0f;

	points *= (GameAPP::GetInstance().Difficulty * 0.5f);
```

在 `points *= (GameAPP::GetInstance().Difficulty * 0.5f);` 之后、`isFlagWave` 判断之前，插入 survival 增长系数：

```cpp
	points *= (GameAPP::GetInstance().Difficulty * 0.5f);

	// 生存模式：单波点数预算随轮次递增（每轮 mCurrentWave 会重置，故由轮次系数补偿）
	if (mIsSurvival)
	{
		points *= (1.0f + SURVIVAL_BUDGET_GROWTH * static_cast<float>(mSurvivalRound - 1));
	}
```

- [ ] **Step 4: 类型解锁单一来源——修改 PickZombieType 旁路 minWave**

`Board.cpp` 的 `PickZombieType()`（第 448~459 行）当前判定：

```cpp
		if (remainingPoints >= cost && mCurrentWave >= minWave)
			return type;
```

替换为（survival 下不再叠加 `minWave` 门槛，类型可用性完全由 `mSpawnZombieList` 决定，避免每轮重置 `mCurrentWave` 把强僵尸误锁）：

```cpp
		if (remainingPoints >= cost && (mIsSurvival || mCurrentWave >= minWave))
			return type;
```

- [ ] **Step 5: 验证编译**

请主人在 VS 按 F7 编译。
预期：0 错误。

- [ ] **Step 6: 提交（主人确认后）**

```
feat(survival): Board 构造分支、出怪表与逐轮难度系数

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 3: CardSlotManager 清空卡槽

**Files:**
- Modify: `PlantVsZombies/Game/CardSlotManager.h`
- Modify: `PlantVsZombies/Game/CardSlotManager.cpp`

- [ ] **Step 1: 声明 ClearAllCards**

在 `CardSlotManager.h` 的 `void AddCard(Card* card);`（第 42 行附近）之后新增：

```cpp
	// 清空所有卡槽卡牌（销毁 GameObject）。用于生存模式轮间空槽重选。
	void ClearAllCards();
```

- [ ] **Step 2: 实现 ClearAllCards**

`CardSlotManager.cpp` 现有 `AddCard`（第 91~93 行）：

```cpp
void CardSlotManager::AddCard(Card* card) {
	if (card) cards.push_back(card);
}
```

在其后新增（参考 `ChooseCardUI::RemoveAllCards` 的销毁方式，第 50~56 行）：

```cpp
void CardSlotManager::ClearAllCards() {
	DeselectCard();
	for (auto* card : cards) {
		if (card) GameObjectManager::GetInstance().DestroyGameObject(card);
	}
	cards.clear();
	selectedCard = nullptr;
}
```

> 注意：`CardSlotManager.cpp` 顶部需确保已 include `GameObjectManager.h`。若未包含则补 `#include "GameObjectManager.h"`（多数 .cpp 已含，编译报错时再加）。

- [ ] **Step 3: 验证编译**

请主人在 VS 按 F7 编译。
预期：0 错误。

- [ ] **Step 4: 提交（主人确认后）**

```
feat(survival): CardSlotManager 新增 ClearAllCards 支持轮间空槽重选

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 4: Board::OnSurvivalRoundClear + GameScene 轮间选卡子流程

**Files:**
- Modify: `PlantVsZombies/Game/Board.cpp`
- Modify: `PlantVsZombies/Game/GameScene.h`
- Modify: `PlantVsZombies/Game/GameScene.cpp`

- [ ] **Step 1: GameScene.h 新增轮间标志与子流程声明**

在 `GameScene.h` 的 `void GameOver();`（第 57 行附近）之后新增公有声明：

```cpp
	// 生存模式：一轮清空后重新进入选卡子流程（同会话轮间，轻量路径）
	void BeginSurvivalCardSelect();
```

在 `GameScene.h` 私有字段区（`bool mReadyToBackMenu = false;` 第 88 行附近）新增：

```cpp
	bool mSurvivalRoundTransition = false;  // true=正处于同会话轮间转场（ChooseCardComplete 走轻量路径）
	bool mGameUiRegistered = false;         // 防止 ZombieNumber/LevelName/Difficulty 绘制命令重复注册
```

- [ ] **Step 2: 实现 Board::OnSurvivalRoundClear**

在 `Board.cpp` 的 `GameOver()`（第 566 行附近）之后新增：

```cpp
void Board::OnSurvivalRoundClear()
{
	if (!mIsSurvival) return;

	// 推进轮次并重置本轮波次状态（轮次单列在 mSurvivalRound）
	mSurvivalRound++;
	mCurrentWave = 0;
	mMaxWave = SURVIVAL_WAVES_PER_ROUND;
	mZombieCountDown = 20.0f;
	mTrophySpawned = false;
	mHasHugeWaveSound = false;
	mHugeWaveCountDown = 0.0f;
	mNextWaveSpawnZombieHP = 0.0;
	mCurrectWaveZombieHP = 0.0;
	mTotalZombieHP = 0.0;

	// 重算难度（解锁更强僵尸）+ 刷新关卡名
	BuildSurvivalSpawnList(mSurvivalRound);
	UpdateSurvivalLevelName();

	// 回到选卡：暂停波次推进
	mBoardState = BoardState::CHOOSE_CARD;

	// 通知场景重新进入选卡子流程（植物/阳光保留）
	if (mGameScene)
		mGameScene->BeginSurvivalCardSelect();
}
```

> 注意：`mHasHugeWaveSound`、`mHugeWaveCountDown` 是 `Board` 的 private 字段（Board.h 第 96~97 行），在 `Board.cpp` 成员函数内可直接访问。

- [ ] **Step 3: 实现 GameScene::BeginSurvivalCardSelect**

在 `GameScene.cpp` 的 `ChooseCardComplete()`（第 569 行）之前新增。它清空旧卡槽、创建新的选卡 UI（直接到位、不重播平移）、并落一个轮间存档点：

```cpp
void GameScene::BeginSurvivalCardSelect()
{
	mSurvivalRoundTransition = true;

	// 清空上一轮的卡槽（空槽重选），场上植物保留
	if (mCardSlotManager)
		mCardSlotManager->ClearAllCards();

	// 创建新的选卡界面，直接定位到屏内目标位置（相机已在战斗位，不重播背景平移）
	mChooseCardUI = GameObjectManager::GetInstance().
		CreateGameObjectImmediate<ChooseCardUI>(LAYER_UI, this);
	if (auto transform = mChooseCardUI->GetComponent<TransformComponent>()) {
		transform->SetPosition(mChooseCardUITargetPos);
	}
	mChooseCardUI->AddAllCard();
	if (mChooseCardUI->GetButton()) {
		mChooseCardUI->GetButton()->SetEnabled(true);
	}

	// 停在 COMPLETE 阶段，等待玩家点击"一起摇滚吧"
	mCurrentStage = IntroStage::COMPLETE;

	// 轮间存档点（CHOOSE_CARD 状态，依赖 Task 5 放宽保存门槛）
	GameAPP::GetInstance().mGameInfoSaver.SaveLevelData(mBoard.get(), mCardSlotManager);
}
```

- [ ] **Step 4: ChooseCardComplete 增加轮间轻量路径**

`GameScene.cpp` 的 `ChooseCardComplete()`（第 569~617 行）当前结构：开头 `if (mCurrentStage != IntroStage::COMPLETE) return;`，随后设 `READY_SET_PLANT`、转移卡牌、销毁 ChooseCardUI，再注册 ZombieNumber/LevelName/Difficulty 绘制命令。

做两处修改：

(a) 将函数体开头改为——转移卡牌、销毁选卡 UI 后，按是否轮间转场分流：

把现有：

```cpp
void GameScene::ChooseCardComplete()
{
	LOG_INFO("GameScene") << "选卡完成，准备开始游戏";
	if (mCurrentStage != IntroStage::COMPLETE) return;
	mCurrentStage = IntroStage::READY_SET_PLANT;
	mReadyStartPos = Vector(mCurrectSceneX, 0);

	if (mChooseCardUI) {
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager);
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI = nullptr;
	}
```

替换为：

```cpp
void GameScene::ChooseCardComplete()
{
	LOG_INFO("GameScene") << "选卡完成，准备开始游戏";
	if (mCurrentStage != IntroStage::COMPLETE) return;

	if (mChooseCardUI) {
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager);
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI = nullptr;
	}

	// 生存模式同会话轮间：走轻量路径，不重播相机回移、不重建铲子/小推车
	if (mSurvivalRoundTransition) {
		mSurvivalRoundTransition = false;
		mCurrentStage = IntroStage::FINISH;
		mBoard->mBoardState = BoardState::GAME;   // 直接恢复波次推进
		RegisterSurvivalGameUiOnce();
		return;
	}

	// 首次进入/存档重进：正常流程（相机回移后由 READY_SET_PLANT 调 StartGame）
	mCurrentStage = IntroStage::READY_SET_PLANT;
	mReadyStartPos = Vector(mCurrectSceneX, 0);
```

(b) 把原本 `ChooseCardComplete` 末尾那三段 `RegisterDrawCommand("ZombieNumber"/"LevelName"/"Difficulty", ...)` + `SortDrawCommands();`（第 583~616 行）整体抽成一个带去重保护的私有方法 `RegisterSurvivalGameUiOnce()`，并在原位置调用它。

在 `GameScene.h` 私有方法区（`void OpenQuitMenu();` 第 124 行附近）新增声明：

```cpp
	void RegisterSurvivalGameUiOnce();
```

在 `GameScene.cpp` 新增定义（把原三段绘制命令搬进来，加 `mGameUiRegistered` 去重）：

```cpp
void GameScene::RegisterSurvivalGameUiOnce()
{
	if (mGameUiRegistered) return;
	mGameUiRegistered = true;

	RegisterDrawCommand("ZombieNumber",
		[this](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(3, 569), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(5, 570), { 223,186 ,98 ,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
		},
		LAYER_UI);
	RegisterDrawCommand("LevelName",
		[this](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText(mBoard->mLevelName,
				Vector(766, 575), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText(mBoard->mLevelName,
				Vector(768, 576), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
		},
		LAYER_UI);
	RegisterDrawCommand("Difficulty",
		[](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1030, 575), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1032, 576), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
		},
		LAYER_UI);
	SortDrawCommands();
}
```

然后把原 `ChooseCardComplete` 正常流程末尾的三段 `RegisterDrawCommand(...)` + `SortDrawCommands();` 替换为一行：

```cpp
	RegisterSurvivalGameUiOnce();
}
```

> 关于 `mLevelName` 文字：生存模式下它显示"生存模式：无尽 第N面旗"。`RegisterSurvivalGameUiOnce` 内 lambda 每帧读 `mBoard->mLevelName`，`OnSurvivalRoundClear` 已更新它，故轮次会自动刷新，无需重注册。

- [ ] **Step 5: CheckWin 钩子改走 OnSurvivalRoundClear（此时函数已定义，可正常链接）**

`Zombie.cpp` 顶部需可见 `Board`（已 include `Board.h`，现有 `CheckWin` 已使用 `mBoard->mCurrentWave` 等，无需新增 include）。`CheckWin()`（第 149~155 行）当前为：

```cpp
void Zombie::CheckWin() const
{
	if (mBoard && mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0)
	{
		mBoard->CreateTrophy(GetPosition());
	}
}
```

替换为：

```cpp
void Zombie::CheckWin() const
{
	if (mBoard && mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0)
	{
		if (mBoard->mIsSurvival)
			mBoard->OnSurvivalRoundClear();   // 生存模式：进入下一轮，不出奖杯
		else
			mBoard->CreateTrophy(GetPosition());
	}
}
```

（同时把本任务 Files 列表视为含 `PlantVsZombies/Game/Zombie/Zombie.cpp`。）

- [ ] **Step 6: 验证编译 + 首轮转场运行验证**

请主人在 VS 按 F7 编译（`OnSurvivalRoundClear` 已在 Step 2 定义，应能正常链接），0 错误后运行：
1. （需 Task 7 入口或临时手段进入生存模式）打满第 1 轮 10 波。
2. 预期：最后一只僵尸死亡后**不出奖杯**，自动回到选卡界面（出现选卡 UI、卡槽清空、场上植物仍在、阳光数不变）。
3. 重新选卡点"一起摇滚吧"，预期：相机无突跳、铲子未重复出现、进入第 2 轮，僵尸出现更强类型。

- [ ] **Step 7: 提交（主人确认后）**

```
feat(survival): 轮清推进 OnSurvivalRoundClear + CheckWin 钩子 + 轮间选卡轻量路径

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 5: 存读档扩展（CHOOSE_CARD 轮间存档 + survival 字段）

**Files:**
- Modify: `PlantVsZombies/GameInfoSaver.cpp`
- Modify: `PlantVsZombies/Game/GameScene.cpp`

- [ ] **Step 1: SaveLevelData 放宽保存门槛**

`GameInfoSaver.cpp` 的 `SaveLevelData`（第 64~66 行）当前：

```cpp
bool GameInfoSaver::SaveLevelData(Board* board, CardSlotManager* manager)
{
	if (board->mBoardState != BoardState::GAME) return false;
```

替换为（survival 允许 GAME 或 CHOOSE_CARD 都存）：

```cpp
bool GameInfoSaver::SaveLevelData(Board* board, CardSlotManager* manager)
{
	const bool stateOk = (board->mBoardState == BoardState::GAME) ||
		(board->mIsSurvival && board->mBoardState == BoardState::CHOOSE_CARD);
	if (!stateOk) return false;
```

- [ ] **Step 2: SaveLevelData 写入 survival 字段**

在 `SaveLevelData` 的 `j["boardState"] = static_cast<int>(board->mBoardState);`（第 72 行附近）之后新增：

```cpp
	j["isSurvival"] = board->mIsSurvival;
	j["survivalRound"] = board->mSurvivalRound;
```

- [ ] **Step 3: LoadLevelData 恢复 survival 字段**

`GameInfoSaver.cpp` 的 `LoadLevelData` 在恢复 Board 状态处（`board->mBoardState = static_cast<BoardState>(...)`，第 258 行附近）之后新增：

```cpp
	board->mIsSurvival = j.value("isSurvival", false);
	board->mSurvivalRound = j.value("survivalRound", 1);
	if (board->mIsSurvival) {
		board->BuildSurvivalSpawnList(board->mSurvivalRound);
		board->UpdateSurvivalLevelName();
	}
```

> `BuildSurvivalSpawnList` / `UpdateSurvivalLevelName` 是 `Board` public 方法（Task 1 声明、Task 2 实现），`GameInfoSaver` 是 `Board` 的 friend（Board.h 第 61 行），可访问。

- [ ] **Step 4: GameScene::OnExit 放宽存档门槛**

`GameScene.cpp` 的 `OnExit`（第 175~181 行）当前：

```cpp
void GameScene::OnExit() {
	auto& gameApp = GameAPP::GetInstance();
	if (mBoard->mBoardState == BoardState::GAME && !mReadyToRestart) {
		gameApp.mGameInfoSaver.SaveLevelData
		(mBoard.get(), mCardSlotManager);
	}
	gameApp.mGameInfoSaver.SavePlayerInfo();
```

替换条件为（survival 时 CHOOSE_CARD 也存）：

```cpp
void GameScene::OnExit() {
	auto& gameApp = GameAPP::GetInstance();
	const bool saveState = (mBoard->mBoardState == BoardState::GAME) ||
		(mBoard->mIsSurvival && mBoard->mBoardState == BoardState::CHOOSE_CARD);
	if (saveState && !mReadyToRestart) {
		gameApp.mGameInfoSaver.SaveLevelData
		(mBoard.get(), mCardSlotManager);
	}
	gameApp.mGameInfoSaver.SavePlayerInfo();
```

- [ ] **Step 5: 验证编译 + 存读档运行验证**

请主人在 VS 按 F7 编译，0 错误后运行：
1. **轮内退出**：生存模式第 N 轮打到一半，主菜单退出 → 再进入，预期跳过过场直接续局，僵尸/植物/阳光/轮数一致（复用现有 GAME 分支）。
2. **轮间退出**：在轮间选卡界面退出主菜单 → 再进入，预期**重播完整过场动画**后进入选卡，场上植物/阳光/轮数（关卡名"第N面旗"）已恢复。
3. 检查 `./saves/level1000_data.json` 内含 `isSurvival:true`、`survivalRound`、`boardState`。

- [ ] **Step 6: 提交（主人确认后）**

```
feat(survival): 存读档支持 CHOOSE_CARD 轮间存档与 survival 字段

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 6: 失败收尾与边界一致性

**Files:**
- Modify: `PlantVsZombies/Game/Board.cpp`（如需）
- 验证为主，可能无需改动

- [ ] **Step 1: 确认失败删档路径覆盖 survival**

`GameScene::GameOver()`（GameScene.cpp 第 639~644 行）已调用 `gameApp.mGameInfoSaver.DeleteLevelData(mBoard.get())`，因 survival 复用 `level1000_data.json`，失败会自动删档。无需改动，仅确认。

- [ ] **Step 2: 确认预览僵尸不干扰轮清判定**

`OnSurvivalRoundClear` 不重建预览僵尸（`CreatePreviewZombies` 仅在 CHOOSE_CARD 且 `mPreviewZombieList` 为空时生成，且轮间不调用它）。确认轮间回选卡时**不出现**预览僵尸散落，避免 `mZombieNumber` 残留导致下一轮轮清判定异常。
- 运行验证：第 2 轮选卡界面无散落预览僵尸；第 2 轮可正常触发轮清。

- [ ] **Step 3: 确认"重新开始"回到第 1 轮**

失败菜单或暂停菜单"重新开始"走 `mReadyToRestart` → `DeleteLevelData` + `SwitchTo("GameScene")`，重建 Board → 构造分支 `mSurvivalRound=1`。
- 运行验证：生存模式失败后"重新开始"，关卡名显示"第1面旗"，难度回到初始。

- [ ] **Step 4: 提交（如有改动，主人确认后）**

```
fix(survival): 失败删档/预览僵尸/重开回第一轮 边界确认

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Task 7: 生存模式入口接线（便于测试，主人可改/替换 UI）

**Files:**
- Modify: `PlantVsZombies/Game/MainMenuScene.h`
- Modify: `PlantVsZombies/Game/MainMenuScene.cpp`

> 主人说"进入生存模式的 UI 不用你操心"。本任务只提供**最小可测试入口**：把已存在但无回调的 survival 按钮接到"进入生存模式"。主人可随时替换为自己的 UI/过场。

- [ ] **Step 1: MainMenuScene.h 新增进入标志**

在 `MainMenuScene.h` 的 `bool mReadyToSwitchAlmanac = false;`（第 18 行附近）之后新增：

```cpp
	bool mReadyToSwitchSurvival = false;
```

- [ ] **Step 2: survival 按钮接回调**

`MainMenuScene.h` 的 `GameButton::Start()` 中 survival 按钮创建后（第 85~92 行，`mSurvival` 设置 `SetImageKeys` 之后）新增点击回调：

```cpp
		survival->SetClickCallBack([this](bool) {
			DeltaTime::SetPaused(false);
			mMainMenuScene->mReadyToSwitchSurvival = true;
			});
```

> `GameButton` 持有 `mMainMenuScene`（MainMenuScene.h 第 39 行），可访问其公有标志。`DeltaTime` 头文件已在 MainMenuScene.h 第 6 行包含。

- [ ] **Step 3: MainMenuScene::Update 处理进入**

`MainMenuScene.cpp` 的 `Update()` 中，在 `mReadyToSwitchAlmanac` 处理块（第 39~45 行）之后新增：

```cpp
	if (mReadyToSwitchSurvival) {
		mReadyToSwitchSurvival = false;
		auto& gameApp = GameAPP::GetInstance();
		auto& SceneMgr = SceneManager::GetInstance();
		gameApp.GetGraphics().SetCameraPosition(0, 0);
		SceneMgr.SetGlobalData("EnterLevel", std::to_string(SURVIVAL_ENDLESS_LEVEL));
		SceneMgr.SwitchTo("GameScene");
		return;
	}
```

> `SURVIVAL_ENDLESS_LEVEL` 定义在 `Board.h`。`MainMenuScene.cpp` 需能见到该常量；`MainMenuScene.h` 已间接通过 `SceneManager.h`/`GameObject.h` 链，但**未必**包含 Board.h。编译报"未声明标识符"时，在 `MainMenuScene.cpp` 顶部 include 区新增 `#include "Board.h"`。

- [ ] **Step 4: 验证编译 + 端到端运行**

请主人在 VS 按 F7 编译，0 错误后运行完整验收（见下"验收标准"）：主菜单点 survival → 过场 → 选卡 → 打满第 1 轮 → 回选卡 → 第 2 轮更难 → 轮内/轮间退出重进一致 → 失败删档重开回第 1 轮。

- [ ] **Step 5: 提交（主人确认后）**

```
feat(survival): 主菜单 survival 按钮接入生存模式入口（可替换）

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## 验收标准（对应 spec §10）

1. 入口进入生存模式 → 正常过场 → 选卡 → 摆植物 → 打满 10 波（含一大波）。
2. 一轮清空后自动回选卡界面，场上植物与阳光保留，可重新选卡进入第 2 轮。
3. 第 2 轮起单波僵尸明显更强（点数预算更高 + 新类型出现）。
4. 轮内退出主菜单再进入：跳过过场直接续局，状态一致。
5. 轮间（选卡界面）退出主菜单再进入：重播完整过场后进入选卡，植物/阳光/轮数恢复。
6. 失败弹"游戏结束"，删档；"重新开始"回到第 1 轮。
7. 生存模式永不出奖杯、永不推进冒险关进度。

## 已知次要项（v1 可接受，后续可优化）

- 存档重进 CHOOSE_CARD 时，`Board` 构造期的预览僵尸散落基于"第 1 轮"出怪表（此时 `survivalRound` 尚未从存档载入）；纯展示层差异，不影响玩法。需要时可在 `LoadLevelData` 后重建预览。
- `SURVIVAL_BUDGET_GROWTH = 0.35f` 与类型解锁门槛为初值，跑通后按手感调参（集中在 `Board.h` 常量 / `BuildSurvivalSpawnList`）。
