---
name: project_pvz_reinforced_door_zombie
description: 加固铁门僵尸当前源码：270 本体/1030 门、持门植物普通伤害10与灰烬320、仙人掌尖刺帧伤1、免魅惑、大喷双伤只伤门
metadata:
  node_type: memory
  type: project
---

# 加固铁门僵尸

## 契约与实现

- 类型 `ZOMBIE_REINFORCED_DOOR`，类 `ReinforcedDoorZombie : DoorZombie`；2026-07-30 当前源码为本体 270、门 1030，青绿色三阶段门贴图由 `scripts/recolor_reinforced_door.ps1` 从原版门可重复生成。主人会直接调整这些数值，后续平衡与断言必须先读取当前源码，不得依据本文件中的历史数字。
- 门还在且从正面命中时，`AdjustIncomingDamage` 把植物普通伤害在词条缩放后钳到每次 10；门掉后取消该上限。`Bullet` 背击通过 `bypassShield` 完全绕过门并取消持门上限，直接对后层结算完整伤害。大喷/寒冰大喷先经 `ModifyFumeDamage` 乘 2，再走普通植物伤害链，所以正面持门时仍最终为 10，掉门后每喷 40。
- 仙人掌尖刺的帧伤会拆成多个独立 `TakeDamage(1)`，普通单击上限无法表达“本帧总伤害为 1”。因此 `Zombie::ModifySpikeFrameDamage` 在倍速额度累计前提供目标侧修正；本类型正面持门时返回 1，掉门或背击时恢复尖刺基础 3，0.5x/1x/2.0x 的等伤语义保持不变。
- 灰烬统一走 `Zombie::TakePlantAshDamage` 和 `DamageSource::PLANT_ASH`。持门时 `CanBeCharred=false` 且每次灰烬最终最多 320；门掉后恢复普通化灰烬与完整灰烬伤害。大嘴花直杀走返回 `bool` 的 `TakePlantInstantKill`：持门时返回 false，再由 `AdjustRejectedChomperBiteDamage()` 把大嘴花统一默认值调整为 10 点基础普通伤害；门掉后直接吞掉并返回 true。小推车仍以 `OTHER + INT32_MAX` 正常秒杀。
- 缠绕水草通过 `ResistsTangleKelpDrowning()` 接入同一“持门时抗直杀、掉门后恢复普通规则”契约。持门目标被原地锚定并保持 `anim_grab` 5 秒，不下沉、不死亡；到时仅水草死亡并释放目标。束缚开始立即停止啃食，期间不执行移动、阵风位移和品种逻辑，但动画与状态计时继续。水草中途被摧毁也只会提前释放目标；门已掉落时仍按普通水草流程拖沉。
- 本类型 `CanBeCharmed=false`。醒着的魅惑菇仍由 `Zombie::EatTarget` 立即 `Die()` 并播放 `SOUND_FLOOP`，但随后 `StartMindControlled()` 被豁免守卫拒绝，僵尸阵营与门/本体血量都不变。
- 2026-08-02 主人指定加固铁门免疫磁力菇，`HasMagneticItem()` 固定 false；普通铁门仍可被
  剥离，加固门的 1030 护盾、耐性、持门手臂和截断能力不因近距离磁力菇消失。
- 持门时 `BlocksFumePiercing=true`。FumeShroom 按 X 由近到远结算，命中它后停止；本击改用非穿透且丢弃破门溢出，门存在时只伤门、本体不掉血。粒子用同一 collider 左沿作为 `clipRightX`，不改 FumeCloud XML 长度。门掉后恢复大喷全额双倍本体伤害。
- 正式波次生成通过 `Board::ResolveWaveZombieType` 计数；每波前两只保留本类型，第三只起返回 `NUM_ZOMBIE_TYPES`。`TrySummonZombie` 在选行和扣预算前直接 `continue`，继续抽取其他候选，禁止回退普通铁门干扰出怪池。计数在新波/生存轮清时归零并进入存档；开发/AutoTest 的 `spawn_zombie` 直造不占配额。
- 生存池配置：`weight=2500, appearWave=6, survivalRound=6`。冒险模式在 2-8 用“普通铁门 + 加固铁门”做独立家族教学（玩家刚取得毁灭菇，能直接观察灰烬抗性），2-9 作为 8 种重点机制综合池必定包含本类型；4-5 在双向射手解锁后的首个可选卡关复习本类型，展示反向两连发背击绕门，4-6 综合池继续保留。

## 验证

- `smoke_reinforced_door_wave_cap.json`：同波第 1/2 只为加固门，第 3 个候选完全跳过且随后普通僵尸正常生成；新波计数清零后可再次生成。
- `smoke_night_spawnlists.json`：逐关验证黑夜出怪节奏；2-8/2-9 均含本类型，2-9 仍为 8 种池。
- `smoke_fog_spawnlists_4_1_to_4_6.json`：4-5/4-6 均含本类型，且 4-5 预览顺序保持矿工为末位教学主题。
- 2026-08-01 上述雾关专项在 `clang-release` 可见运行 85 条命令、exit 0；4-5 选卡预览中的青绿色门体清楚可辨。
- 2026-07-23 当时的 300/920 版本已可见运行 `smoke_reinforced_door.json` 与 `smoke_reinforced_door_potatomine.json` 且均退出 0，覆盖普通攻击扣门 10、灰烬扣门 320、破门、门后灰烬直杀、大嘴花、魅惑菇和小推车；该结果只作为历史证据。
- 2026-07-24 的水草 5 秒束缚扩展按主人要求未新增或运行 AutoTest；`clang-playtest` 与启用 LTO 的 `clang-release` 均零警告编译通过。
- 2026-07-30 已按当前 270/1030 数值静态同步两份原专项，并在 `smoke_cactus.json` 增加持门 1 点、掉门恢复 3 点的断言；按主人要求未编译、未运行 AutoTest。
