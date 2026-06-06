# 生存模式（无尽）设计文档

- 日期：2026-06-06
- 状态：已与主人确认方案（Approach A），待主人复核 spec
- 范围：仅"生存模式：无尽"（Survival: Endless）。其它生存子模式（白天/黑夜/泳池 5 面旗等）不在本期范围。

## 1. 目标与玩法概述

参考原版《植物大战僵尸》"生存模式：无尽"，在本作中实现可无限循环的生存玩法：

- 玩家进入后像普通关一样选卡、摆植物、抵御一轮波次。
- 一轮（一面旗）= 固定波数，以一大波（旗帜波）收尾。
- 一轮清空后**回到选卡界面**调整阵容；**场上已种植物保留、阳光保留**；进入下一轮。
- 僵尸难度逐轮递增：**单波血量预算递增 + 逐轮解锁更强僵尸类型**。
- 失败条件与普通关相同：僵尸吃掉脑子（且割草机已用尽）。
- 无胜利终点：永不产生奖杯、永不推进 `mAdventureLevel`。

进入生存模式的 UI 由主人自行接（主菜单已有 survival 按钮，当前无回调）。本设计只提供入口契约与完整的模式功能。

## 2. 选定方案（Approach A）

在现有 `Board` 上加 `mIsSurvival` 标志（由特殊 level 号触发）+ 少量生存状态字段，**复用**现有 `GameScene` 的 Intro 状态机、Card 选卡流程与存档系统，按标志走分支。

理由：`GameScene` 与 `Board` 强耦合（Intro 状态机在 `GameScene`、波次在 `Board`），另起 `SurvivalScene/SurvivalBoard` 会大量复制过场/存档代码。加标志位复用现有机器的爆炸半径最小，符合 YAGNI。

被否决方案：
- B（子类化 `SurvivalBoard`/新 `SurvivalScene`）：隔离干净但复制 Intro/存档/场景注册，工作量大、易漂移。
- C（通用 `GameMode` 枚举 + 策略对象）：最可扩展但对"先做无尽"过度抽象。

## 3. 模式识别与入口契约

- 新增常量 `SURVIVAL_ENDLESS_LEVEL = 1000`（> 50，避开 `Trophy.cpp:53` 的冒险关推进逻辑）。
- 入口（UI 由主人接）：
  ```cpp
  SceneManager::GetInstance().SetGlobalData("EnterLevel", std::to_string(SURVIVAL_ENDLESS_LEVEL));
  SceneManager::GetInstance().SwitchTo("GameScene");
  ```
  不引入额外耦合，复用现有 `EnterLevel` 全局数据通道。
- `Board` 构造函数：
  - `mIsSurvival = (level == SURVIVAL_ENDLESS_LEVEL)`。
  - survival 时 `mLevelName = u8"生存模式：无尽 第N面旗"`（N = `mSurvivalRound`，每轮刷新）。
  - survival 时**不**走 `LoadSpawnListFromJson` 的关卡号匹配，改用生存出怪逻辑（见 §5）。
  - 背景固定 `Background::GROUND_DAY`（`GetBackgroundID` 对未知 level 已默认返回 GROUND_DAY，无需改动，但需确认 1000 落入 else 分支——确认成立）。

## 4. 轮（旗）模型

- 新增字段 `int mSurvivalRound = 1;`（第几面旗，从 1 起）。
- 每轮波数固定常量 `SURVIVAL_WAVES_PER_ROUND = 10`，最后一波为一大波/旗帜波（复用现有 `mCurrentWave % 10 == 0` 一大波逻辑，10 波正好对齐）。
- **每轮重置 wave 计数**：每轮 `mMaxWave = SURVIVAL_WAVES_PER_ROUND`（=10）、`mCurrentWave` 重置为 0；轮次单独记在 `mSurvivalRound`。这样能直接复用现有"第 10 波 = 一大波 + 终波音效/提示"判断，无需改 `UpdateLevel` 的波次分支。

## 5. 难度递增（每轮血量预算 + 解锁更强僵尸）

### 5.1 血量/点数预算
- 现有 `CalculateWaveZombiePoints()` 用 `mCurrentWave` 算点数。survival 时额外乘一个随轮放大的系数：
  ```
  budget *= (1.0f + SURVIVAL_BUDGET_GROWTH * (mSurvivalRound - 1));   // SURVIVAL_BUDGET_GROWTH = 0.35f（可调）
  ```
