---
name: project_pvz_torchwood_firepea
description: 经典火炬树桩、运行时子弹换型、FirePea 全时间轴动画子弹、火焰伤害与对象池/存档契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-02
---

# 火炬树桩与动画火豌豆

2026-07-27 完成。`PLANT_TORCHWOOD` 对齐 C# 原版：175 阳光、7.5 秒冷却、300 生命；同排普通豌豆穿过树桩中心右侧 30px 点火为 40 伤害火豌豆，寒冰豌豆先融化为 20 伤害普通豌豆。`mHitTorchwoodColumn` 防止同一列重复处理，但融化后的豌豆仍可在更靠右的树桩重新点燃。

## 火焰伤害

- 非抗火目标受到 40 点直接伤害并解除寒冷；沿火豆当前飞行方向的同行 100px 范围内，
  其他敌对僵尸受到按 C# 公式封顶的约 13 点溅射伤害。`mVelocityX<0` 时范围镜像到
  命中点左侧，`mVelocityX>=0` 保持历史右侧口径；溅射会穿透二类护盾，但不会替次要
  目标解冻。
- 冰车、鎏金冰车，以及从正面命中仍持有铁门或梯子的僵尸按门板/机械抗火口径处理：只吃普通 40 点直接伤害，不触发范围溅射、解冻和火焰命中特效。反向火豆从背后命中持盾目标时绕过门板，按本体火焰命中处理；门保持原血量。
- 火豌豆飞行音效使用 `firepea.ogg`，命中火焰音效使用 `ignite.ogg`；命中特效复用 `ANIM_JALAPENO_FIRE` 的完整时间轴单次播放，不依赖帧事件。

## 动画子弹与对象池

- `FirePea.reanim` 是完整时间轴循环子弹，不是静态贴图；由 `Bullet::ConfigurePresentation` 按当前 `mBulletType` 动态挂接/移除 Animator，随机使用 50～80fps，并按飞行方向切换偏移和翻转。
- `mPoolType` 是对象池槽位的不可变分配类型，`mBulletType` 是运行时表现/伤害类型。豌豆槽位可暂时变成火豌豆，回收后必须恢复为原始豌豆，否则会污染后续复用。
- Animator 子弹必须同时覆盖串行更新与 `UpdateParallel` 的动画推进握手，避免主线程重复推进；存档恢复时保留对象池所有权，不能把池内子弹改成非池对象。
- 火豌豆阴影复用普通豌豆布局并放大到 1.4x；运行时换型后立即重算阴影。
- `FireballImpact` 是 `AnimatedObject(ObjectType::OBJECT_PARTICLE)` 播放的 reanim，
  不是 ParticleSystem XML。其局部 X 偏移绝对值为 38px，按 `mVelocityX` 符号镜像，
  因此反向命中不再把火焰画到目标右侧。

## 验证

可见 `smoke_torchwood.json` 覆盖普通豌豆点火、寒冰豌豆融化后在下一树桩重新点火、对象池回收复用、直接/溅射伤害与解冻差异、持门抗火、1-24 奖励解锁和图鉴文案。`clang-playtest` 构建、可见 AutoTest、状态导出与截图均通过。当前项目尚无雾系统，因此原版火炬树桩驱雾能力暂无接入点。

2026-07-31 新增 `smoke_splitpea_fireball_direction.json`：真实双向射手后头的两颗反向豌豆
穿过身后火炬后，直击目标共受 80 点、其左侧次要目标共受 26 点；同步截图后
`FireballImpact` 相对直击目标中心 X 为 -8px 且矩形相交。修复前同一取证为 +68px，
并且溅射固定落在命中点右侧。

2026-08-02 毒豆纳入同一转换链：`BULLET_TOXICPEA` 穿过火炬后变为独立 `BULLET_TOXICFIREBALL`，享受普通火豆的 40 伤、溅射、解冻和抗火语义，并使用紫焰飞行/命中特效、只让直击目标附毒；溅射目标不附毒。当前类型明确入档，不可变 `mPoolType` 仍保留 ToxicPea 回收归属，复用后恢复 15 伤纯紫毒豆。

最终 `clang-release` 构建通过；桌面可见 `smoke_torchwood.json` 退出 0、25 条状态断言全绿，确认普通豌豆仍转换为 `BULLET_FIREBALL` 且紫焰计数为 0。毒囊专项默认与 `-NoInstance` 均退出 0，独立紫焰类型、存读档、直击/溅射分流和对象池复位通过。
