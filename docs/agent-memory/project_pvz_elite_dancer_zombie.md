---
name: project_pvz_elite_dancer_zombie
description: 黑夜强台风专属精英舞王——紫衣资源、持续八伴舞、无视植物、超强台风强化与吞车机制
metadata:
  node_type: memory
  type: project
  originSessionId: 019f82d6-5401-7d41-a693-b7db195f6b84
---

# 精英舞王僵尸（2026-07-21）

`ZOMBIE_ELITE_DANCER` 是黑夜强天气条件变异：正式波次选中普通舞王后，仅在大雨且台风强度为 `SEVERE` / `SUPER` 时按 25% 变异；同一波至多成功两次。`Board::ResolveRainMutationType` 是正式刷怪与 AutoTest 共用的唯一解析入口，失败不消费波次上限，类型创建后不会因天气减弱而回退。当前波成功生成数量进入 `GameInfoSaver`，`SummonNextWave()` 进入新波时清零，天气与台风切换不清零；旧版单台风布尔字段按已生成一只迁移。新枚举追加在全部既有已实现僵尸之后、`NUM_ZOMBIE_TYPES` 之前，不能插入旧类型中间破坏存档数值 ID。

精英类继承 `DancerZombie` 并调用父类 `SetupZombie()`，复用 Jackson 的 Die@146、EatTarget@90/99、断头/断臂和月球漫步轨道，不新增帧事件。数值与行为：本体 800；无视植物碰撞、始终调用 `Zombie::ZombieMove` 向终点推进；每 0.4 游戏秒补一只普通 `BackupDancerZombie`，最多维持 8 只。`kMaxActiveBackupDancers` 与 `kBackupSummonInterval` 集中在 `EliteDancerZombie.cpp` 匿名命名空间；冻结暂停召唤，既有减速的 scaled delta 会自然拖慢计时，雨势与精英动画倍率不重复加速召唤。魅惑时放弃旧阵营关联，新伴舞继承领队阵营；ID、计时、编队游标与魅惑边沿状态均入派生类存档。

超强台风额外 1.70 倍通过 `Zombie::GetAbilityAnimSpeedMultiplier()` 接入统一 extra 速度链；`Board` 在台风开始、恢复、衰减和停止时调用 `RefreshZombieWeatherSpeeds()`，所以既有精英会立即在 1.70 / 1.00 间切换。大雨 1.40 下状态字段 `animExtraSpeedPct` 为 238；Jackson 的 1.2 是 clip 基速，不应重复计入 extra 层断言。

小推车碰撞先正常触发当前车，再查询 `Zombie::ConsumesOtherMowersOnContact()`。精英命中后，当前行 mower 走原 `Trigger()`、音效和 `INT32_MAX` 秒杀链；`Board::RemoveOtherMowersWithoutTrigger(currentID)` 复制 mower ID，只对其他行记 loseMower 并直接 `Die()`，不触发它们。这样精英不能借吞车能力直接进家，同时仍让其余行永久失车。

紫衣不使用 overlay。`scripts/recolor_elite_dancer.ps1` 以 System.Drawing 仅筛选 Jackson 11 张衣物 PNG 中的红色布料，固定转到 282° 紫色相并保留原亮度、alpha、白手套、皮肤、鞋和描边；两个 preset 生成一致哈希。派生 reanim 只替换 11 个衣物 image key，头、发、手、脚与全部 track 名继续复用原 Jackson 契约。

验证：`clang-release` 全量构建零警告。当前桌面可见 `smoke_elite_dancer` 验证 800 HP、0.4 秒补充、8 上限、植物无视、SUPER 1.70、当前行 mower 独存并进入 `MOVING`、精英血量归零、其他四辆静默消失且精英随后正常退场；`smoke_elite_dancer_mutation` 覆盖普通台风不变异、25/26 边界、每波两只上限、天气切换不重置与新波复位；`smoke_elite_dancer_summon_steps` 按 0.5 秒安全采样确认伴舞数 1→2→3；`smoke_elite_dancer_lifecycle` 覆盖非爆炸死亡、魅惑后 8 同阵营伴舞、冻结时 0 召唤及减速后约 0.8 秒才补出第一只；既有 `smoke_dancer_summon` 回归通过。视觉截图确认紫衣保留阴影和高光。

关联：[舞王与伴舞](project_pvz_dancer_zombie.md)、[黑夜雨势与台风](project_pvz_night_rain_weather.md)、设计与调参表 `docs/superpowers/specs/2026-07-21-elite-dancer-zombie-design.md`。
