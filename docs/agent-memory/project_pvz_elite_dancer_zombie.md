---
name: project_pvz_elite_dancer_zombie
description: 黑夜强台风专属精英舞王——紫衣资源、持续八伴舞、无视植物、超强台风强化与吞车机制
metadata:
  node_type: memory
  type: project
  originSessionId: 019f82d6-5401-7d41-a693-b7db195f6b84
---

# 精英舞王僵尸（2026-07-21）

`ZOMBIE_ELITE_DANCER` 是黑夜台风天气条件变异：正式波次选中普通舞王后，在大雨且任意非 `NONE` 台风中按 50% 变异；同一波至多成功两次。`Board::ResolveRainMutationType` 是正式刷怪与 AutoTest 共用的唯一解析入口；达到上限后的成功变异返回 `NUM_ZOMBIE_TYPES`，由 `TrySummonZombie` 直接继续挑选，不再退回普通舞王占用本次刷新。未命中 50% 的候选仍生成普通舞王。当前波计数进入 `GameInfoSaver`，新波清零，天气与台风切换不清零；旧版单台风布尔字段按已生成一只迁移。

2026-07-27 图鉴接入永久实际遭遇：解析后的实际类型由 `Board::CreateResolvedWaveZombie` 创建成功时才调用 `GameAPP::RecordEliteDancerEncounter()`，首次置位立即把 `encounteredEliteDancer` 写入 `PlayerInfo.json`，旧档默认未遇见。通用直造、预览、读档与被上限丢弃的候选都不解锁；`ZombieAlmanacScene` 在通关 spawnlist 并集之外合并该标记。

精英类继承 `DancerZombie` 并调用父类 `SetupZombie()`，复用 Jackson 的 Die@146、EatTarget@90/99、断头/断臂和月球漫步轨道，不新增帧事件。当前数值与行为：本体 720、基础移动倍率 1.25；无视植物碰撞并持续推进；每 0.2 游戏秒补一只普通 `BackupDancerZombie`，最多维持 36 只。冻结暂停召唤，减速的 scaled delta 自然拖慢计时；魅惑时放弃旧阵营关联，新伴舞继承领队阵营。

天气能力通过 `Zombie::GetAbilityAnimSpeedMultiplier()` 接入统一 extra 速度链：普通台风保持基础 1.25，强台风为 `1.25×1.45`，超强台风为 `1.25×1.75`；再与雨势、减速或冻结层统一组合。`Board` 在台风开始、恢复、衰减和停止时刷新既有僵尸速度。

小推车碰撞先正常触发当前车，再查询 `Zombie::ConsumesOtherMowersOnContact()`。精英命中后，当前行 mower 走原 `Trigger()`、音效和 `INT32_MAX` 秒杀链；`Board::RemoveOtherMowersWithoutTrigger(currentID)` 复制 mower ID，只对其他行记 loseMower 并直接 `Die()`，不触发它们。这样精英不能借吞车能力直接进家，同时仍让其余行永久失车。

紫衣不使用 overlay。`scripts/recolor_elite_dancer.ps1` 以 System.Drawing 仅筛选 Jackson 11 张衣物 PNG 中的红色布料，固定转到 282° 紫色相并保留原亮度、alpha、白手套、皮肤、鞋和描边；两个 preset 生成一致哈希。派生 reanim 只替换 11 个衣物 image key，头、发、手、脚与全部 track 名继续复用原 Jackson 契约。

2026-07-27 当前刷新验证：`clang-playtest` 与 `clang-release` 零警告构建；`smoke_elite_dancer_mutation` 已按源码同步为 50/51 边界与每波两只上限，桌面可见运行覆盖两个成功变异、第三个成功候选完全跳过、上限后未命中仍生成普通舞王、天气切换不重置与新波复位，退出 0。`smoke_elite_dancer_almanac_unlock` 可见验证直造不解锁、正式变异解锁与图鉴显示，退出 0。其他脚本中的旧 800/0.4/8/1.70 数值断言属于此前平衡快照，运行前须同步当前数值。

关联：[舞王与伴舞](project_pvz_dancer_zombie.md)、[黑夜雨势与台风](project_pvz_night_rain_weather.md)、设计与调参表 `docs/superpowers/specs/2026-07-21-elite-dancer-zombie-design.md`。
