---
name: project-pvz-splitpea
description: 经典双向射手的双头独立索敌、后向两连发、复合 Animator 附件、存档与 AutoTest 契约
metadata:
  node_type: memory
  type: project
---

# 双向射手（SplitPea）

2026-07-31 完成经典双向射手。`SplitPea` 继承 `Shooter`，沿用原版 125 阳光、
7.5 秒冷却和 1.5 秒攻击轮询；正面目标触发前头单发，背面目标触发后头两连发，
两侧独立索敌且可同轮同时攻击。前头发射帧为主人确认的全局帧 95，后头为 57，
这两个数已是 `AddFrameEvent` 口径，禁止再次减一。

## 实现契约

- 根动画循环 `anim_idle`；前头使用 `anim_head_idle → anim_shooting`，后头使用
  `anim_splitpea_idle → anim_splitpea_shooting`。两个全尺寸子 reanim 都附着根
  `anim_idle`，以 `SetLocalPosition(-37.6,-48.7)` 补 C# 附件的
  `inverse(basePose)`。
- 前弹复用普通 `BULLET_PEA`；后弹仍复用同一类型，只把基础 X 速度设为
  `-290px/s`。不要为方向新建 BulletType，否则会重复火炬、台风、碰撞和对象池链。
  运行时变成火豆后，100px 溅射区与 38px 命中特效偏移也必须按负速度镜像到左侧。
- 后头第一颗的帧事件置位第二发，下一逻辑步重播同一射击轨。保存
  `rearSecondShotPending/rearSecondShotInBurst`，并用
  `Shooter::SaveHeadAnimatorState/LoadHeadAnimatorState` 保存后头完整
  `PlayTrackOnce` 状态；旧档缺字段时保持待机中性默认，无需提升存档版本。
- `GameDataManager` 注册 `IMAGE_SPLITPEA/ANIM_SPLITPEA/REANIM_SPLITPEA`；
  `AdventureProgression` 既有表在内部关卡 31（4-4）结算时解锁该植物。
  防线推演只记稳定前向输出 `attackDps=13.33`，不把条件性的背向火力恒算进去。

## 验证证据

- `clang-playtest` 全量构建通过。
- `smoke_splitpea.json` 在主人当前桌面可见运行 94 条命令，exit 0；覆盖无目标不空射、
  前向 1 发、后向 2 发、双向 3 发、后头两发之间隔离快照往返、声音请求、数值画像及
  4-4 奖励。5 张同步截图已逐张目检。
- `smoke_splitpea_instance.json` 与 `smoke_splitpea_noinstance.json -NoInstance`
  均可见 exit 0；同一静止帧 `worldBounds` 整数值逐项相同，仅 `renderPath` 不同。
- 共用头部存档逻辑的 `smoke_threepeater.json` 可见 exit 0，5 张当前截图已目检。
  `repro_shooter_save_resume.json` 依赖本地专用 `level1001_data.json` 和
  `-AutoTestLoadSave`；当前构建目录没有该历史问题存档，不能把它的前置断言失败记为
  代码回归。
- `smoke_splitpea_fireball_direction.json` 可见 exit 0：两颗后向火豆令直击目标
  270→190、左侧次要目标 270→244；`FireballImpact` 与直击目标矩形相交，中心横差
  -8px。原 `smoke_torchwood.json` 89 条命令及 `smoke_splitpea.json` 94 条命令同轮
  可见回归通过，全部当前截图已目检。
