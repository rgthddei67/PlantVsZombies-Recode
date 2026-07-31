---
name: project-pvz-starfruit
description: 经典杨桃的五向预测索敌、跨行星弹、无影子表现、对象池、存档与双绘制路径验证
metadata:
  node_type: memory
  type: project
---

# 经典杨桃（Starfruit）

2026-07-31 完成。`StarFruit : Plant` 使用原版 125 阳光、7.5 秒冷却和
1.36～1.5 秒后续射击周期；主人给出的全局第 27 帧是可直接传给
`AddFrameEvent` 的真实帧号。射击动画使用 28fps，并在同一帧发出左、上、下、
右上 30°、右下 30° 五颗 20 伤星弹。原版杨桃不绘制通用植物影子，
`SetupPlant()` 必须移除 `ShadowComponent`。

## 实现契约

- 索敌逐行使用 `ForEachZombieInRow`。同行只认杨桃左侧目标；跨行用
  `Zombie::GetTargetLeadX` 预测星弹飞行时间，再按 C# 的竖直交叉和角度窗口判断，
  不复制原版 800×600 世界绝对坐标；保留 Boss 在杨桃位于后四列时直接开火的兼容分支。
- `BULLET_STAR` 使用基础 `Bullet` 对象池槽位，速度为 333px/s；纵向飞行后按
  Board 首行顶边和当前行高更新 `mRow`，确保跨行碰撞。上下越过
  `0..SCENE_HEIGHT` 时回收，不受台风或火炬树桩影响。
- 星弹纹理每颗随机正反自旋；角度、角速度、纵向速度和动态行随正式快照往返，
  对象池 `Reset()` 必须清零旋转状态。命中触发原版语义的 `StarSplat` 粒子。
- 植物保存射击计时和当前随机间隔；通用 Animator 存档负责恢复
  `anim_shoot → anim_idle` 的一次性播放进行态。
- `GameDataManager` 注册 `IMAGE_STARFRUIT/ANIM_STARFRUIT/REANIM_STARFRUIT`；
  防线推演画像为 `baseHealth=300`、`attackDps=13.33`、`attackRowRadius=4`。
  冒险内部关卡 32 结算后由既有奖励表解锁。

## 验证证据

- `clang-release` 全量与无影子增量构建均 0 warning。
- `smoke_starfruit.json` 在主人当前桌面可见运行，默认实例化与 `-NoInstance`
  均 exit 0、113 条命令全绿；覆盖无目标不发弹、第27帧五向齐发、20伤、Throw、
  随机自旋、跨行命中、`StarSplat`、射击中/飞弹中快照往返和第32关奖励。
- 两条绘制路径均断言 `hasShadow=false`；同一静止画面的 Starfruit
  `worldBounds=(407,309,482,375)` 完全一致，五向分离截图已逐张目检。
