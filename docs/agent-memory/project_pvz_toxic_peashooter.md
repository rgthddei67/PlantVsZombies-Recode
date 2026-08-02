---
name: project_pvz_toxic_peashooter
description: 毒囊射手、目标级五层独立计时毒素、状态染色优先级、火炬转换及4-8奖励闭环
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-02
---

# 毒囊射手与目标级叠毒

2026-08-02 完成 `PLANT_TOXICPEASHOOTER`：125 阳光、7.5 秒冷却、300 生命，复用 Shooter 的 1.5 秒攻击节奏和既有全局第 64 帧发射事件。植物追加在 `PlantType` 末尾，`ANIM_TOXICPEASHOOTER` 与 `BULLET_TOXICPEA` 同样只在各自枚举末尾追加；冒险内部关卡 35（显示 4-8）由空奖励改为毒囊射手。

## 毒素与交互

- 毒豆直击 15；命中目标增加一层 6 秒、每 0.4 秒 1 伤（2.5 DPS）的独立计时毒素。每只僵尸持有五格计时数组，所有来源共享该目标的五层上限；满层再命中刷新剩余时间最短的一格。
- 毒伤按未减速的游戏 `deltaTime` 累积目标级小数余量，统一以 `TakeProjectileDamage(..., 0.0f)` 结算，因此不会以背击规则绕过二类护盾。行走、啃食、跳跃、减速与冻结的行为早退都不停止毒素。
- 五层剩余时间和小数余量进入 `Zombie::SaveProtectedData/LoadProtectedData`；旧档默认无毒，旧版四层数组自然补一个空槽。死亡清理；`StartMindControlled` 与魅惑旧档归一化立即清除毒层，避免友军残留死亡。
- 魅惑、冻结/减速和中毒共用 `UpdateStatusOverlay()`，优先级为魅惑红 > 寒冷蓝 > 中毒紫；任一状态退出后会重新派生最终颜色，不会直接关 overlay 抹掉其他状态。
- 火炬树桩把毒豆转换为独立 `BULLET_TOXICFIREBALL` 紫焰 40 伤火豆：解冻、抗火与溅射伤害规则沿用普通火豆，但范围由普通火豆的 100 像素缩窄为沿飞行方向 30 像素；直击和实际受到溅射的目标均增加一层毒素。不可变 ToxicPea 池槽负责回收归属；读档保持紫焰类型与附毒语义，回收后恢复 15 伤毒豆、静态紫色表现和转换防重状态。

## 资源与验证

`ToxicPeaShooter.reanim` 复用 PeaShooter 时间线但引用独立换色部件：头、嘴和共享 `ANIM_SPROUT` 派生的头后小叶为紫色，地面叶座与茎为青绿；毒豆为纯紫色并保留自然高光。`scripts/recolor_toxic_peashooter.ps1` 可从权威 PeaShooter 素材重建卡片、11 张轨道图、毒豆和 reanim；不要把地面 `backleaf` 误当成头后小叶。

初版四层/2 DPS 实现曾通过 `clang-release` 与桌面可见专项回归。2026-08-02 根据主人实测把上限调为五层、单层调为每 0.4 秒 1 伤；随后新增独立紫焰毒火豆并重新完成 `clang-release` 构建。桌面可见 `smoke_toxic_peashooter.json` 默认实例化与 `-NoInstance` 均退出 0，最终脚本 55 条状态断言全绿，覆盖五层/第六发刷新、五层盾伤、存档、魅惑、倍速、独立紫焰类型、紫焰存读档、直击附毒和 ToxicPea 池槽复位；截图确认两条渲染路径的紫焰表现一致。紫焰火豆后续收窄为 30 像素溅射并让实际受到溅射的目标同样叠毒，最终回归证据见本主题后续记录。

2026-08-02 紫焰溅射附毒调整完成后，`clang-release` 构建通过；桌面可见 `smoke_toxic_peashooter.json` 退出 0、59 条状态断言全绿。专项在同排布置直击目标、30 像素内次要目标和范围外目标，逐只断言仅前两只中毒、范围外目标保持 270 满血，且三者总生命符合 40 点直击、13 点单体溅射和两份毒伤；`d1_toxic_fireball_30px_splash_and_pool_reset.png` 同时确认前两只呈紫色、第三只保持正常颜色。对象池复位断言继续通过。
