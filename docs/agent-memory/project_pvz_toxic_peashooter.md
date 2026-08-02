---
name: project_pvz_toxic_peashooter
description: 毒囊射手、目标级四层独立计时毒素、状态染色优先级、火炬转换及4-8奖励闭环
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-02
---

# 毒囊射手与目标级叠毒

2026-08-02 完成 `PLANT_TOXICPEASHOOTER`：125 阳光、7.5 秒冷却、300 生命，复用 Shooter 的 1.5 秒攻击节奏和既有全局第 64 帧发射事件。植物追加在 `PlantType` 末尾，`ANIM_TOXICPEASHOOTER` 与 `BULLET_TOXICPEA` 同样只在各自枚举末尾追加；冒险内部关卡 35（显示 4-8）由空奖励改为毒囊射手。

## 毒素与交互

- 毒豆直击 15；命中目标增加一层 6 秒、2 DPS 的独立计时毒素。每只僵尸持有四格计时数组，所有来源共享该目标的四层上限；满层再命中刷新剩余时间最短的一格。
- 毒伤按未减速的游戏 `deltaTime` 累积目标级小数余量，统一以 `TakeProjectileDamage(..., 0.0f)` 结算，因此不会以背击规则绕过二类护盾。行走、啃食、跳跃、减速与冻结的行为早退都不停止毒素。
- 四层剩余时间和小数余量进入 `Zombie::SaveProtectedData/LoadProtectedData`；旧档默认无毒。死亡清理；`StartMindControlled` 与魅惑旧档归一化立即清除毒层，避免友军残留死亡。
- 魅惑、冻结/减速和中毒共用 `UpdateStatusOverlay()`，优先级为魅惑红 > 寒冷蓝 > 中毒紫；任一状态退出后会重新派生最终颜色，不会直接关 overlay 抹掉其他状态。
- 火炬树桩把毒豆转换为普通 40 伤火球并移除附毒身份；不可变 ToxicPea 池槽在回收后恢复 15 伤毒豆、静态紫色表现和转换防重状态。

## 资源与验证

`ToxicPeaShooter.reanim` 复用 PeaShooter 时间线但引用独立换色部件：头、嘴和共享 `ANIM_SPROUT` 派生的头后小叶为紫色，地面叶座与茎为青绿；毒豆为纯紫色并保留自然高光。`scripts/recolor_toxic_peashooter.ps1` 可从权威 PeaShooter 素材重建卡片、11 张轨道图、毒豆和 reanim；不要把地面 `backleaf` 误当成头后小叶。

`clang-release` 构建通过。桌面可见 `smoke_toxic_peashooter.json` 默认实例化与 `-NoInstance` 均退出 0，且各自 49 条状态断言全绿；覆盖资源、实射、植物/子弹存档、四层/第五发刷新、魅惑、寒冷恢复、门板、火炬、对象池和 0.5x/1x/2x 相同游戏时间等伤。`smoke_adventure_progression.json` 断言 4-8 解锁，`smoke_torchwood.json` 保护普通/寒冰豌豆旧转换，`smoke_iceshroom.json` 保护共享 overlay 的冻结/解冻/魅惑旧行为，三者同样可见退出 0。
