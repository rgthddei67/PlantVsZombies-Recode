---
name: project-pvz-cabbagepult
description: 经典卷心菜投手的解析抛物线预判、末段碰撞、对象池、存档、屋顶与双绘制路径验证
metadata:
  node_type: memory
  type: project
---

# 经典卷心菜投手（Cabbage-pult）

2026-08-03 完成。`CabbagePult : Plant` 使用经典 100 阳光、7.5 秒卡冷却、
300 生命和 40 点卷心菜直击伤害；首轮攻击相位在 0～3.0 秒均匀分布，
后续周期为 2.86～3.0 秒。`anim_shooting` 按 C# 的 35fps 播放，主人确认的
全局第 43 帧直接作为 `AddFrameEvent` 发射帧，声音为 Throw/Throw2。

## 实现契约

- 同行索敌只遍历 `ForEachZombieInRow`，选择最近的合法地面目标；发射时用
  `Zombie::GetTargetLeadX(1.2f)` 预测目标水平落点，Y 取目标碰撞箱上部身体。
  不移植 C# 针对海豚、跳跳、潜水等类型的固定水平落点修正，也不保留 Boss 的独立 Y 特判。
- `Bullet` 的通用解析抛射状态以
  `lerp(start,target,p) + (0,-4*apex*p*(1-p))` 推进，卷心菜使用 1.2 秒飞行和
  210px 拱高。高弧段关闭碰撞器，下降且离基准轨迹不超过 35px 时开启；到达
  落点后保留 0.08 秒给碰撞系统，再按落空生成 `CabbageSplat` 并回收。
- 起点、预测落点、elapsed、duration、apex、旋转和速度均随正式快照往返；
  对象池 `Reset()` 清空全部抛射状态并恢复碰撞器。卷心菜池槽基础伤害恢复 40，
  不响应台风轻型子弹倍率，也不进入屋顶平射遮挡判定。
- 卷心菜贴图由 reanim 自动加载出的
  `IMAGE_REANIM_CABBAGEPULT_CABBAGE` 取得；绘制以逻辑弹心居中，沿飞行旋转，
  地面阴影独立采样当前 X 的地形并随离地高度缩小。命中与落空使用
  `CabbageSplat`，首 emitter 名必须与运行时键一致。
- `GameDataManager` 注册 `IMAGE_CABBAGEPULT/ANIM_CABBAGEPULT/REANIM_CABBAGEPULT`；
  冒险内部关卡 36（4-9）结算后由既有奖励表解锁。

## 验证证据

- `clang-release` 配置与最终 LTO 链接成功。
- `smoke_cabbagepult.json` 在主人当前桌面可见运行，默认实例化与
  `-NoInstance` 均 exit 0；每条路径 123 条命令、45 条状态断言全绿。
- 专项覆盖资源闭环、无目标不射击、第 43 帧发弹、40 伤、Throw、固定靶与
  移动靶预判、拱顶碰撞关闭、在途快照、落空飞溅、池槽复位、屋顶花盆承载和
  4-9 奖励。两条绘制路径关键截图逐张检查，本体基线、旋转高弧、命中飞溅与
  屋顶视觉一致；日志无 FAIL、Fatal、WATCHDOG 或资源缺失。
