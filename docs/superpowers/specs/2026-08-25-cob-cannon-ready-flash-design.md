# 玉米加农炮待发炮弹闪烁设计

日期：2026-08-25　状态：按主人要求还原原版表现

## 目标

玉米加农炮进入 `READY` 后，仅让 `CobCannon_cob` 炮弹轨道按原版节奏明暗闪烁；开火、重新装填和其他状态都保持纯白，不改变炮身、动画时序、点击或存档玩法状态。炮弹仍在空中时不再于冻结落点提前绘制黑色椭圆标记，改为复用普通 `IMAGE_PLANTSHADOW` 飞行阴影；瞄准光标保留，落地后原版 `Blastmark` 焦痕仍正常生成。

## 原版证据

`Plant.cs::UpdateCobCannon` 在 `CobcannonReady` 中每次更新把炮弹轨道颜色设为 `TodCommon.GetFlashingColor(mBoard.mMainCounter, 75)`，`CobCannonFire` 则立刻恢复 `SexyColor.White`。原版公式以 75 个 100Hz 计数形成 0.75 秒三角波，RGB 在 55～255 之间同步变化，Alpha 恒为 255。

`Projectile.cs::DrawShadow` 的 `Cobbig` 分支继续使用普通弹丸阴影图，把 X 轴拉宽到 3 倍；飞行高度截断到 200px 后按 `200 / (高度 + 200)` 缩放，因此高空为落地尺寸的 0.5 倍，垂降时连续长回 1 倍。该层与瞄准光标、落地 `Blastmark` 是三个独立视觉阶段。

## 方案

- 为 `Animator` 的 `TrackExtraInfo` 增加默认纯白的单轨道乘色，并提供 `SetTrackColor/GetTrackColor` 窄接口；默认实例化与 `-NoInstance` 绘制路径共用相同颜色和 Alpha 计算。
- `CobCannon` 以 Board 已保存、随游戏时间推进的 60Hz `mBoardFrame` 为节拍，将原版 0.75 秒周期换算为 45 帧；只有 `READY` 每帧更新炮弹轨道颜色。
- `SetupPlant`、装填、开火和读档归一化都显式同步颜色，保证进入非 READY 状态时立即恢复纯白，不新增存档字段。
- 删除 `Bullet::Draw` 中炮弹飞行后段对 `IMAGE_COBCANNON_TARGET_SHADOW` 的额外提交；C# 原版飞行路径没有该预告层。`GameScene::DrawCobCannonTarget` 继续用于玩家瞄准，落地结算继续发射 `CobCannonBlastMark`。
- 玉米棒重新启用 Bullet 自带的 `ShadowComponent` 和 `IMAGE_PLANTSHADOW`，保持原版 3:1 横向倍率，并把三段飞行轨迹映射为 0→200px 升空、200px 高空换位、200→0px 垂降，再套用原版高度公式。对象池复用和在途读档都重建同一派生阴影，不增加存档字段。

## 验收

- `smoke_cob_cannon_core.json` 投影炮弹轨道 RGBA：未就绪为纯白，READY 为等通道、Alpha 255 且处于 55～255，开火后立即回到纯白。
- READY 相隔半周期截两张同步截图，确认只有炮弹明暗变化；在途、高空和垂降截图确认没有固定黑色预告标记，且 `IMAGE_PLANTSHADOW` 从高空约 0.5 倍逐渐长大；落地截图确认焦痕仍存在。默认实例化、`-NoInstance` 与 OpenGL 各运行一次并检查截图、状态、日志和退出码。
