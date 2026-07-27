---
name: project_pvz_zombie_ability_anim_speed
description: 2026-07-27 僵尸自身整体动画倍率统一到 GetAbilityAnimSpeedMultiplier，删除 Zombie::mExtraSpeed，并保留快速铁桶旧档迁移
metadata:
  node_type: memory
  type: project
---

# 僵尸自身整体动画能力倍率

2026-07-27 将遗留的 `Zombie::mExtraSpeed` 删除。僵尸自身对整体动画及 `_ground` 位移提供的倍率统一由
`GetAbilityAnimSpeedMultiplier()` 返回；`Zombie::UpdateAnimSpeed()` 只负责组合能力、减速、雨势与黄色冰道，
冻结仍最终优先为 0。子类不得直接写 `Animator::SetExtraSpeedMultiplier()`。

当前迁移结果：

- `FootballZombie` / `PinkFootballZombie` 分别返回固定 `1.8 / 1.95`。
- `PaperZombie` 按持报纸状态返回 `1.0 / 1.4`；`FastPaperZombie` 在该结果外乘 `1.5`，因此狂暴后为 `2.1`。
- `FastBucketZombie` 的 `1.55～1.60` 仍为每实例随机值，但状态下沉到派生类字段并进入 `extraData`。
- 精英舞王、精英撑杆和鎏金冰车继续使用同一虚函数，无第二套基础倍率。

存档契约：新档不再写僵尸根字段 `extraSpeed`。`Zombie::LoadProtectedData()` 仅保留只读兼容钩子，
让旧档中的快速铁桶把根字段迁移到派生状态；固定倍率与读报阶段均可由类型和既有状态重建。
`PaperZombie::LoadExtraData()` 在恢复 `mHasNewspaper` 后必须重新调用 `UpdateAnimSpeed()`。

验证：`clang-playtest` 配置与编译零警告；当前桌面可见 `smoke_zombie_ability_speed` 通过并验证
`180/195/140/210/155` 在快照读档前后一致、快速铁桶真实随机值持久化且根字段已消失；
可见 `smoke_night_rain` 与 `smoke_gilded_zamboni` 回归通过。既有 `smoke_weather_pressure`
在与本次无关的第 20 波压力断言处仍期望 86、当前源码实际为 97，其此前能力/雨势/冻结速度断言全部通过。
