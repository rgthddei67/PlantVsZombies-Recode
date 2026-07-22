---
name: project_pvz_reinforced_door_zombie
description: 2026-07-22 加固铁门僵尸：500 本体/1000 门、持门植物单次15、灰烬100且免化灰、大喷双伤并阻断判定与粒子、每波最多2只
metadata:
  node_type: memory
  type: project
---

# 加固铁门僵尸

## 契约与实现

- 类型 `ZOMBIE_REINFORCED_DOOR`，类 `ReinforcedDoorZombie : DoorZombie`；本体 500、门 1000，青绿色三阶段门贴图由 `scripts/recolor_reinforced_door.ps1` 从原版门可重复生成。
- 门还在时，`AdjustIncomingDamage` 把植物普通伤害在词条缩放后钳到每次 15；门掉后取消该上限。大喷/寒冰大喷先经 `ModifyFumeDamage` 乘 2，再走普通植物伤害链，所以持门时仍最终为 15，掉门后每喷 40。
- 灰烬统一走 `Zombie::TakePlantAshDamage` 和 `DamageSource::PLANT_ASH`。本类型 `CanBeCharred=false`，每次灰烬最终最多 100；樱桃、毁灭菇、土豆雷都不能化灰或秒杀它。大嘴花等植物直杀统一走 `TakePlantInstantKill`，本类型降级为 15 点普通植物伤害。小推车仍以 `OTHER + INT32_MAX` 正常秒杀。
- 持门时 `BlocksFumePiercing=true`。FumeShroom 按 X 由近到远结算，命中它后停止；粒子用同一 collider 左沿作为 `clipRightX`，不改 FumeCloud XML 长度。
- 正式波次生成通过 `Board::ResolveWaveZombieType` 计数；每波前两只保留本类型，第三只起回退同成本普通铁门。计数在新波/生存轮清时归零并进入存档。开发/AutoTest 的 `spawn_zombie` 直接生成不占正式波次配额。
- 生存池配置：`weight=2100, appearWave=6, survivalRound=6`。冒险模式在 2-8 用“普通铁门 + 加固铁门”做独立家族教学（玩家刚取得毁灭菇，能直接观察灰烬抗性），2-9 作为 8 种重点机制综合池必定包含本类型；末关撤下泛用铁桶维持原池大小。

## 验证

- `smoke_reinforced_door.json`：500/1000、普通伤害 15、门前截断、掉门后大喷 40 对普通 20、门在/掉门灰烬各 100、免大嘴花直杀、小推车击杀。
- `smoke_reinforced_door_wave_cap.json`：同波第 1/2 只为加固门、第 3 只回退普通门，新波计数清零。
- `smoke_reinforced_door_potatomine.json`：土豆雷后本体 500、门 900。
- `smoke_night_spawnlists.json`：逐关验证黑夜出怪节奏；2-8/2-9 均含本类型，2-9 仍为 8 种池。
- 回归：`smoke_potatomine`、`smoke_icefumeshroom`、`smoke_door_fume_death` 均通过；`smoke_doomshroom` 与伤害相关断言通过，旧鼠标点击相邻弹坑格步骤仍未种下植物，独立失败需另案处理。
