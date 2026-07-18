# 生存词条平衡与出怪池多样性 Implementation Plan

**Goal:** 新增植物韧性、阳光增产、卡片加速三个词条，收敛过强僵尸诅咒，并把每轮出怪池限制为最多 8 种且带 ±1～2 种随机波动。

**Architecture:** 三个新词条均使用 `SurvivalPerkManager` 的原型 A 中性倍率；分别接入 `Plant::TakeDamage`、`Board::AddSun`、`CardComponent::Update`。出怪只改 `BuildSurvivalSpawnList` 的候选过滤与目标数量计算。

## Task 1：先写失败的 AutoTest

- 新建 `autotest/scripts/smoke_perks_balance.json`，引用三个尚不存在的新 key 并写数学闭合断言。
- 更新 `autotest/scripts/smoke_survival_spawn_round.json`，引用尚不存在的 `spawnTypeCount`。
- 用当前二进制运行，确认分别因未知词条或缺少断言字段失败。

## Task 2：实现 manager 数据与聚合 API

- `PerkType.h` 追加三个枚举，更新现有数值注释。
- `SurvivalPerkManager.cpp` 更新四项平衡值并追加三行 `kPerks`。
- `SurvivalPerkManager.{h,cpp}` 增加：
  - `GetPlantDamageTakenMultiplier` / `ScaleDamageToPlant`
  - `GetSunIncomeMultiplier` / `ScaleSunIncome`
  - `GetPlantCardRechargeMultiplier`
- 免伤倍率显式夹底 55%，避免浮点和损坏状态越过设计下限。

## Task 3：接入三个运行时钟点

- `Plant::TakeDamage` 在扣血前调用 `ScaleDamageToPlant`。
- `Board::AddSun` 调用 `ScaleSunIncome` 后再做 `MAX_SUN` 钳制。
- `CardSlotManager` 加只读 `GetBoard()`；`CardComponent::Update` 用 `dt * GetPlantCardRechargeMultiplier()` 推进冷却。
- 所有 0 层路径保持单位元，不散写生存模式判断。

## Task 4：实现出怪种类上限与波动

- `Board.h` 增加总种类上限 8、波动幅度 1～2 的常量。
- `BuildSurvivalSpawnList` 排除 `weight <= 0` 候选。
- 第 3 轮起把基础总种类先钳到 8，再随机选择正负号和 1～2 幅度，最终按候选数与上限夹紧。
- 保持普通僵尸必出和无放回抽样。

## Task 5：补齐 AutoTest 可见性与断言

- `TestDriver.cpp` 的 `kPerkNames`、`perks.stacks` 加三个词条。
- dump 整数投影 `damageToPlantOn100`、`sunIncomeOn100`、`cardRechargeOn1000`。
- 根状态增加 `spawnTypeCount`。
- 更新所有受数值变化影响的旧断言；固定 Seed 运行出怪脚本后，把已验证的实际计数写成断言。

## Task 6：文档记忆、构建与回归

- 更新 `docs/agent-memory/project_pvz_perk_system.md`、`project_pvz_survival_spawn_round_table.md` 和索引摘要。
- 按项目指南导入 VS x64 环境，运行 `cmake --preset clang-release` 与 `cmake --build --preset clang-release`，要求 0 warning / 0 error。
- 从 `build/clang-release` 运行新脚本和最小相关既有脚本，检查退出码、`run.log`、状态 JSON；必要时检查截图。
- 复核 diff 和工作树，仅提交本任务文件；满足 fast-forward 与上游明确条件时 push。
