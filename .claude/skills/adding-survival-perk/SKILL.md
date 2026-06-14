---
name: adding-survival-perk
description: Use when adding a new 生存模式词条 (survival perk) to PvZ — covers the 3 perk archetypes, their chokepoints, save contract, and AutoTest verification pattern proven across 6 perks.
---

# 给生存模式加词条 (Survival Perk)

统一管理器 `SurvivalPerkManager`（`Game/Perk/`，`Board` 值成员）。核心 = **中性默认即自动失效**：0 层时 getter 返回单位元，钟点对非生存关/未获取词条自动 no-op，**永远不要**散写 `if(mIsSurvival)`。

## 第一步：先归类到 3 种原型之一

| 原型 | 状态在哪 | 例子 | 钟点 |
|---|---|---|---|
| **A. 无状态倍率** | 仅 manager 层数 | 植物伤害、僵尸伤害、僵尸血量、僵尸免伤 | 在伤害/血量计算点读 getter |
| **B. per-entity 状态** | 实体上加字段 + 存档 | 僵尸前 N 次免伤 | 实体方法首行消费 + 出生初始化 + Save/LoadProtectedData |
| **C. Board 全局脉冲** | Board 计时器（不存档） | 植物回血 | `Board::UpdateLevel` 累加计时器，触发时遍历 |

选 A 永远优先（最简）；只有"每个实体各自计数"才用 B；"全场周期性一起发生"才用 C。

## 通用步骤（所有原型）

1. **`PerkType.h`**：枚举 `COUNT` 前加项。
2. **`SurvivalPerkManager.cpp` `kPerks[]`**：对应位置加一行 `{key, nameZh, descZh, perStack, maxStacks}`（顺序必须与枚举一致，`static_assert` 强制）。不限层用 `maxStacks=9999`。
3. **`SurvivalPerkManager.{h,cpp}`**：加聚合 getter。倍率型镜像现有 `GetXxxMultiplier()` + `ScaleXxx()`（走 `RoundScale`，已四舍五入+夹底≥1）。**计数型用纯整数乘 `(int)perStack * stacks`，绝不写 `static_cast<int>(x+0.5f)`——那是截断不是取整，是误导死代码**（perStack 是精确整数才碰巧对）。
4. **存档**：层数走现有 `SurvivalPerkManager::Save/Load`（按 key 字符串，新 key 旧档缺失→0），**无需改 GameInfoSaver 的词条段**。
5. **AutoTest 接入**（UI 前唯一注入手段）：
   - `TestDriver.cpp` `kPerkNames` 加 `PK(NEW_TYPE)`。
   - `dump_state` 的 `perks` 块加 `stacks[...]` + getter 暴露值。
   - 写 `autotest/scripts/smoke_perks_<name>.json`，**用数学闭合断言**（如 `ScaleX(100)==115`），不赌时序。
6. **构建**：`clang-release` 必须 0 warning/0 error（唯一报全量警告的配置）。
7. **回归**：跑既有 `smoke_perks.json`，确认旧词条 dump 值**逐位不变**（中性默认的证明）。

## 原型 B 专属（per-zombie 状态）

- 字段加在 `Zombie.h`（如 `int mFreeHitsRemaining = 0;`，默认值=中性）。
- 消费放 `Zombie::TakeDamage` **首行**（`damage<=0` 后、免伤倍率 + `SetGlowingTimer` 之前——吸收的伤害不该闪白光）。
- 出生初始化只在 `Board::CreateZombie` 的 `!isPreview && !skipsettings` 块（波次生成路径），**绝不**在 `CreateZombieWithID`（读档路径，由 Load 还原，否则覆盖存档值）。
- 存档进 `Zombie::Save/LoadProtectedData`（基类无条件调用，不受子类 `SaveExtraData` 覆盖；旧档 `j.value(key,0)`）。**存档会在 GAME 状态带场上实体落盘**，所以 per-entity 状态必须持久化。

## 原型 C 专属（Board 全局脉冲）

- 计时器加 `Board.h`（如 `float mPlantRegenTimer = 0.0f;`），仿 `mUpdateHPCheckTimer` 范式。
- 脉冲逻辑放 `Board::UpdateLevel`，**在 `if(mCurrentWave>=mMaxWave)return` 之前**（否则出怪完就停），`mBoardState==GAME` 守卫之后。
- `timer += dt; if(timer>=interval){timer=0; int amt=GetXxx(); if(amt>0){遍历...}}`——`amt>0` 守卫让无词条时整个 O(n) 循环跳过，零开销。O(n) 只在脉冲帧发生（非每帧），与 memory 记的"每帧 `GetAllGameObjects` foot-gun"不同量级。
- 计时器**不存档**（读档从 0 重计，损失≤一个周期，换零存档改动）；但脉冲产生的实体状态（如过量治疗的血量）天然随实体字段存档。
- 读 `Plant` 的 protected 成员需加公有 getter（`Board` 非 `Plant` 友元）。

## 流程

走 brainstorm→spec→plan→subagent-driven 全流程；数值/策略类决策（间隔、上限、是否过量治疗）先和主人确认。第二批 spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-14-perk-batch2*`，可作模板。

## 关联记忆

`[[project_pvz_perk_system]]`（6 词条全清单 + 钟点 + UI 下次任务的配对规则）、`[[project_pvz_autotest_suite]]`。
