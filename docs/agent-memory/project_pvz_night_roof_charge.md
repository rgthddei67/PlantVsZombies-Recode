---
name: project-pvz-night-roof-charge
description: 黑夜屋顶雷荷的背景资格、积累状态机、放电实体效果、UI、存档和 AutoTest 契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-10
---

# 黑夜屋顶基础雷荷

## 场景与所有权

- `Background::NIGHT_ROOF` 是唯一资格；不按冒险关卡号判断。正式 6-1～6-9（内部 46～54）均映射到该背景，5-9 保持白天 `ROOF`。
- `Board` 唯一持有雷荷；它与 `RainIntensity`、昼夜屋顶径流相互独立。雷荷不新增雨势档位，也不替换黑夜屋顶的冲刷。
- 预警转放电的唯一边沿会对锁定行形成一次稳定实体 ID 快照并结算基础玩法；处决僵尸劫持、避雷植物和台风改向仍未实现。

## 状态机与数值

- `CHARGING` 阶段范围为 0～100：晴夜每游戏秒减少 0.5，小/中/大雨每游戏秒增加 1/2/3；现有大雨局部闪电每次额外增加 18。
- 达到 100 时仅随机一次行，进入 4.0 游戏秒 `WARNING`；随后进入 0.65 游戏秒 `DISCHARGING`，沿锁定行从平台到屋檐绘制程序化折线并请求现有雷声。
- 跨过 100 的同一笔正向输入会把溢出转入余电；预警和放电演出期间的雨势积累、普通局部闪电也只增加余电，封顶15，不改变本次放电数值。活动阶段晴夜不泄漏余电；放电结束将余电一次兑现为下一轮 `CHARGING` 初值并清零，之后重新服从每秒0.5漏电。
- 活动阶段的行、阶段、余时和余电是唯一未来事实；绘制不得重抽。`RestoreNightRoofChargeState` 会拒绝非黑夜屋顶、非法行、非法阶段、非有限数值和不完整活动组合。

## 放电实体效果与通用接口

- 普通瓦面上，锁定行所有活动非花盆植物停机 2.5 游戏秒；地面僵尸承受 75 点 `OTHER` 环境伤害，非车辆再麻痹 0.75 游戏秒。
- 若锁定行在放电瞬间正处于径流 `FLOWING`，仅坡面区升级为强导电：植物停机 5.0 秒，僵尸承受 120 伤害并麻痹 1.2 秒；同一行平台仍使用普通数值。花盆不停机，飞行与地下阶段连伤害都免疫，冰车/投篮车等车辆只受伤不麻痹。
- `Plant::ApplyShutdown/IsShutdown` 是可复用停机接口：计时停机按较长余时刷新并入实体存档；`IsShutdown` 同时合并 Board 的连续径流区域源。植物并行帧事件、串行动画和 `PlantUpdate` 只认该统一有效查询，禁止用 Animator `Pause/Play`。
- `Zombie::ApplyParalysis/IsParalyzed` 是可复用麻痹接口，已进入 `IsImmobilized`、动画速度层和保护字段存档。`CanBeParalyzed` 表达车辆等永久免疫，`CanBeAffectedByGroundHazards` 独立表达飞行/地下阶段免伤；中立环境麻痹跨魅惑保留，但死亡轨前必须原子清除。
- 放电效果只在 `WARNING -> DISCHARGING` 转换时调用一次。保存/恢复已经处于 `DISCHARGING` 的局面只还原 Board 阶段以及实体生命/状态余时，绝不重复伤害或重施状态。

## 展示与持久化

- 天气展板在黑夜屋顶增加紫色“屋顶雷荷”进度条，与青色径流条并列常驻；预警与放电显示锁定行、余电百分比，并在满条右端叠加亮紫余电段。
- 世界预警是锁定行上的稀疏紫色节点/小分叉；放电沿 `Board::GetRowCenterYAtX` 贴合连续坡面，不使用粒子 XML。
- `GameInfoSaver` 保存 `nightRoofCharge/Overcharge/Phase/PhaseTimer/Row`、植物 `shutdownTimer` 与僵尸保护字段 `paralysisTimer`。旧档缺余电字段使用中性0，无需提升 schema；活动阶段快照往返继续原行、余时和余电。

## AutoTest 与证据

- `set_night_roof_charge` 用 `phase=CHARGING/WARNING/DISCHARGING`、`charge`、活动阶段 `row` 和可选 `remaining/overcharge` 固定状态。
- `dump_state.weather.nightRoofCharge` 导出 `supported/chargePct/overchargePct/phase/row/phaseRemainingMs/dischargeProgressPct`。
- 实体状态另导出植物 `shutdown/shutdownTimerMs` 与僵尸 `paralyzed/paralysisTimerMs/canBeParalyzed/groundHazardEligible`；`zombiesByType` 供异品种靶按语义稳定取证。
- 2026-08-10 `clang-release` 配置、编译和 LTO 链接退出 0。主人当前桌面可见 `smoke_night_roof_charge.json` 已覆盖跨阈值溢出、15%封顶、活动期雨势积累、余电存读档和放电后兑现；`smoke_night_roof_charge_effects.json` 覆盖干瓦/湿坡、平台分区、花盆/飞行/地下/车辆边界、中立麻痹跨魅惑、普通小丑技能停表和放电中存读档防重入。
- 同步截图已目验 6-1 为正式黑夜屋顶，预警行节点稀疏可读，放电折线贴合锁定坡面；余电文案和满条右端亮紫段均在天气展板内清晰可见。

## 后续边界

- 避雷植物、台风改向和处决僵尸劫持仍属第六大关延期内容；基础放电不得预先实现这些反制或改写已确认数值。
- 第六大关当前没有专属奖励、出怪表或 6-9 BOSS；普通僵尸兜底只用于让场景骨架可进入，不能作为最终关卡设计依据。
