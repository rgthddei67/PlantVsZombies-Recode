# 雨天天气扩展契约

本文件记录截至 2026-08-23 的当前实现。动手前用文中的搜索词核实源码；当前代码优先于本文件。

涉及原版已有天气或视觉时，C# 只用来锁定玩家可感知的功能、时序和反馈；实现前仍须核对本项目 `Board` 所有权、独立天气维度、场景坐标、资源与存档契约，并以当前接口实现等价行为，禁止机械照搬类结构或绝对数值。

## 目录

- [天气状态与时间语义](#天气状态与时间语义)
- [波次锁定的复合天气](#波次锁定的复合天气)
- [地图专属天气积累器](#地图专属天气积累器)
- [独立雾势与跨天气联动](#独立雾势与跨天气联动)
- [源码钟点](#源码钟点)
- [雨天专属能力配方](#雨天专属能力配方)
- [雨天出生变异配方](#雨天出生变异配方)
- [可逆形态配方](#可逆形态配方)
- [存档契约](#存档契约)
- [AutoTest 契约](#autotest-契约)

## 天气状态与时间语义

`RainIntensity` 有 `CLEAR/LIGHT/MEDIUM/HEAVY` 四档，和 `TyphoonStrength`、`WindDirection`
一起声明在 `WeatherTypes.h`。`Board` 是唯一权威：

| 接口 | 语义 |
|---|---|
| `GetRainIntensity()` | 当前目标档位；正式切档瞬间改变 |
| `GetPreviousRainIntensity()` | 两秒平滑过渡的来源档位 |
| `IsWeatherTransitionActive()` | 倍率、暗幕和雨声仍在过渡 |
| `GetZombieRainSpeedMultiplier()` | 已插值的僵尸动画/移动倍率 |
| `GetPlantRainActionSpeedMultiplier()` | 已插值的植物行动倍率 |
| `GetRainOverlayAlpha()` | 已插值的世界暗幕 alpha |

暂停时天气过渡和阶段计时冻结；倍速时按游戏时间等比推进。天气 UI 的滑入、5 秒当前天气牌和失败提示另用未缩放时间。

提前 5 游戏秒的大雨分级文字警报只由玩家可见的公开预报驱动：公开预报为 `HEAVY` 就显示，
否则即使真实下一天气为大雨也隐藏，弹窗有无不得成为判断预报真假的旁路。公开误报大雨时也须
用真实大雨完全相同的权重锁定警报等级和文案编号，并随关卡快照恢复；揭晓为非大雨时必须清除，
不得改变当前台风、天气或保底。真实下一天气为新大雨时仍须在预报锁定期预抽完整待生效台风
状态；即使公开预报不是大雨而隐藏警报，切档时也要消费同一份锁定结果。
天气预报行若同时显示台风等级，也只能消费这份玩家可见的锁定警报等级：公开预报为大雨且等级
不是 `NONE` 时追加“台风/强台风/超强台风”；公开误报大雨显示同分布诱饵等级，公开预报非大雨
则不得显示隐藏的真实待生效台风。当前天气行仍只在台风真正生效后显示实况等级与风向。

离散能力默认这样判断：

```cpp
const RainIntensity rain = mBoard ? mBoard->GetRainIntensity() : RainIntensity::CLEAR;
const bool isRaining = rain != RainIntensity::CLEAR;
```

不要用 overlay alpha 推断是否下雨：它是视觉插值，雨转晴的两秒内仍大于 0。

主菜单控制台的 `openingTyphoonProtectionEnabled` 是玩家全局偏好，旧 `PlayerInfo` 缺字段时默认
开启。开启后，冒险关和生存首轮的第 1～5 波仍允许自然雨势，但新大雨附加台风的实际概率为 0；
进入第 6 波恢复原天气导演概率，生存第二轮起不再重复保护。关闭开关则从开局起完整使用原规则。
天气预警可能在阶段揭晓前锁定台风结果，因此“本次因开局保护锁定为无台风”由 Board pending
标志进入关卡档；兑现时不增加 `heavyPhasesWithoutTyphoon`，避免第 6 波因保护期被伪计为连续落空。

实体若能主动改变雨势，实体只保存施法次数/冷却或生命跨段，实际切档必须走 Board 的窄入口。
入口要显式检查地图支持与关卡锁定，并规定强度优先级、同档续期和台风策略；默认只升不降，周期
技能不续同档，避免把自然大雨降级或永久锁雨。一次性阶段技能允许补足同档余时时也须显式传参。
入口复用 `BeginRain` 后，雨势、过渡、余时、粒子、声音、天气板和存档仍由 Board 统一负责，实体
不得复制这些字段。专项覆盖不支持地图 no-op、弱转强、已有更强天气、同档策略与双方快照往返。

## 地图专属天气积累器

昼夜屋顶径流是当前首个地图专属积累器。`Board::UpdateRoofRunoff` 在 `ROOF/NIGHT_ROOF` 中把雨势
换算为积累，满值后一次锁定 1～3 个不重复目标行，并同时预抽冲刷后的 30%～60% 残留湿度；
`roofRunoffRowMask`、pending 残留、阶段和余时会影响未来结算，必须随关卡存档。结束时只兑现
pending 并清空，不能临时重抽。`GameScene` 的累计条与坡面水膜、`Plant` 暂停、`Zombie` 漂移和 AutoTest dump
都查询同一 Board 行组，禁止在展示或实体侧另存/重抽目标。世界效果以 `GetRowCenterYAtX` 贴坡，
低透明面状表现优先于密集规则流线；多行同时生效时每行各绘制水膜和屋檐反馈。

实体若能引导锁行，Board 只在积累满值的锁定帧查询一次语义资格，按明确的位置与稳定 ID 规则
选唯一候选；只能替换现有目标，不能增加已抽中的行数。进入 WARNING 后最终 row mask 是唯一事实，
候选死亡、晚出生或跨波残留都不得重抽。导流投篮车当前选择坡段内最靠房屋的活动未爆胎实例，
只把自己的行纳入；普通地面僵尸仍承受 `-60px/s`，该实例通过 Zombie 虚倍率 `5/3` 独享
`-100px/s`，爆胎后回到倍率 1。禁止把这一能力实现成选中行所有僵尸的公共加速。

黑夜屋顶雷荷是并行的第二个积累器。`Board::UpdateNightRoofCharge` 只在 `NIGHT_ROOF` 中推进：
晴夜每秒漏 0.5，小/中/大雨每秒积累 1/2/3，现有大雨闪电一次增加 18；满 100 后锁定一次路线，
普通预警 4 秒、有效劫持者预警 7 秒，放电演出 0.65 秒。每个普通行与每只有效引雷单位都是候选；
轻量蒙特卡洛开启时用独立 32 rollout、10 秒时域和共同随机数统一比较，关闭或失败时才消费正式 RNG 回退。
`nightRoofCharge/Overcharge/Phase/PhaseTimer/Row/Guided/GuideID` 全部入档，紫色累计条、锁行节点、
坡面折线、附件短弧和 AutoTest 都查询 Board 同一状态。路线在 WARNING 开始时锁定；实体随后死亡、换行、
破甲或新出生都不重选。`WARNING -> DISCHARGING` 的唯一边沿按稳定 ID 快照结算：普通非花盆植物停机
8 秒，地面僵尸承受 200 环境伤害并麻痹 1.5 秒；若锁定行正处于径流 `FLOWING`，仅湿坡升级为
20 秒、600 伤害和 5.5 秒。同一行平台仍用普通数值，花盆不停机，飞行/地下阶段免疫，车辆只受伤。
引导路线保留同一套植物停机与保护，但跳过全部僵尸伤害/麻痹；锁定引雷者放电时仍合格，才以其碰撞箱中心为圆心，让 130 像素内同阵营活动僵尸清除减速/冻结/黄油并获得 30 秒对应免疫。范围只在放电边沿结算一次，麻痹、灰烬和普通伤害不受影响，离域或引雷者之后失效不取消既得余时。
满电的同一笔正向输入若越过 100，溢出部分立即进入余电；从预警开始
到放电演出结束，雨势积累与普通局部闪电也只增加余电，封顶 15，不改变本次放电数值。活动阶段晴夜
不泄漏余电；放电结束一次兑现为下一轮主电荷并清零，之后才重新服从晴夜泄漏。恢复已经处于
`DISCHARGING` 的档不得重复结算，余电也必须按已保存值继续而不能重算。

避雷花盆是花盆 `under` 层的紫卡原位升级。它只有在自身格仍承载活动的 `normal/pumpkin/overlay`
宿主时才生效：单格宿主保护同格；跨格宿主则由 Board 在结算边沿扫描完整 footprint，任一占格的
有效避雷花盆都保护同一个宿主实体整株，支撑物必须复核自身格当前仍引用该目标，不能要求目标锚点
列与花盆列相等。受保护宿主免本次普通雷荷停机与劫持者处决；锁定行内存在至少一个有效避雷花盆时，
普通雷击对合格地面僵尸的干坡/湿坡伤害由 200/600 提升为 400/1200，麻痹不变，多盆不叠加。
保护只通过 Cell 分层入口在结算边沿查询；同行倍率只扫描该行固定列一次并取最大值。空盆返回中性值，
活跃保护不入档；正常外伤摧毁花盆后，上层按既有层语义保留且保护立即失效。

场上至少存在一只仍满足候选门禁的劫持者时，小/中/大雨输入在上述基础上固定再增加 4.1 点/秒；
多只不叠加，晴夜仍只漏电。Board 的热路径通过 `EntityRegistry` 劫持者专用弱索引在首个有效候选
处返回，不扫描普通僵尸；该增量完全由当前雨势与实体派生，无需入档。75% 选中者首次锁定时当前与
最大本体生命各增加 1000，实体保存幂等标志；Board 完成全部实体恢复后重建锁定阶段时不得重复加血。

目标间允许承接放电时，仍复用这份排序后的僵尸 ID 快照：每个目标从同一快照经 `Zombie`
语义虚接口筛提供者，按水平距离、再按稳定 ID 选唯一实例。提供者负责复核同行、阵营、地面资格、
当前防具与干湿状态，并原子返回是否承接成功；成功后 Board 不再对目标施加原伤害或麻痹，失败才调
目标自己的放电入口。持续湿润一类效果把“当前仍在 FLOWING 坡段”与“离坡后的存档残留计时”分开；
固定湿坡伤害只能看前者，不能因残留湿润把平台放电也升级成湿坡数值。

环境要求植物完全暂停时，不能只让 `PlantUpdate()` 提前返回：并行阶段可能已经由 Animator 产生
帧事件。应同时阻断 `UpdateParallel` 的事件队列，并在串行回退把 `mAdvancedInParallel` 置位，让
`AnimatedObject::Update` 跳过本帧 Animator，再让公开行动倍率返回 0。禁止临时 `Pause/Play`：
一次性轨道可能被公共结束检查误判，此前已经暂停的轨道也可能被错误唤醒。离散来源调用
`Plant::ApplyShutdown` 并保存计时余量；连续径流继续由 Board 区域查询拥有，二者在
`Plant::IsShutdown` 合并，不能每帧用短计时刷新来模拟区域场。

## 波次锁定的复合天气

4-9 的暴风雨夜是当前首个由关卡/波次派生的复合天气：第 22 波只显示紫红色
`天气预报：暴风雨`，第 23～30 波固定大雨、大雾和强台风。生效时立即完成普通雨势的两秒过渡，
锁定阶段不再走自然雨势/雾势/台风衰减；强台风仍按原规则仅有一次阵风，已消费额度不得因逐帧
强制、天气测试入口或读档返还。该阶段同时把 `CalculateWaveZombiePoints()` 乘 2，并把普通出波
倒计时上限改为 5 游戏秒；现有波内血量阈值仍可提前开波，最终波前的 7.5 秒大波警告保持独立。

`IsStormyNightActive()` 由 level/wave 重算，不需要保存；`mStormyNightInitialized`、闪光 pattern/余时
以及既有台风阵风状态会影响未来行为，必须进入关卡存档。旧档缺初始化字段时只补做一次激活；
新档恢复后不得重播入场状态或重置阵风。世界层使用 C# 4-10 的黑幕/白闪节奏，普通大雨的局部
程序化闪电在此阶段停用，`trigger_lightning` 改走全屏短闪与同一雷声请求。

天气覆盖层位于世界粒子之后、UI 之前。`Scene` 的 `isUI=true` 贴图必须由 pre-overlay 接缝在
`DrawWorldOverlay()` 后绘制，再继续画 UI GameObject；否则卡片虽可见，`IMAGE_SEEDBANK_LONG`
这类 Scene 卡槽底板仍会被全屏黑幕遮掉。不要按当前卡槽坐标给黑幕挖洞。

昼夜屋顶各有只作用于静态背景的雨景变体：`GameScene::DrawRoofRainBackground` 以
`LAYER_BACKGROUND + 1` 位于原背景贴图之后、冰道和战场实体之前，复用
`GetRainOverlayAlpha()` 的两秒平滑值，按当前背景交叉淡入 `IMAGE_BACKGROUND_ROOF_RAIN`
或 `IMAGE_BACKGROUND_NIGHTROOF_RAIN`。两张 1880×720 PNG 分别以 `background_roof.jpg`
与 `background_nightroof.jpg` 为底图，按同一天空轮廓蒙版合成对应的白天灰云与深蓝紫夜云。
蒙版外的瓦片、烟囱、树木、房屋和卫星锅像素与各自原图完全相同，因此可以在不重绘网格的前提下交叉淡化。
晴夜仍显示原星月；大雨夜 alpha 到 255 时用夜云完整遮住星星和月牙。它不得挪到
`DrawWorldOverlay()`，否则实体会被额外染色。

## 独立雾势与跨天气联动

`FogWeatherIntensity::DEFAULT/SMALL/NORMAL/DENSE` 与 `RainIntensity` 并列声明在
`WeatherTypes.h`。`DEFAULT` 是原版基础覆盖，另外三档是独立增强事件；雾势有自己的阶段计时、
准确预报和存档字段，不要借用雨势误报候选、雨势阶段计时或把雾势伪装成某档雨。当前阶段循环
是“默认雾休整 → 小雾/普通迷雾/大雾事件 → 默认雾休整”，因此增强事件不会无限续期。

跨天气交互只发生在 `Board::UpdateFogDispersal`：只有 `TyphoonStrength::SUPER` 能增加驱散比例
并在完全驱散时提前结束增强雾势；普通和强台风只按风向推动雾团，不降低 alpha，已有驱散量还会
按回流速度恢复。雾势不会反向修改雨势、台风强度或风向。所有雾渲染层共享同一逐格 alpha、
驱散和偏移；层数只由当前雾势派生，不入存档。

外部能力若允许玩家改台风方向，应调用 `Board` 的单一重定向入口：持续风向与活动阵风锁定
方向必须同步更新，并重新取得完整方向持续阶段、重启方向相关视觉。该入口只改方向，不改台风
强度、阵风预算/剩余时长、已经完成的植物换格或迷雾驱散；没有台风时保持 no-op。

迷雾关卡分两层资格：`NIGHT_WATER_POOL` 继续由背景提供通用雾场；明确复用完整迷雾的固定冒险关
由 `AdventureProgression::HasLevelSpecificFogMechanics()` 集中登记；当前仅正式背景为 `NIGHT_ROOF`
的 6-9。固定关卡不能只打开绘制：`SupportsStageFog()`、`SupportsFogWeather()` 与
`SupportsPlanternMechanics()` 必须共同覆盖，使默认雾、动态小雾/普通迷雾/大雾、路灯花燃料/照明/
索敌和正式波次雾火掉落保持同一套机制；以后新增同类关卡只扩展关卡判定函数。

- `GetBaseFogLeftColumn()` 保存由关卡编号换算的原版基准。
- `GetEffectiveFogLeftColumn()` 再应用当前平衡扩展；默认雾不扩格，小雾/普通迷雾/大雾依次多 1/2/3 格。

`GameScene::DrawFog` 使用原生 210×190、8 帧 RGBA 雾片按 80×85 格距高重叠。默认雾只画
原版主层，小雾与普通迷雾画 2 层，大雾画 3 层；补层只换稳定帧并做小幅错位，禁止用固定白色
矩形底幕填透明洞。
1100px 画面右侧收边必须使用另一稳定帧，不能直接照抄原版 800px 同帧尾片让透明洞重合。
原生雾片仍可能因大量低 alpha/透明采样在满逻辑 alpha 时泄露实体轮廓；需要加强遮蔽时，
优先增加消费同一逐格 alpha 的错位冷灰雾片补洞，不要改逻辑层数、索敌阈值或铺纯色矩形。
同步截图必须成对检查“无照明浓雾确实藏住目标”和“路灯花照亮区仍清楚”。

4-2 起以及固定复用完整迷雾的关卡中，逐格 alpha 还承担雾中远程索敌权威。路灯花用
`Board::GetPlanternIllumination(row,col)` 在 `UpdateFogCellAlpha()` 目标值中削减雾，
索敌再读取同一个平滑后的 `GetFogCellAlpha()`；不要另造“逻辑照明范围”绕开可见雾，
否则刚点亮/熄灭时画面与玩法会不同步。近身感知是统一入口中的显式例外，已在飞行的子弹不撤销。
若设计允许植物看入第一格薄雾，目标格超过 alpha 阈值后应沿目标朝植物的方向检查相邻一格：
相邻格已可见才放行当前目标。不要只比较 `GetEffectiveFogLeftColumn()` 或复制路灯花形状，
否则固定雾线、照明边缘和台风平滑过渡会出现三套不同口径；专项同时断言第一格 true、第二格
false，并保留目标格自身仍高 alpha 的证据。
照明形状同时服务产光倍率，必须从 Board getter 派生，不能让各植物复制挡位范围。
当前路灯花 I/II/III 依次使用朝僵尸来向多一列的 4×3、裁角 8×5、裁角 10×7；III
最外圈为 72% 照明。雾火按每关自己的首波到最终波 smoothstep，单团 15→10、普通累计份额
0.50→0.25、高耐久额外份额上限 0.25→0.15，每波最多三团即预算 45→30。
该经济曲线不得复用天气导演压力或写死 4-2 的总波数，否则未来关卡会随天气变强反向增产。

`MistFuel` 是实际到账时序而不只是装饰：僵尸死亡只调用 `Plantern::ReserveFuel` 占用容量，
0.62 游戏秒后抵达当前同 ID 路灯花才 `DeliverReservedFuel`。燃料卡牌在途中保持原值；
目标死亡则丢弃。在途对象本身不进存档，因此 `pendingFuel` 随路灯花保存，读档时结算进实际
燃料，避免丢奖励或永久占用容量。

关卡 schema v3 保存雾势、预报、阶段计时、驱散和偏移。v2 的旧 `CLEAR/DENSE` 二态必须迁移
为视觉等价的 `SMALL/DENSE`，不能把旧双层 `CLEAR` 误降为单层默认雾。v1/无版本旧档没有足够
上下文让纯迁移函数判断是否属于夜间泳池背景，因此加载端保留 `StartGame()` 已按当前 Board 建立的
原版默认雾；只有文档确实包含雾势字段时才调用 `RestoreFogState()`。

`mFogCellAlpha` 不入存档，因为它由当前雾势、驱散量和路灯花照明完整派生。反序列化先恢复
Board 雾势、再恢复植物，所以 `RestoreFogState()` 只清空旧缓存；必须等全部实体恢复完成，
由 `Board::CompleteLoadRestore()` 在首帧绘制前直接同步目标 alpha。禁止让读档后的格子从
0 平滑填充，否则玩家能通过退出重进短暂看穿迷雾；正常新局和游戏中雾势变化仍保持平滑。

## 源码钟点

| 目的 | 当前位置 / 搜索词 | 约束 |
|---|---|---|
| 权重、持续时间、倍率 | `Board.cpp` 匿名命名空间 `k*Rain*` | 调参常量同行中文注释 |
| 随机下一天气 | `Board::RollNextWeather` / `RainTransitionForRoll` | 与合法预报候选保持同构 |
| 合法公开预报 | `BuildPlausibleForecasts` | 错误预报也必须真实可达 |
| 正式切档 | `BeginRain` / `EndRain` / `BeginWeatherTransition` | 目标枚举先变，倍率再插值 |
| 实体主动改天 | `TriggerRoofMarshalWeather` 或同类 Board 窄入口 | 实体只发请求；入口规定只升不降、同档续期与台风策略 |
| 天气逐帧推进 | `Board::UpdateWeather` | 全局场景状态，不属于波次更新 |
| 昼夜屋顶径流 | `Board::UpdateRoofRunoff` / `DrawRoofRunoff` | Board 持积累与行组；GameScene 只画常驻条和坡面瞬态 |
| 黑夜屋顶雷荷 | `Board::UpdateNightRoofCharge` / `ResolveNightRoofChargeDischarge` / `DrawNightRoofCharge` | 只看 `NIGHT_ROOF`；Board 持积累、阶段、余时和锁定行，转换边沿快照实体并调用通用状态接口，GameScene 只画紫条与瞬态 |
| 波次锁定复合天气 | `IsStormyNightActive` / `ActivateStormyNight` / `EnforceStormyNightWeather` | 生效条件派生；一次性资源和闪光未来状态入档 |
| 世界天气覆盖与 Scene UI 贴图 | `GameAPP` pre-overlay hook / `Scene::DrawUITextures` | 世界粒子 → 天气覆盖 → Scene UI 贴图 → UI GameObject |
| 僵尸天气动画倍率 | `Zombie::UpdateAnimSpeed` | 冻结 > ability × 减速 × rain |
| 植物天气行动倍率 | `Plant` 的 weather action helper | 不改变全局 delta |
| 正式波次选型/生成 | `Board::TrySummonZombie` | 出生变异的默认接入点 |
| 通用创建 | `Board::CreateZombie` | AutoTest、召唤等也可能调用，不默认随机变异 |
| 读档创建 | `Board::CreateZombieWithID` | 只还原已保存类型，永不重 roll |
| 预览僵尸 | `Board::CreatePreviewZombies` | 使用基础出怪表，默认不展示临时天气变异 |
| 天气玩法存档 | `GameInfoSaver.cpp` 搜索 `rainIntensity` | `Board` 天气先恢复，再加载实体 |
| 天气 UI 请求 | `BoardPresentation.h` / `GameScene` 实现 | `Board` 不包含具体 `GameScene`，也不持有 UI 计时 |
| 天气 UI 存档 | `CaptureWeatherPresentationState` / `RestoreWeatherPresentationState` | 经展示端口保存可重建的视觉瞬态，不得影响玩法 |
| 天气面板上方交互浮层 | `GameScene::BuildDrawCommands` / `PlanternGearMenu` | `GameObjects` 命令整体早于天气面板；需另注册更晚的 UI 绘制命令，不能只提高组件所属对象层级 |
| 存档版本升级 | `SaveSchema::UpgradeLevelDocument` | 升级成功后才允许修改 `Board` 或实体 |
| 天气 AutoTest 状态 | `TestDriver.cpp` 搜索 `out["weather"]` | 浮点另给整数投影；闪电路径暴露激活、主干/分叉段数与落点 X，雷声暴露资源加载与播放请求次数 |
| 雾势玩法与绘制 | `Board::UpdateFog*` / `GameScene::DrawFog` | Board 持状态；GameScene 只按 getter 绘制 |
| 路灯花照明/雾中索敌 | `GetPlanternIllumination` / `CanPlantAcquireZombie` | 同一逐格 alpha 同时服务视觉和远程索敌；近身例外只在统一入口 |
| 雾势 AutoTest 状态 | `TestDriver.cpp` 搜索 `out["fog"]` / `out["plantern"]` | 断言覆盖逐格 alpha、照明矩阵、驱散、燃料、挡位和资源 |

## 雨天专属能力配方

### 即时伤害、范围或抗性

在真正结算的唯一函数读取天气并选择倍率，不要永久改写基础字段。例如攻击伤害在命中时算；范围在技能释放时算。若伤害要与生存词条组合，明确乘算顺序，并继续传正确的 `DamageSource`。

植物若要抵抗强/超强台风的整格位移，让植物基类提供声明式“锚定格位”和直接撞击回调，
由 `Board::TriggerTyphoonPlantMove` 在既有前缘到后缘、逐格循环中调用：

- 源格任一活跃层声明锚定时，整个 Cell 的全部活跃植物层组合留在原格；搬运、出界与弹坑死亡同样遍历全部层，不能只处理 `under/normal`。
- 目标格取顶层锚定植物；只有来袭组合直接尝试进入该格才派发撞击，后方被普通植物占格阻挡
  不把压力继续传导。
- 每一格阻挡立即结算伤害，使锚定植物死亡后下一步能够重新读取腾出的 Cell；若同一阵风
  多步撞击需要减少噪声，只合并音效/粒子，不合并伤害。
- Board 所有且附着于植物格的共享对象（例如已放置扶梯）随组合一起结算：成功搬运时在清空
  源格后同步更新对象的 `row/column`，绘制直接复用目标格宿主植物的阵风二维视觉偏移，避免
  复制一套计时产生漂移；被阻挡时留在源格，出界或弹坑死亡由植物既有死亡链移除。若目标格
  留有失去宿主的陈旧同类对象，应在迁移事务中恢复唯一格约束。AutoTest 除断言最终格外，还要
  在 0.45 秒追赶中段断言附件偏移非零且与宿主误差为零。
- 这类能力由当前品种、生命与 Cell 状态派生，无需保存实体标志；最近直接阻挡格次可作为
  瞬态 AutoTest 观测值。测试同时覆盖双向、紧邻多步、间隔移动、连续链、水路组合和中途死亡。
- 屋顶等非水平网格仍沿用同一逻辑换格，但视觉插值必须使用源/目标 `GetCellCenterPosition`
  的完整二维坐标；Animator、影子和 AutoTest 的视觉锚点共用该结果，不能只改 X 后保留源格 Y。

雨滴等地面瞬态若按行随机落点，先随机 X，再用 `GetRowCenterYAtX` 求该 X 对应的连续地面线并
建立窄带判定；整行固定 Y 带只适用于水平地图。粒子自身仍是瞬态，不进入存档。

### 技能冷却或蓄力

只缩放该能力自己的计时：

- 想随两秒天气过渡平滑增强，可使用 `GetZombieRainSpeedMultiplier()` 或新增语义明确的天气能力 getter。
- 想在雨开始瞬间切换，按 `GetRainIntensity()` 的命名档位选择倍率。
- 能力由动画帧事件触发时，雨天已经通过 Animator extra 层让事件更早到达；不要再次缩短同一轮动画的逻辑计时。
- 不要把倍率乘到整个 `ZombieUpdate(scaledDelta)`，否则移动、状态机和所有子技能一起被意外加速。

### 动画与移动

子类通过 `GetAbilityAnimSpeedMultiplier()` 返回自身最终整体动画倍率；固定品种值可直接返回常量，动态阶段从已保存状态派生，实例随机值必须由派生类持久化。状态变化后调用 `UpdateAnimSpeed()`，不要直接设置 Animator extra 倍率，否则会覆盖冻结停格或丢失减速/雨天组合。`PlayTrack` 的 clip speed 是另一层，允许换轨但不能替代状态层倍率。

活动阵风对僵尸的漂移在冻结与啃食早退之前修改真实 Transform。移动后必须由僵尸基类复核已保存的植物目标仍存活、仍是顶层且保持有效咬合距离，不能把收尾完全托付给碰撞退出回调：海豚/撑杆被高坚果阻拦后可能停在碰撞箱外的小间隙并由状态机直接 `StartEat`，因此根本没有已登记碰撞对。目标失效时同步清目标 ID、`mEaterCount` 与啃食视觉；僵尸进入死亡轨道前同样先停止攻击。

## 雨天出生变异配方

默认语义是“基础僵尸被替换为实际精英类型，生成后即使放晴仍保持精英类型”。推荐在 `TrySummonZombie()` 中：

1. `PickZombieType()` 得到基础 `selected`。
2. 用基础类型计算预算 `cost`。
3. 调用单一 `ResolveRainMutationType(selected)`，只在符合雨势时 roll 一次。
4. `CreateZombie(actualType, row, x)` 创建实际类型。
5. 仍扣基础 `cost`，保持原出怪密度；若设计要求精英另收预算，必须显式改为实际类型成本并测试低预算兜底。

变异类型若只允许替换出现，可把其独立抽取权重保持为 0，并由 resolver 产生；仍须按 `adding-zombie` 补齐枚举、工厂、数据、动画、掉落和 AutoTest 名称。

不要在以下位置 roll：

- `CreateZombie()`：会污染 `spawn_zombie`、召唤单位和其他显式创建者。
- `CreateZombieWithID()` / `LoadExtraData()`：会让读档改变实际类型或重复增强。
- `GetWeightedRandomZombie()` 的循环重试内部：同一出生可能 roll 多次，概率会被预算筛选扭曲。
- `mSpawnZombieList`：这是关卡/生存轮次持久数据，不应随瞬态天气反复改写。

如果预览必须显示雨天变异概率，应单独定义 UI 表达；不要直接实例化随机实际变体造成选卡页每次进入都变化。

若出生变异另有“每波最多 N 只”限制：

- 计数属于波次状态而非天气阶段状态；只在 `Board::SummonNextWave()` 正式推进到新波时清零。
- `StopTyphoon()`、放晴、再次进入大雨或切换台风强度都不能重置计数，否则同一波可通过天气切换反复返还额度。
- 当前波计数必须与 `mCurrentWave` 一同进入存档；旧字段迁移为保守的已消费数量，并夹紧到新上限。
- AutoTest 至少覆盖同波前 N 次命中、N+1 被拦截、天气切换不重置，以及正式进入下一波后归零。

## 可逆形态配方

只有明确要求“放晴还原”才在实体保存状态。至少使用：

- `bool mRainMutated`：当前是否已应用形态。
- 幂等 `ApplyRainMutation()` / `RevertRainMutation()`：重复调用不叠层。
- 在更新中比较所需雨势与 `mRainMutated`，只在边沿调用 Apply/Revert。
- `SaveExtraData` / `LoadExtraData` 保存实际形态或足以重建它的稳定标志，旧档默认未变异。

读档顺序中 Board 天气先恢复、僵尸后恢复，因此 `LoadExtraData` 可以校验当前天气；但必须以存档结果为主，不能重新 roll。若形态包含一次性资源、技能次数或冷却，也一并保存。

可逆最大生命值必须先规定比例保持、缺血保持或只改上限；严禁每次 Apply 都按当前值乘、Revert 再除，避免舍入漂移和反复天气回血。

## 存档契约

- 纯 `GetRainIntensity()` 派生的伤害/冷却倍率无需实体字段。
- 出生替换成另一 `ZombieType` 后，关卡存档已经保存实际 `type`；读档走 `CreateZombieWithID`，不再解析天气变异。
- 同类内的一次性随机增强必须保存布尔/枚举/数值结果；不要只保存随机种子并重算。
- 新字段用 `j.value("key", neutralDefault)` 兼容旧档，并夹紧损坏范围。
- 关卡 JSON 根节点包含 `schemaVersion`。结构或语义变化无法只靠中性默认值表达时，提升 `SaveSchema::kCurrentLevelVersion`，增加连续迁移步骤和 `SaveSchemaTests`；升级事务必须在任何 `Board` 状态变更前成功。
- 通用僵尸核心状态优先 `SaveProtectedData/LoadProtectedData`；某个派生类独有状态用其 `SaveExtraData/LoadExtraData`，不要让另一个类解释该字段。
- 若修改 Board 天气未来行为，保存所有会影响下一次抽取的资格和计时；瞬态粒子、水花不存档。
- 关卡/波次派生事件的 active 布尔通常不存，但“是否完成一次性初始化”、闪光节奏/余时、阵风余额等未来状态必须保存；旧档默认未初始化，新档读回后禁止重复初始化。
- 天气 UI 的计时与展示状态经 `BoardPresentation` 捕获/恢复；它们是可重建瞬态，不能反向成为天气玩法权威。

## AutoTest 契约

现有命令：

- `set_weather`：固定天气并立即完成过渡；可传 `duration`、小雨的 `canIntensify`。
- `set_opening_typhoon_protection`：在进程内开关默认启用的前 5 波台风保护，不触碰真实 `PlayerInfo`。
- `set_roof_runoff`：昼夜屋顶可用；`phase=IDLE/WARNING/FLOWING`，活动阶段以非空 `rows` 数组固定行组，可选 `charge/remaining/retainedCharge`；单个 `row` 只作旧脚本兼容。
- `weather.roofRunoff`：导出 `chargePct/retainedChargePct/phase/rowMask/rowCount/rows/phaseRemainingMs/flowProgressPct/zombieDriftSpeed/guideCandidateRow/guideCandidateSelected`；植物另导出 `roofRunoffPaused`，僵尸逐体导出 `roofRunoffGuideEligible/roofRunoffDriftMultiplierOn1000/roofRunoffDriftVelocity`。
- `set_night_roof_charge`：只对黑夜屋顶可用；`phase=CHARGING/WARNING/DISCHARGING`，活动阶段用 `row` 固定路线，可选 `charge/remaining`。
- `weather.nightRoofCharge`：导出 `supported/chargePct/overchargePct/phase/row/phaseRemainingMs/dischargeProgressPct`；`set_night_roof_charge` 可用 `overcharge` 固定活动阶段余电，积累阶段会强制归零。
- `roofResources.rainBackgroundLoaded`：白天屋顶雨景变体已按 `IMAGE_BACKGROUND_ROOF_RAIN` 完成注册与加载。
- `roofResources.nightRainBackgroundLoaded`：夜间屋顶雨景变体已按 `IMAGE_BACKGROUND_NIGHTROOF_RAIN` 完成注册与加载。
- `weather.roofRainBackgroundAlpha`：昼夜屋顶雨景变体的整数 alpha；两者当前晴/小/中/大雨均为 `0/96/149/255`，非屋顶恒为 `0`。
- `set_weather_forecast`：固定公开/真实天气和揭晓时刻；公开预报或真实下一天气涉及大雨时可用 `typhoonStrength` 固定共享的警报/待生效等级。`weather.forecastDisplayText` 导出面板实际公开文案，须成对断言误报大雨显示诱饵台风、非大雨预报隐藏真实台风。
- `advance_weather_phase`：用权重落点强制结束雨段，并立即完成过渡。
- `trigger_lightning`：只允许大雨；普通大雨同步播放 `SOUND_THUNDER` 并生成程序化主干/分叉；暴风雨夜改走 C# 风格全屏短闪，且不得同时激活普通局部闪电。
- `set_fog_weather`：固定当前合格迷雾关卡的 `DEFAULT/SMALL/NORMAL/DENSE` 雾势与持续时间。
- `set_fog_forecast`：固定公开/真实雾势与揭晓时刻；当前雾势预报保持准确。
- `set_fog_dispersal`：固定 `0..1` 驱散比例，供存档与渲染状态测试。
- `set_plantern_gear` / `set_plantern_fuel` / `award_plantern_fuel`：固定挡位和燃料边界。
- `assert_can_target`：直接断言统一雾中索敌入口；远目标和近身例外应成对覆盖。
- `set_zombie_mist_fuel_reward` / `kill_zombie`：绕开随机分配，只验证正式死亡发起；先断言
  `pendingFuelTenths` 增加且实际燃料不变，再等待飞行结束断言到账。满仓、部分溢出、在途读档、
  魅惑和无路灯花丢弃也在此覆盖；概率/预算另用正式波次路径验证。

为雨天能力新增脚本时：

1. 晴天生成/使用能力，断言中性结果。
2. 固定目标雨势，生成新实体或触发技能，断言增强值。
3. 若能力跟随天气，切回晴天断言还原；若出生永久变异，断言它不还原。
4. 用减速和冻结覆盖 Animator 组合；有魅惑交互时同时覆盖立场。
5. 随机变异用 `-Seed` 固定，并在 dump 暴露实际类型/标志；测试 resolver 的 0%、命中和不符合雨势分支。
6. 需要自然过渡时不要依赖 `set_weather`，改用短倒计时的正式切档路径。
7. 检查退出码、`run.log`、状态 JSON、断言和必要截图。

普通玩家存档在 AutoTest 中仍默认短路；`save_level_snapshot` / `reload_level_snapshot`
会在脚本输出目录内走正式序列化，销毁旧场景并创建新的 `GameScene` 反序列化，可验证
schema、Board 天气和 UI 计时的进程内往返。它不覆盖中央真实存档路径、跨进程退出重进
或历史版本迁移；这些仍须按风险用 `SaveSchemaTests`、只读问题档或源码审计补充。
迷雾首帧测试应在保存前把 `timeScale` 设为 0，重载后不等待任何帧就断言 `visibleCells`、
`maxAlpha` 和代表性 `columnMaxAlpha`，再同步截图；这样旧实现的全零 alpha 会确定性失败。
