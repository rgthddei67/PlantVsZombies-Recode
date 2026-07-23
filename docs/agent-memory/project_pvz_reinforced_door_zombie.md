---
name: project_pvz_reinforced_door_zombie
description: 2026-07-23 加固铁门僵尸当前源码：300 本体/920 门、持门植物普通伤害10与灰烬320、免魅惑、大喷双伤只伤门
metadata:
  node_type: memory
  type: project
---

# 加固铁门僵尸

## 契约与实现

- 类型 `ZOMBIE_REINFORCED_DOOR`，类 `ReinforcedDoorZombie : DoorZombie`；2026-07-23 当前源码为本体 300、门 920，青绿色三阶段门贴图由 `scripts/recolor_reinforced_door.ps1` 从原版门可重复生成。主人会直接调整这些数值，后续平衡与断言必须先读取当前源码，不得依据本文件中的历史数字。
- 门还在时，`AdjustIncomingDamage` 把植物普通伤害在词条缩放后钳到每次 10；门掉后取消该上限。大喷/寒冰大喷先经 `ModifyFumeDamage` 乘 2，再走普通植物伤害链，所以持门时仍最终为 10，掉门后每喷 40。
- 灰烬统一走 `Zombie::TakePlantAshDamage` 和 `DamageSource::PLANT_ASH`。持门时 `CanBeCharred=false` 且每次灰烬最终最多 320；门掉后恢复普通化灰与完整灰烬伤害。大嘴花直杀走返回 `bool` 的 `TakePlantInstantKill`：持门时把尝试降级为 10 点基础普通伤害并返回 false，门掉后直接吞掉并返回 true。小推车仍以 `OTHER + INT32_MAX` 正常秒杀。
- 本类型 `CanBeCharmed=false`。醒着的魅惑菇仍由 `Zombie::EatTarget` 立即 `Die()` 并播放 `SOUND_FLOOP`，但随后 `StartMindControlled()` 被豁免守卫拒绝，僵尸阵营与门/本体血量都不变。
- 持门时 `BlocksFumePiercing=true`。FumeShroom 按 X 由近到远结算，命中它后停止；本击改用非穿透且丢弃破门溢出，门存在时只伤门、本体不掉血。粒子用同一 collider 左沿作为 `clipRightX`，不改 FumeCloud XML 长度。门掉后恢复大喷全额双倍本体伤害。
- 正式波次生成通过 `Board::ResolveWaveZombieType` 计数；每波前两只保留本类型，第三只起返回 `NUM_ZOMBIE_TYPES`。`TrySummonZombie` 在选行和扣预算前直接 `continue`，继续抽取其他候选，禁止回退普通铁门干扰出怪池。计数在新波/生存轮清时归零并进入存档；开发/AutoTest 的 `spawn_zombie` 直造不占配额。
- 生存池配置：`weight=2100, appearWave=6, survivalRound=6`。冒险模式在 2-8 用“普通铁门 + 加固铁门”做独立家族教学（玩家刚取得毁灭菇，能直接观察灰烬抗性），2-9 作为 8 种重点机制综合池必定包含本类型；末关撤下泛用铁桶维持原池大小。

## 验证

- `smoke_reinforced_door_wave_cap.json`：同波第 1/2 只为加固门，第 3 个候选完全跳过且随后普通僵尸正常生成；新波计数清零后可再次生成。
- `smoke_night_spawnlists.json`：逐关验证黑夜出怪节奏；2-8/2-9 均含本类型，2-9 仍为 8 种池。
- 2026-07-23 已按当前源码同步 `smoke_reinforced_door.json` 与 `smoke_reinforced_door_potatomine.json`：桌面可见运行均退出 0，覆盖 300/920、普通攻击扣门 10、灰烬扣门 320、破门、门后灰烬直杀、大嘴花、魅惑菇和小推车。
