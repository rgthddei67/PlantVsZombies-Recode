# 海蘑菇（Sea-shroom）设计

日期：2026-07-29　状态：已获主人批准

## 目标

新增经典植物海蘑菇：只能直接种在空水格，不需要睡莲；不能种在陆地。战斗行为与当前
小喷菇一致，阳光消耗为 0，卡片冷却为 10 秒，主人指定在全局第 33 帧发射孢子。

## 已就位的基础设施

- `PlantType::PLANT_SEASHROOM` 与 AutoTest 植物名称表。
- 3-9 奖励表中的 `PLANT_SEASHROOM`。
- 卡片图、完整 reanim 部件图、`SeaShroom.reanim` 和 `resources.xml` 注册。
- `BULLET_PUFF`、短程索敌与 `SOUND_PUFF`。

## 动画与实现

`SeaShroom.reanim` 为 12fps。包装轨范围为 `anim_idle` 0–24、
`anim_shooting` 25–38、`anim_sleep` 39–63、`anim_idle_aquarium` 64–88；
第 33 帧只会在射击轨中经过。

`SeaShroom : PuffShroom` 复用 1.5 秒攻击间隔、300px 短程索敌、孢子弹和射击计时存档。
`SetupPlant()` 单独注册第 33 帧事件，避免继承小喷菇第 28 帧事件；海蘑菇不绘制陆地阴影。
射击动画速率与小喷菇保持一致。

## 放置、资源与图鉴

- `Board::CanPlantAt` 将海蘑菇与睡莲、缠绕水草归为直接落水植物：仅空水格为真。
- 海蘑菇占普通植物层，不能叠在睡莲或其他植物上。
- `gamedata.json`：`cost=0`、`cooldown=10.0`，初始视觉偏移按原版水面下沉语义设置，
  最终以当前 1100×600 场景截图校准。
- 注册独立 `IMAGE_SEASHROOM`、`ANIM_SEASHROOM` 与 `SeaShroom` reanim，并补图鉴条目。

## 验收

`autotest/scripts/smoke_seashroom.json` 在夜间泳池关卡覆盖：

1. 空水格可直接种、陆地禁种、已有睡莲的水格禁种；
2. 单株海蘑菇为普通层且使用 `anim_idle`，同步截图核对水线和无阴影；
3. 300px 内静止僵尸触发 `anim_shooting` 和 `BULLET_PUFF`；
4. 与睡莲上的小喷菇同行截图，校对脚底、水线、发射点与卡图。