- 系数 `0.35f` 为初值，跑通后可调。集中为一个具名常量，便于调参。

### 5.2 僵尸类型解锁
- survival 专用 `{round → 解锁类型}` 表（写死在 Board，便于第一版；后续可外置 json）。初版门槛建议：
  - 第 1 轮：`ZOMBIE_NORMAL`
  - 第 2 轮：+ 路障（ConeZombie）
  - 第 3 轮：+ 撑杆（Polevaulter）、读报（PaperZombie）
  - 第 4 轮：+ 铁桶（BucketZombie）
  - 第 5 轮起：全部已实现类型
  - （具体类型以 `ZombieType.h` 现有枚举为准，落地时核对）
- 每轮在进入前根据 `mSurvivalRound` 重建 `mSpawnZombieList`。
- 现有 `PickZombieType` 内的 `minWave` 门槛机制：survival 下改用 `mSurvivalRound` 折算（或直接依赖重建后的 `mSpawnZombieList`，避免双重门槛冲突——落地时择一，倾向只靠 `mSpawnZombieList` 控制可用类型，保持单一事实来源）。

## 6. 轮结束 → 回选卡（核心特殊处理）

- 胜利判定改写：`Zombie::CheckWin()`
  ```cpp
  if (mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0) {
      if (mBoard->mIsSurvival) mBoard->OnSurvivalRoundClear();
      else                     mBoard->CreateTrophy(GetPosition());
  }
  ```
- 新增 `Board::OnSurvivalRoundClear()`：
  1. `mSurvivalRound++`。
  2. 重置 `mCurrentWave = 0`、`mZombieCountDown`、`mTrophySpawned = false`、`mHasHugeWaveSound = false`、`mHugeWaveCountDown = 0`。
  3. 重算难度（重建 `mSpawnZombieList`、更新预算系数）。
  4. `mBoardState = CHOOSE_CARD`。
  5. 通知 `GameScene` 重新进入选卡子流程：种子槽 + `ChooseCardUI` 重新滑入，**植物/阳光保留**；相机已在战斗位，故跳过背景大平移（仅重播种子槽/选卡 UI 滑入）。
  6. 选卡确认走现有 `ChooseCardComplete → StartGame` 进入下一轮。
- **轮间存档点**：进入 CHOOSE_CARD 后立刻触发一次存档（见 §7），保证"轮数结束特殊读档"成立。
- 卡牌处理：轮间**空槽重选**（清空已选卡槽，玩家重新选）；场上已种植物保留（即使该植物本轮未被选中也保留）。

### 6.1 GameScene 选卡子流程复用
- 当前 `GameScene` 的 Intro 状态机：`BACKGROUND_MOVE → SEEDBANK_SLIDE → COMPLETE → READY_SET_PLANT → FINISH`。
- 轮间转场（无退出）：从 `SEEDBANK_SLIDE` 重新起（跳过 `BACKGROUND_MOVE`，相机已在战斗位）；`READY_SET_PLANT` 的相机回移在 survival 轮间需特殊处理——相机本就在战斗位，避免重复平移造成镜头跳动（落地时：survival 轮间把 `READY_SET_PLANT` 的相机插值起点=终点，或直接跳过该阶段进入 `StartGame`）。
- 需要复位 `GameScene` 的一次性动画标记（`mSeedbankAdded` 等），以便重新创建 `ChooseCardUI`。落地时新增一个 `GameScene::BeginSurvivalCardSelect()` 入口集中处理这些复位与子流程重启。

## 7. 存档/读档扩展（特殊处理重点）

### 7.1 保存（`GameInfoSaver::SaveLevelData`）
- 放宽门槛：现为 `if (board->mBoardState != BoardState::GAME) return false;`。survival 时允许 `GAME` 或 `CHOOSE_CARD` 都保存。
- 新增字段：`isSurvival`、`survivalRound`。
- 轮间（CHOOSE_CARD）也持久化：场上植物（已有逻辑遍历 EntityManager）、阳光、`survivalRound`；卡牌槽轮间通常为空（空槽重选），按现状序列化即可。
- 文件名：复用 `level1000_data.json`（特殊 level 号方案）。

