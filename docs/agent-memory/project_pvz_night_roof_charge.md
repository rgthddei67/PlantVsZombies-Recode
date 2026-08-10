---
name: project-pvz-night-roof-charge
description: 黑夜屋顶基础雷荷的背景资格、积累状态机、UI、存档和 AutoTest 契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-10
---

# 黑夜屋顶基础雷荷

## 场景与所有权

- `Background::NIGHT_ROOF` 是唯一资格；不按冒险关卡号判断。正式 6-1～6-9（内部 46～54）均映射到该背景，5-9 保持白天 `ROOF`。
- `Board` 唯一持有雷荷；它与 `RainIntensity`、昼夜屋顶径流相互独立。雷荷不新增雨势档位，也不替换黑夜屋顶的冲刷。
- 当前仅实现基础积累、锁行、演出、存档与测试观测，不结算植物/僵尸伤害、暂停、绝缘或处决僵尸联动。

## 状态机与数值

- `CHARGING` 阶段范围为 0～100：晴夜每游戏秒减少 0.5，小/中/大雨每游戏秒增加 1/2/3；现有大雨局部闪电每次额外增加 18。
- 达到 100 时仅随机一次行，进入 4.0 游戏秒 `WARNING`；随后进入 0.65 游戏秒 `DISCHARGING`，沿锁定行从平台到屋檐绘制程序化折线并请求现有雷声，结束后回到 0 与 `CHARGING`。
- 活动阶段的行、阶段和余时是唯一未来事实；绘制不得重抽。`RestoreNightRoofChargeState` 会拒绝非黑夜屋顶、非法行、非法阶段、非有限数值和不完整活动组合。

## 展示与持久化

- 天气展板在黑夜屋顶增加紫色“屋顶雷荷”进度条，与青色径流条并列常驻；预警与放电显示锁定行和状态。
- 世界预警是锁定行上的稀疏紫色节点/小分叉；放电沿 `Board::GetRowCenterYAtX` 贴合连续坡面，不使用粒子 XML。
- `GameInfoSaver` 保存 `nightRoofCharge/Phase/PhaseTimer/Row`。旧档缺字段使用中性积累态，无需提升 schema；活动阶段快照往返继续原行和余时。

## AutoTest 与证据

- `set_night_roof_charge` 用 `phase=CHARGING/WARNING/DISCHARGING`、`charge`、活动阶段 `row` 和可选 `remaining` 固定状态。
- `dump_state.weather.nightRoofCharge` 导出 `supported/chargePct/phase/row/phaseRemainingMs/dischargeProgressPct`。
- 2026-08-10 `clang-release` 配置、编译和 LTO 链接退出 0。主人当前桌面可见 `smoke_night_roof_charge.json`、`smoke_sixth_area_night_roof.json`、`smoke_roof_runoff.json`、`smoke_roof_rain_sky.json`、`smoke_adventure_progression.json` 与 `smoke_fog_weather.json` 均 exit 0、`script finished OK`。
- 同步截图已目验 6-1 为正式黑夜屋顶，预警行节点稀疏可读，放电折线贴合锁定坡面，紫色雷荷条与青色径流条区分明确。

## 后续边界

- 放电伤害/定身、花盆绝缘、避雷植物、台风改向和处决僵尸劫持仍属第六大关内容设计，实施前需重新定案并补充交互专项。
- 第六大关当前没有专属奖励、出怪表或 6-9 BOSS；普通僵尸兜底只用于让场景骨架可进入，不能作为最终关卡设计依据。
