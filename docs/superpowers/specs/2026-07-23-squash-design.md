# 倭瓜设计

## 目标

- 新增经典植物 `PLANT_SQUASH`，使用 `Squash.reanim`，阳光 50、冷却 30 秒。
- 通关冒险 3-1（内部关卡 19）后获得；奖励继续走 `AdventureProgression` 显式表。
- 行为与 `PlantsVsZombies.NET-master` 的 `Plant.UpdateSquash` / `FindSquashTarget` /
  `DoSquashDamage` 对齐，不引入动画帧事件。

## 行为

倭瓜待机时逐帧扫描本行敌对、有头、可碰撞的僵尸。普通目标进入植物攻击矩形前方
70 像素后触发；正在啃食的目标放宽到 110 像素。多个倭瓜不会同时锁定同一只僵尸，
撑杆僵尸持杆或跳跃阶段不作为目标。

触发后依次经历：

1. `LOOKING`：朝目标播放 `anim_lookleft` 或 `anim_lookright`，保持 0.8 秒。
2. `PRELAUNCH`：播放 `anim_jumpup`，0.3 秒后重新索敌并按目标 0.3 秒运动提前量修正落点。
3. `RISING`：0.5 秒阶段；前 0.3 秒用 EaseInOut 从原格中心移动到落点上方 120 像素，
   后 0.2 秒停在最高点。
4. `FALLING`：播放 `anim_jumpdown`，0.1 秒落地；剩余 0.04 秒时结算落点攻击矩形。
   本体当前耐久不超过 1800 的僵尸直接调用 `Die()`；超过 1800 时承受 1800 植物伤害，
   并按 C# `TakeDamage(1800, 18U)` 的 bit1 语义穿透二类护盾。
5. 草地落地后抖屏、播放重击声和 `DustSquash`，停留 1 秒后消失；水路落地播放
   `SquashPoolSplash` 和水花声并立即消失。

动画速率按 C# 的 24/60 fps 相对 `Squash.reanim` 基础 12 fps 折算：
look/jumpup 为 2.0 倍，jumpdown 为 5.0 倍。五条所需轨道为
`anim_idle`、`anim_lookleft`、`anim_lookright`、`anim_jumpup`、`anim_jumpdown`。

## 存档与验证

自定义状态、剩余时间、目标 ID、落点 X 和伤害是否已结算写入 `SaveExtraData`。
读档根据状态和剩余时间重建空中位置；动画播放状态由 `GameInfoSaver` 统一恢复。

`smoke_squash.json` 覆盖站位/阴影、左右观察与跳跃轨道、1800 伤害、落地尘土和水路
立即消失；`smoke_adventure_progression.json` 补充 3-1 奖励断言。