### 7.2 读取（`GameInfoSaver::LoadLevelData` + `GameScene::OnEnter`）
- 恢复 `isSurvival`、`survivalRound`。
- 分支：
  - `boardState == GAME`（轮内退出）→ 跳过过场直接续局。**现有逻辑天然支持**（`OnEnter` 检测 `GAME` → 跳过 Intro、`StartGame`）。
  - `boardState == CHOOSE_CARD`（轮间退出）→ 恢复 round/植物/阳光，**重播完整过场动画**（主人选择）：相机从头平移 → 种子槽滑入 → 选卡。即 `OnEnter` 维持 CHOOSE_CARD 分支播放完整 Intro，同时把已存植物画在场上。
    - 注意：此分支下植物已在场，但 Intro 仍从 `BACKGROUND_MOVE` 起，需保证植物随相机平移正确显示（植物坐标为世界坐标，相机平移自然带出，预期无额外处理；落地验证）。

> 说明：主人最初括号提到"回到过场动画最后一帧前"，但在澄清中**明确选择"重播一遍完整过场动画"**。本设计以主人的明确选择为准（重进 = 完整过场）。如需改回"跳到选卡那一帧"，仅需在 OnEnter 的 CHOOSE_CARD 分支按 GAME 分支的方式跳过 Intro 并手动落到 `COMPLETE` 阶段。

### 7.3 收尾
- survival **永不**走 Trophy / `mAdventureLevel++` / `mHaveCards.push_back`。
- 失败 `GameOver`：同普通关弹"游戏结束"框，删 `level1000_data.json`（复用现有 `DeleteLevelData`）。
- `GameScene::OnExit`：当前 `if (mBoardState == GAME && !mReadyToRestart) Save`。改为 survival 时 `(GAME || CHOOSE_CARD)` 也存，保证轮间退主菜单能续。

## 8. 边界与一致性

- `mTrophySpawned` 每轮重置，避免跨轮误判残留。
- `mZombieNumber` 跨轮归零检查：轮清判定依赖 `mZombieNumber <= 0`，轮间不应残留预览僵尸（survival 轮间是否重建预览僵尸？——倾向轮间**不**重建预览僵尸，避免干扰 `mZombieNumber` 与轮清判定；首轮进入时的预览僵尸照常）。
- 失败菜单"重新开始"：survival 重开应回到第 1 轮（删档 + 重进 GameScene，`mSurvivalRound` 自然回 1）。
- 速度按钮、暂停菜单、图鉴跳转等现有 UI 行为对 survival 无需改动。

## 9. 涉及改动清单（实现计划阶段细化）

- `Board.h/.cpp`：`mIsSurvival`、`mSurvivalRound`、常量、构造分支、`OnSurvivalRoundClear()`、难度/出怪表 survival 分支、`CalculateWaveZombiePoints` 系数。
- `Zombie.cpp::CheckWin()`：survival 分支。
- `GameScene.h/.cpp`：`BeginSurvivalCardSelect()` 子流程重启、`OnEnter` CHOOSE_CARD 恢复分支、`OnExit` 存档门槛、轮间 `READY_SET_PLANT` 相机处理。
- `GameInfoSaver.cpp`：`SaveLevelData` 门槛放宽 + survival 字段；`LoadLevelData` 恢复 survival 字段。
- 常量定义位置：`SURVIVAL_ENDLESS_LEVEL`、`SURVIVAL_WAVES_PER_ROUND`、`SURVIVAL_BUDGET_GROWTH`（建议集中在 `Board.h` 或 `Definit.h`）。
- （入口 UI 由主人接，不在本期实现范围，但提供契约。）

## 10. 验收标准

1. 从入口进入生存模式，正常播放过场 → 选卡 → 摆植物 → 打满 10 波（含一大波）。
2. 一轮清空后自动回到选卡界面，场上植物与阳光保留，可重新选卡进入第 2 轮。
3. 第 2 轮起单波僵尸明显更强（血量预算更高 + 新类型出现）。
4. 轮内退出主菜单再进入：跳过过场直接续局，状态一致。
5. 轮间（选卡界面）退出主菜单再进入：重播完整过场动画后进入选卡，植物/阳光/轮数恢复。
6. 失败弹"游戏结束"，删档；重开回到第 1 轮。
7. 生存模式永不出奖杯、永不推进冒险关进度。
