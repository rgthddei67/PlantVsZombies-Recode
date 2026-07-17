---
name: project_pvz_fumeshroom_attack
description: 大喷菇 FumeShroom 攻击补全 + Zombie::TakeDamage 护盾穿透参数（commit e443375，未push）
metadata:
  node_type: memory
  type: project
  originSessionId: 0cee5006-2e13-4765-8247-fbc66fa5d3ef
---

2026-06-24 完成大喷菇攻击（接续前一 commit「大喷菇50%（缺少攻击特性）」），commit `e443375` 在 master 未 push。

**根因**：FumeShroom 原本只在第 27 帧 frame event 播 FumeCloud 粒子 + SOUND_FUME，从不造成伤害（纯视觉）。

**实现**（全参照 C# `Plant.cs` / `Zombie.cs`，主人选「还原穿透」档）：
- 原版大喷菇**不发子弹**，是瞬时区域攻击：`DoRowAreaDamage(20, 2U)`。本仓库 `FumeShroom::FumeAttack()` 在**复用现有第27帧 frame event**里调用（喷雾成形即攻击，未新增 frame event），`ForEachZombieInRow` 重新扫本行、对 dx∈[0,380]（`kFumeReach`，与 HasZombieInRow 共用）全部僵尸 `TakeDamage(20, penetrateShield=true)`。
- **护盾穿透**＝原版 `2U` 标志语义：`Zombie::TakeDamage(int, bool penetrateShield=false)` 新增参数。穿透时 `TakeShieldDamage(damage)` 让二类护盾照常受损/掉落（触发报纸狂暴），但 `remainingDamage=damage` 重置回全额 → 头盔+本体照吃。**只穿透二类护盾（铁门/报纸/梯子），一类头盔（路障/铁桶）不穿透**，与原版一致。
- 全仓库仅 `FastPaperZombie`/`FastBucketZombie` 两处 override `Zombie::TakeDamage`（加随机免伤），已同步透传新参数；其随机免伤对喷雾命中仍生效。**`Plant::TakeDamage` 是独立继承链（CherryBomb override），未动。**默认参数让所有旧 1-arg 调用（Board/Bullet/LawnMower）零改动。

**验证**（AutoTest，-Seed 42，**夜间关 level 10**（蘑菇白天睡）；temp 脚本走 scratchpad 不污染 repo）：普通僵尸每发 -20 本体；报纸僵尸**护盾仍 440/500 时本体已 240/300** → 标准 shield→body 模型下护盾未破本体不可能掉血，穿透铁证。每发恰好 -20 双扣。

**前瞻**：未来 Gloom-shroom（大喷菇升级，喷三行）直接复用此穿透路径——对 `mRow±1` 三行调 `TakeDamage(dmg, true)` 即可，无需再碰 Zombie 伤害代码。参见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)（蘑菇夜间关/AutoTest 字段坑）、[project_pvz_zombie_row_index](project_pvz_zombie_row_index.md)（ForEachZombieInRow 按行索引）。
