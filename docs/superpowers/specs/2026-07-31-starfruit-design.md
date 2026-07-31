# 杨桃（Starfruit）设计

日期：2026-07-31　状态：主人已指定发射帧 27，并要求参照 C# 原版实现

## 目标

新增经典植物杨桃。保持 C# `Plant.cs::LaunchStarFruit`、`FindStarFruitTarget`、
`StarFruitFire` 与 `Projectile.cs` 的五向索敌、发射、跨行运动、伤害和命中反馈语义，
并接入本项目资源注册、对象池、存档、轻量防线推演和 AutoTest。

## 已就位的基础设施

- `PlantType::PLANT_STARFRUIT`、冒险 4-5 奖励表和 TestDriver 植物名称表。
- `BulletType::BULLET_STAR`、`IMAGE_PROJECTILE_STAR`。
- 卡图、`Starfruit.reanim`、星弹图片和全部 reanim 部件图；`resources.xml` 已由主人加入对应资源。
- `throw.ogg` / `throw2.ogg` 已注册，复用原版 `FoleyType.Throw`，无需新增声音。

## 动画

- `Starfruit.reanim` 基础 12fps：`anim_idle` 活跃帧 0–16，`anim_shoot` 活跃帧 17–33。
- 主人给出的真实 `AddFrameEvent` 发射帧为 **27**，代码直接使用、不再减 1；待机轨不会扫到该帧。
- 待机使用原版随机 10–15fps；射击使用原版 28fps，并按生存攻速和雨势行动倍率同步加速。
- `PlayTrackOnce("anim_shoot", "anim_idle", ...)` 承载射击到待机回切，进入与返回混合均为 0.2 秒。

## 索敌与射击

- `StarFruit : Plant`，不使用豌豆系独立头部 `Shooter`。
- 原版射击周期为 150 厘秒；首轮随机 0–1.5 秒，后续每轮 1.36–1.5 秒。每轮到期先重置周期，
  无目标也等待下一轮；计时乘 `GetAttackSpeedMultiplier()`。
- 通过 `ForEachZombieInRow` 逐行使用行桶，不扫描全实体表。排除魅惑、无头和当前高度层不可索敌目标。
- 从当前格子中心派生原版语义锚点：索敌原点 `(0,-10)`，发射点 `(-15,-25)`；不复制 C# 世界绝对坐标。
- 同行只把杨桃左侧僵尸视为目标；其他行按 333px/s 飞行时间调用 `Zombie::GetTargetLeadX`，
  再复刻 C# 的竖直交叉与 30° 上下斜线角度窗口。被啃食时保持开火资格；预留原版 Boss
  在杨桃位于后四列时直接触发射击的兼容分支。
- 第 27 帧一次创建 5 颗 20 伤害星弹并播放一次 Throw：左、上、下、右上 30°、右下 30°。

## 星弹

- `BulletPool` 为 `BULLET_STAR` 分配基础 `Bullet` 槽位；`Reset()` 必须恢复纹理、伤害、速度、旋转和行状态。
- 星弹使用 333px/s 与 `cos/sin(30°)` 分解速度；纵向移动时按 Board 当前行高和首行顶边动态更新 `mRow`，
  使碰撞与排序跟随所在行，泳池 85px 行高同样正确。
- Y 超出 `0..SCENE_HEIGHT` 或 X 离场时回收。纹理按原版持续随机正/反向旋转；角度和角速度随正式存档往返。
- 星弹不受台风修正，不被火炬树桩转换；命中走普通 20 点植物伤害与现有护甲撞击声。
- 命中触发 `StarSplat`。从原版 `StarSplat.xml` 和现有素材移植，厘秒换算为秒；补入
  `Star_splats.png` / `Star_particles.png`，配置只放权威 `build/clang-release/resources/`。

## 数值与图鉴

- `cost=125`、`cooldown=7.5s`、`health=300`。
- 原版杨桃不绘制通用植物影子；实例创建时移除 `ShadowComponent`，并由状态断言和截图共同验收。
- C# 将杨桃本体下移 10px；以当前通用植物 offset 为基准，初值 `[-37.6,-34.0]`，最终由可见截图校对。
- 轻量推演：`attackDps=13.33`、`attackRowRadius=4`，表示单目标稳定伤害与全场行覆盖的保守近似。
- `info.txt` 增加中文名称与原版语义图鉴说明。

## 存档

- 植物 `SaveExtraData/LoadExtraData` 保存当前射击计时和下一轮间隔；根 Animator 由通用存档恢复。
- 子弹通用存档增加旋转角和角速度，旧档缺字段时以中性值恢复；星弹类型、速度、动态行和对象池类型沿现有字段恢复。

## 验收

`autotest/scripts/smoke_starfruit.json` 使用 `clang-release` 可见运行，至少覆盖：

1. 杨桃与向日葵站位、杨桃无影子、卡图、图鉴资源加载，默认实例路径截图；另以 `-NoInstance` 跑同一静态画面。
2. 无目标不发弹；左侧同行、竖直或斜线目标能触发 `anim_shoot`，第 27 帧后恰有五颗星弹。
3. 五个速度向量、动态跨行、20 点伤害、星形命中特效和 Throw 请求次数。
4. 射击动画中和飞行星弹的 `save_level_snapshot` / `reload_level_snapshot` 往返，包括对象池类型、速度与旋转。
5. `plantDefinitions.PLANT_STARFRUIT` 的阳光、冷却和 simulation 投影；冒险 4-5 奖励保持可解锁。
