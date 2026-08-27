---
name: project-pvz-squash
description: "2026-07-23 倭瓜：C# Squash 状态机、3-1 奖励、1800 阈值直杀、草地尘土/水路溅水、存档与动画轨道"
metadata:
  node_type: memory
  type: project
---

# 倭瓜（Squash）

2026-07-23 新增 `PLANT_SQUASH`。内部关卡 19（显示 3-1）通关奖励为倭瓜；基础数值为 50 阳光、30 秒冷却，动画使用 `Squash.reanim`。

## C# 对齐口径

权威参考为 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Plant\Plant.cs` 的
`FindSquashTarget`、`UpdateSquash`、`DoSquashDamage`，以及 `Zombie.cs` 的
`TakeDamage` / `PlayDeathAnim`：

- 状态时序：观察 0.8 秒 → 跳跃预备 0.3 秒 → 上升 0.5 秒（前 0.3 秒移动、后 0.2 秒悬停）→ 下砸 0.1 秒。
- 起跳前重新索敌，并按目标 0.3 秒后的水平位置修正落点；多株倭瓜不会同时锁定同一僵尸。
- C# 伤害入口是 `TakeDamage(1800, 18U)`：bit1 穿透二类护盾，bit4 令被打空本体的普通目标立即消失。本项目按主人确认的结果语义显式实现为：当前本体耐久 `<=1800` 直接 `Die()`；更高耐久目标调用穿透二类护盾的 `TakeDamage(1800, PLANT, true)`。
- 草地落地抖屏、播放重击声并发射 `DustSquash`，压扁造型保留 1 秒；水路落地发射 `SquashPoolSplash`、播放水花声并立即消失。
- C# `NotOnGround()` 仅在 `SquashRising`、`SquashFalling`、`SquashDoneFalling` 返回 true；本项目对应 `RISING/FALLING/LANDED` 阶段 `CanBeEaten=false`，观察与起跳预备仍保持可被选中。组合格的僵尸 `EatingOrder` 只能让仍可啃的上层遮挡支撑层，因此倭瓜升空后僵尸应持续啃花盆/睡莲，不能停吃恢复行走后又被下层碰撞重开同一啃食帧。
- C# `Plant::Squish()` 只允许尚未索敌的 `Notready` 倭瓜进入压扁态；本项目由 `Squash::ResolveGargantuarSmash()` 对齐：`IDLE` 仍可压扁，`LOOKING/PRELAUNCH/RISING/FALLING/LANDED` 均忽略巨人锤击并继续原有完整动画。同格花盆等其他层仍由巨人快照独立结算。

## 动画与资源

不需要新增帧事件。所需轨道只有：

- `anim_idle`
- `anim_lookleft`
- `anim_lookright`
- `anim_jumpup`
- `anim_jumpdown`

`Squash.reanim` 基础帧率为 12 fps；look/jumpup 按 C# 的 24 fps 使用 2.0 倍，jumpdown 按 60 fps 使用 5.0 倍。重演文件里的 `IMAGE_REANIM_SQUASH_*` 会由 `Reanimation::LoadFromFile` 从 `resources/image/reanim/` 懒加载，不要把四张分层图另注册成 `IMAGE_SQUASH_*`。

权威资产只放在 `build/clang-release/resources`；`clang-playtest` / `msvc-debug` 通过 Junction 共用。新增资源含倭瓜卡图、reanim、四张分层图、Hmm 音效、落地/水花音效、尘土与水花粒子贴图和两个粒子配置。

## 存档与验证状态

`Squash` 保存状态、剩余计时、目标 ID、预测落点 X 和伤害是否已结算；读档会重建空中位置、碰撞开关及固定地面阴影。

2026-07-23 `clang-playtest` 与 `clang-release`（LTO）均编译通过且无警告。可见 `smoke_squash.json` 在主人叫停 AutoTest 前已通过草地的 idle/look/jumpup/jumpdown、伤害、落地与自毁断言；水路首次等待 1.72 秒落在 `FALLING` 边界，脚本已放宽到 1.85 秒。随后主人要求不再运行 AutoTest，并把伤害改为显式阈值直杀，因此当前脚本已同步新断言但尚未在最终代码上重跑；不得把它记成最终通过。2026-08-04 补齐离地啃食资格与 C# EatingOrder 后，`clang-release` 再次编译通过；按主人要求仍未运行 AutoTest，待使用 `level37_data.json` 实机确认。

设计文档：`docs/superpowers/specs/2026-07-23-squash-design.md`。

2026-08-15 `clang-release` 构建通过；可见 `smoke_gargantuar_special_plant_smash.json` 在第 93 帧命中后断言索敌倭瓜仍活动且未压扁、下层花盆已压扁，并继续通过 `PRELAUNCH/RISING/LANDED` 完成 1800 伤害；`smoke_squash.json` 54 条命令 exit 0 并恢复完整观察、上升、落地尘土与水路父回归，关键截图均已目验。

2026-08-27 对齐 C# `NotOnGround()` 的占格语义：倭瓜进入 `RISING` 时立即释放显式 Cell 槽，空中与落地演出仍保留实体、动画和伤害状态，但原格可以马上种植。`Plant::OccupiesGridSlot()` 成为保存/恢复占格顺序的窄契约；快照先恢复非占格的在途倭瓜，再恢复后来种入同格的植物，避免加载时互相覆盖。默认 Vulkan 可见 `smoke_squash.json` 覆盖起跳前不可种、起跳后同格种向日葵，以及在途倭瓜和同格向日葵的快照往返；exit 0，状态与两张关键截图均核对通过。
