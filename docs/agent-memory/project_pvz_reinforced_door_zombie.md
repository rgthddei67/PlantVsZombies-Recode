---
name: project_pvz_reinforced_door_zombie
description: 2026-07-22 加固铁门僵尸：270 本体/1230 门、持门普通伤害10与灰烬100、免魅惑、大喷双伤只伤门、候选源头每波最多2只
metadata:
  node_type: memory
  type: project
---

# 加固铁门僵尸

## 契约与实现

- 类型 `ZOMBIE_REINFORCED_DOOR`，类 `ReinforcedDoorZombie : DoorZombie`；当前本体 270、门 1230，青绿色三阶段门贴图由 `scripts/recolor_reinforced_door.ps1` 从原版门可重复生成。
- 门还在时，`AdjustIncomingDamage` 把植物与僵尸普通伤害在词条缩放后钳到每次 10；门掉后取消该上限。大喷/寒冰大喷先经 `ModifyFumeDamage` 乘 2，再走普通植物伤害链，所以持门时仍最终为 10，掉门后每喷 40。
- 灰烬统一走 `Zombie::TakePlantAshDamage` 和 `DamageSource::PLANT_ASH`。持门时 `CanBeCharred=false` 且每次灰烬最终最多 100；门掉后恢复普通化灰与完整灰烬伤害。大嘴花直杀走返回 `bool` 的 `TakePlantInstantKill`：持门时把尝试降级为 20 点基础普通伤害（最终 10）并返回 false，门掉后直接吞掉并返回 true。小推车仍以 `OTHER + INT32_MAX` 正常秒杀。
- 本类型 `CanBeCharmed=false`。醒着的魅惑菇仍由 `Zombie::EatTarget` 立即 `Die()` 并播放 `SOUND_FLOOP`，但随后 `StartMindControlled()` 被豁免守卫拒绝，僵尸阵营与门/本体血量都不变。
- 持门时 `BlocksFumePiercing=true`。FumeShroom 按 X 由近到远结算，命中它后停止；本击改用非穿透且丢弃破门溢出，门存在时只伤门、本体不掉血。粒子用同一 collider 左沿作为 `clipRightX`，不改 FumeCloud XML 长度。门掉后恢复大喷全额双倍本体伤害。
- 正式波次生成通过 `Board::ResolveWaveZombieType` 计数；每波前两只保留本类型，第三只起返回 `NUM_ZOMBIE_TYPES`。`TrySummonZombie` 在选行和扣预算前直接 `continue`，继续抽取其他候选，禁止回退普通铁门干扰出怪池。计数在新波/生存轮清时归零并进入存档；开发/AutoTest 的 `spawn_zombie` 直造不占配额。
- 生存池配置：`weight=2100, appearWave=6, survivalRound=6`。冒险模式在 2-8 用“普通铁门 + 加固铁门”做独立家族教学（玩家刚取得毁灭菇，能直接观察灰烬抗性），2-9 作为 8 种重点机制综合池必定包含本类型；末关撤下泛用铁桶维持原池大小。

## 验证

- `smoke_reinforced_door_wave_cap.json`：同波第 1/2 只为加固门，第 3 个候选完全跳过且随后普通僵尸正常生成；新波计数清零后可再次生成。
- `smoke_night_spawnlists.json`：逐关验证黑夜出怪节奏；2-8/2-9 均含本类型，2-9 仍为 8 种池。
- 2026-07-22 当前刷新改动：clang-release 零警告，桌面可见 `smoke_reinforced_door_wave_cap` 退出 0；交互脚本中的旧 500/1100 数值断言属于此前平衡快照，后续运行须先同步到当前 270/1230。
