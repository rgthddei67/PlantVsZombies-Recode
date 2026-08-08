---
name: project-pvz-roof-runoff
description: 昼夜屋顶坡面径流的雨势积累、多行冲刷、UI、存档和验证契约
metadata:
  node_type: memory
  type: project
---

# 昼夜屋顶坡面径流

## 当前实现（2026-08-08）

- `Background::ROOF` 与 `Background::NIGHT_ROOF` 共用径流；触发根因是屋顶斜坡与降雨，不按
  昼夜拆分。第六大关未来的雷荷是叠加系统：径流继续冲刷，电荷另管导电瓦路和放电。
- `Board` 唯一持有 0～100 积累、`IDLE/WARNING/FLOWING` 阶段、阶段余时和锁定行 bitmask。
  小/中/大雨每游戏秒增加 `1.00/2.00/3.50`，晴天每游戏秒排走 `0.30`；首次从 0 满值
  约需 `100/50/29` 游戏秒。
- 满值后一次抽取不重复的 1～3 行，数量权重为 `50/35/15`；锁定行组先预警 3 游戏秒，
  再冲刷 2.2 游戏秒。满值锁行时同时均匀预抽 `30%～60%` 的下一轮残留湿度并入档，冲刷结束
  后兑现而非归零；后续小/中/大雨再次满值约需 `40～70/20～35/11～20` 秒。
  行组不按场上敌我密度选取，避免系统追打拥挤防线。
- 冲刷只作用屋顶坡段（当前第 1～5 列）：花盆本体保持基础设施，不暂停；其上普通植物与南瓜
  暂停动画和行动。可被地面风力移动的僵尸获得 `-60 px/游戏秒` 的屋檐向漂移，单次最多约
  `132px`；冻结和啃食早退
  前照常应用并重新贴合连续坡面；平台、飞行态和不符合地面状态的僵尸不受影响。
- 导流投篮车是锁行阶段的窄扩展：满值时由 Board 选择坡段内活动、未爆胎且最靠房屋的候选，
  若其行不在随机组内只替换一个已选行，不增加行数。最终 row mask 一旦进入 WARNING 就不再随候选
  死亡或新生变化。僵尸基类只提供实例径流倍率；普通僵尸保持 `-60px/s`，未爆胎导流车独享
  `5/3` 倍即 `-100px/s`，爆胎后回到倍率 1。该能力不把导流状态复制到实体，也不改变全行僵尸。

## 展示与持久化

- 天气展板在支持天气的实战中常驻；径流使用可复用累计条。当前青色用于径流；未来夜屋顶电荷
  复用同一规格的另一颜色进度条，两条状态各自独立。
- 活动阶段显示完整行组，例如“第2、4行预警/冲刷中”。世界特效按
  `Board::GetRowCenterYAtX` 贴合坡面：预警用稀疏湿润光点；冲刷用无描边低透明水膜、少量移动
  水珠和各行屋檐飞溅。不要恢复成大量贯穿或规则短线，静帧会显得像轨迹覆盖层。
- `GameInfoSaver` 保存 `roofRunoffCharge/RetainedCharge/Phase/PhaseTimer/RowMask`；旧档缺字段回到
  中性状态。活动阶段读档只继续原锁定行组、余时和已预抽残留湿度，不重新抽取。
- `set_roof_runoff` 可用 `rows` 数组固定活动行组，以 `retainedCharge` 固定结束后的湿度；单个
  `row` 仅为旧脚本兼容。dump 导出 `chargePct/retainedChargePct/phase/rowMask/rowCount/rows`，
  以及 `phaseRemainingMs/flowProgressPct/zombieDriftSpeed/guideCandidateRow/guideCandidateSelected`；僵尸逐体
  导出 `roofRunoffGuideEligible/roofRunoffDriftMultiplierOn1000/roofRunoffDriftVelocity`。

## 验证证据

- `clang-release` 配置、编译和链接通过。
- 可见 `smoke_roof_runoff.json` 覆盖自然满值进入 1～3 行预警及 30%～60% 残留预抽、固定多行、
  植物与僵尸正反例、活动阶段 45% 残留快照往返、结束兑现、昼夜屋顶和同步预警/冲刷流水截图；
  `smoke_roof_terrain_consumers.json` 92 条命令回归通过；窗口标题
  `植物大战僵尸中文版`，退出码 0，`run.log` 以 `script finished OK` 结束。
- 可见 `smoke_elite_catapult.json` 进一步锁定导流替换、最近房屋候选、WARNING 死亡不重抽和
  普通/精英 `-60/-100` 实例速度；固定种子下死亡前后 row mask 都为 22，候选行从 4 变 0 后仍未补入。
