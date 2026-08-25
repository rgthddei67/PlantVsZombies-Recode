---
name: project-pvz-melonpult
description: 经典西瓜投手、三行溅射、护盾穿透、卡图与双绘制路径验证
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 经典西瓜投手（Melon-pult）

2026-08-09 完成；2026-08-14 按当前源码重新核实平衡值。`MelonPult : Plant` 现使用
325 阳光、10 秒卡冷却、300 生命；
首次攻击相位随机 0～3 秒，后续周期 2.86～3.0 秒。`anim_shooting` 按 35fps
播放，主人确认全局第 44 帧直接作为发射事件；回调另核对当前轨道。

## 实现契约

- 弹心从植物稳定视觉锚点加 `(-1,-79)` 发射，复用卷心菜/玉米已验证的
  1.2 秒、210px 解析抛物线、目标提前量、屋顶坡面 Y 补偿、下降末段碰撞、
  在途存档与对象池。`BULLET_MELON` 开启原版 -72° 初始旋转和卷心菜自旋范围。
- 直击伤害 120；以弹丸 `mRow±1` 查行桶，候选 collider 与弹心 60px 水平窗口相交时
  结算 40 次要伤害。先快照稳定僵尸 ID 再扣血，避免同帧死亡/回收破坏遍历；
  极端密度下保留“次要总伤害最多为直击七倍”预算。
- 西瓜走 `penetrateShield=true` 的原版 splash 语义：二类护盾与后方本体同时承受完整
  伤害；不是卷心菜的“绕过不伤盾”。头盔仍走普通一类防具逻辑。
- 命中与落空都随机播放 `melonimpact/melonimpact2`，并发射 `MelonSplash`。粒子图是
  9 帧水平条带，按 `PARTICLE_MELONPULT_PARTICLES` 在 `ParticleTextures` 注册。
- 卡片内立绘仅在 `Card::DrawPlantImage` 使用 0.80 独立倍率，最终相对通用布局偏移
  `(+3,+1)` UI px；不改 gamedata 的战场缩放/偏移。西瓜弹丸地面阴影单独向右 6px。
- 用户放入的 `Melonpult.reanim` 和六张部件图全部通过 `resources.xml` 与精确
  `IMAGE_REANIM_MELONPULT_*` 键注册；卡图、子弹、粒子、音效与 reanim 由 AutoTest 闭环断言。

## 验证证据

- 2026-08-14 `clang-release` 完整配置、编译与 LTO 链接完成，Win7 x64 378 项导入审计通过；
  最终提升环境构建无 applocal 噪声。
- `smoke_melonpult.json` 共 139 条命令，从 `build/clang-release` 在主人当前桌面可见运行，
  退出码 0、`status.json=passed`，`run.log` 无 FAIL/ERROR/WARN/missing；全部 7 张截图逐张检查。
- 定量断言包括：44 帧前 0 弹/后 1 弹，四行目标生命 `270/230/150/230`，
  铁门盾/本体 `1100→980` 与 `270→150`，在途存档，落空粒子/音效，池槽复用，
  花盆屋顶命中，以及完成内部关卡 43（显示 5-7）后解锁 `PLANT_MELONPULT`。
- 同步检查卡图缩放/偏移、手持西瓜、飞行西瓜与右移阴影、三行碎片云、
  铁门命中和屋顶弧线截图；`a0` 的 `renderPath=NO_INSTANCE` 且绘制探针 ready。

## 复用要点

Winter Melon 已按本弹丸的跨行集合、解析轨迹、对象池与 splash 护盾语义实现；
其 100/33 伤害、10 秒群体减速和独立冰瓜视觉资源见 `project_pvz_winter_melon.md`。

## 2026-08-25 冰墙优先级

西瓜家族在同行有冰墙时即使无僵尸也起手，并在发射帧优先锁定墙体；普通西瓜对墙结算 120
直击并播放 `MelonSplash`/impact 反馈，但不向墙后或相邻行僵尸派发 40 溅射。显式锁墙标志使
在途弹跳过僵尸并随快照往返；墙消失时按原落点落空。当前西瓜父专项与共享锁墙专项均在
`clang-release` 可见路径通过。
