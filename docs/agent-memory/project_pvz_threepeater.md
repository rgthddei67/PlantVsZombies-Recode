---
name: project-pvz-threepeater
description: "2026-07-23 三线射手：三头基准姿态附件、29/73/111 发射帧、斜向豌豆、顶底行差速补偿与存档"
metadata:
  node_type: memory
  type: project
---

# 三线射手（ThreePeater）

2026-07-23 新增 `PLANT_THREEPEATER`：325 阳光、7.5 秒卡片冷却、1.5 秒基础攻击间隔。核心索敌与斜向弹道参考 C# 版本；只要本行或相邻有效行存在前方敌对有头僵尸，就启动完整一轮。

## 三头动画与附件

三个头是同一份 `ThreePeater.reanim` 的独立子 Animator：

- 下头 `anim_shooting1`：主人确认的 `AddFrameEvent` 真实帧 29，通常射向下一行。
- 上头 `anim_shooting3`：真实帧 73，通常射向上一行。
- 中头 `anim_shooting2`：真实帧 111，射向本行。

C# `AttachToAnotherReanimation` 使用“父轨道当前姿态 × 基准姿态逆矩阵”，本项目 `AttachAnimator` 只直接应用父轨道当前姿态。不能给三个头套统一植物视觉偏移；必须按根 `anim_idle` 首帧分别抵消附件轨基准锚点：

- `anim_head1`: `(53.0, 50.8)`
- `anim_head2`: `(35.4, 35.0)`
- `anim_head3`: `(21.0, 22.2)`

这些基准姿态的旋转/缩放为单位，所以子 Animator 分别使用 `SetLocalPosition(-baseX, -baseY)`。中头曾因漏掉这层 `inverse(basePose)` 与茎错位；修正后可见待机、三头射击截图均已校对。

## 弹道与边路补偿

普通三行各发一颗豌豆；相邻行豌豆以目标行为碰撞行，从本株统一枪口出发，初始纵向速度为 `±300px/s`，再按每 10ms 乘 `0.97` 衰减，使用指数折算保持固定步长变化时轨迹一致。

主人要求加入非原版边路补偿：顶行缺失的上路弹、底行缺失的下路弹都改发到本行，因此每轮始终三发。本行正常弹保持 `290px/s`，补偿弹为 `360px/s`，同一路两颗豌豆会形成明显间距；伤害都仍为 20。

`Bullet` 保存 `threepeaterMotion`，读档后重建斜向阴影布局并继续按已保存的纵向速度衰减。三线射手另外保存第二、第三个头的完整一次性播放状态；继承的第一个头和攻击计时器沿用 `Shooter` 存档。

## 验证证据

`smoke_threepeater.json -Seed 42` 在当前桌面可见运行退出码 0，窗口标题为“植物大战僵尸中文版”，`run.log` 记录 58 条命令全部完成：

- 中间行：三颗弹分布为 3/2/1 行，斜向标记为 true/false/true，并命中唯一位于相邻行的僵尸。
- 顶行：三颗弹为 `row 1@290 / row 0@290 / row 0@360`。
- 底行：三颗弹为 `row 4@360 / row 4@290 / row 3@290`。
- 顶/底补偿截图均能看出本行两颗弹的间距；三个头在边界也都进入射击轨。

`clang-playtest` 与 `clang-release`（LTO）均编译通过。设计文档为 `docs/superpowers/specs/2026-07-23-threepeater-design.md`。
