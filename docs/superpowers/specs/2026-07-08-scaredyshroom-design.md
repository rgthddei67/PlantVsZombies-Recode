# 胆小菇 (Scaredy-shroom) 设计

日期：2026-07-08　状态：已获主人批准（方案 A）

## 目标

新增经典植物胆小菇：夜间廉价远程输出，僵尸靠近时吓得缩进土里停火，僵尸走开后重新升起。行为与数值忠实还原原版（参照 C# Lawn 实现 `Plant.cs::UpdateScaredyShroom`）。

## 已就位的基础设施（无需改动）

- `PlantType::PLANT_SCAREDYSHROOM`（PlantType.h）
- TestDriver 植物名表已含该项
- `ResourceKeys::IMAGE_SCAREDYSHROOM`、`AnimationType::ANIM_SCAREDYSHROOM`
- 卡片贴图 `PlantImage/ScaredyShroom.png`、reanim 全套贴图与 `ScaredyShroom.reanim`
- `BULLET_PUFF` 孢子弹与命中特效

## 动画 track（ScaredyShroom.reanim，12fps）

`anim_sleep`（白天睡）/ `anim_grow`（升起）/ `anim_scaredidle`（缩头循环）/ `anim_scared`（缩下去）/ `anim_shooting`（活跃区间全局 16–30 帧）/ `anim_idle`

## 方案

**A（采用）**：`ScaredyShroom : Shroom`，仿 `PuffShroom` 结构，`PlantUpdate()` 内四态状态机。
B（弃）：通用"可躲藏蘑菇"基类 — 仅此一个此类植物，YAGNI。
C（弃）：挂 `Shooter` — 其头部独立 Animator 模型是豌豆系专用。

## 状态机（镜像 C#）

```
READY ──害怕圈内有僵尸──▶ LOWERING(anim_scared 播一次)
  ▲                            │播完
  │anim_grow 播完               ▼
RAISING ◀──圈内无僵尸── SCARED(anim_scaredidle 循环)
```

- **害怕判定**：以植物为圆心、半径 120px 的圆与僵尸矩形相交；只查本行 ±1 行（`ForEachZombieInRow` 行桶 ×3，不做全表扫描）；魅惑、濒死/已死僵尸不吓。含身后僵尸（原版细节）。
- **射击**：仅 READY 态。`mShootTimer` 累计（乘词条攻速倍率）→ 本行有僵尸（**全行射程**、`HasHead()`、非魅惑）→ `PlayTrackOnce("anim_shooting", "anim_idle", …)`；**第 25 帧**帧事件（主人指定，已看动画预览）发射 `BULLET_PUFF`。
- **守卫**：射击动画播放中不进入害怕流程（C# `mShootingCounter` 守卫的等价物）；非 READY 态持续重置射击计时器。

## 数值（gamedata.json，两个 preset 的 resources 都要加）

`"PLANT_SCAREDYSHROOM": { cost: 25, cooldown: 7.5, offset/scale 参照小喷菇，截图后微调 }`

## 注册

`GameDataManager::InitializeHardcodedData` 加 `RegisterPlant(PLANT_SCAREDYSHROOM, "PLANT_SCAREDYSHROOM", IMAGE_SCAREDYSHROOM, ANIM_SCAREDYSHROOM, "ScaredyShroom", &MakePlant<ScaredyShroom>)`。卡片由注册表驱动，无需单独加卡。

## 存档

`SaveExtraData/LoadExtraData`：`state` + `shootTimer`；动画轨道帧由现有 Animator 状态存取机制承载，读档后按 state 校正当前轨道。

## 睡眠

`Shroom::SetupPlant()` 已处理白天 `anim_sleep`，直接继承。

## 验收（AutoTest）

`autotest/scripts/smoke_scaredyshroom.json`，夜间关（goto 10-19）、`-Seed` 固定：
1. 种胆小菇，远处刷僵尸 → 截图 + dump 验证产生孢子弹；
2. 近身刷僵尸 → 截图验证缩头，dump 验证停火；
3. 僵尸死后 → 截图验证升起并恢复射击。
