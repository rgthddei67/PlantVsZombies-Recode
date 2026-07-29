# 雨天天气扩展契约

本文件记录截至 2026-07-29 的当前实现。动手前用文中的搜索词核实源码；当前代码优先于本文件。

## 目录

- [天气状态与时间语义](#天气状态与时间语义)
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

离散能力默认这样判断：

```cpp
const RainIntensity rain = mBoard ? mBoard->GetRainIntensity() : RainIntensity::CLEAR;
const bool isRaining = rain != RainIntensity::CLEAR;
```

不要用 overlay alpha 推断是否下雨：它是视觉插值，雨转晴的两秒内仍大于 0。

## 源码钟点

| 目的 | 当前位置 / 搜索词 | 约束 |
|---|---|---|
| 权重、持续时间、倍率 | `Board.cpp` 匿名命名空间 `k*Rain*` | 调参常量同行中文注释 |
| 随机下一天气 | `Board::RollNextWeather` / `RainTransitionForRoll` | 与合法预报候选保持同构 |
| 合法公开预报 | `BuildPlausibleForecasts` | 错误预报也必须真实可达 |
| 正式切档 | `BeginRain` / `EndRain` / `BeginWeatherTransition` | 目标枚举先变，倍率再插值 |
| 天气逐帧推进 | `Board::UpdateWeather` | 全局场景状态，不属于波次更新 |
| 僵尸天气动画倍率 | `Zombie::UpdateAnimSpeed` | 冻结 > ability × 减速 × rain |
| 植物天气行动倍率 | `Plant` 的 weather action helper | 不改变全局 delta |
| 正式波次选型/生成 | `Board::TrySummonZombie` | 出生变异的默认接入点 |
| 通用创建 | `Board::CreateZombie` | AutoTest、召唤等也可能调用，不默认随机变异 |
| 读档创建 | `Board::CreateZombieWithID` | 只还原已保存类型，永不重 roll |
| 预览僵尸 | `Board::CreatePreviewZombies` | 使用基础出怪表，默认不展示临时天气变异 |
| 天气玩法存档 | `GameInfoSaver.cpp` 搜索 `rainIntensity` | `Board` 天气先恢复，再加载实体 |
| 天气 UI 请求 | `BoardPresentation.h` / `GameScene` 实现 | `Board` 不包含具体 `GameScene`，也不持有 UI 计时 |
| 天气 UI 存档 | `CaptureWeatherPresentationState` / `RestoreWeatherPresentationState` | 经展示端口保存可重建的视觉瞬态，不得影响玩法 |
| 存档版本升级 | `SaveSchema::UpgradeLevelDocument` | 升级成功后才允许修改 `Board` 或实体 |
| 天气 AutoTest 状态 | `TestDriver.cpp` 搜索 `out["weather"]` | 浮点另给整数投影；闪电路径暴露激活、主干/分叉段数与落点 X |

## 雨天专属能力配方

### 即时伤害、范围或抗性

在真正结算的唯一函数读取天气并选择倍率，不要永久改写基础字段。例如攻击伤害在命中时算；范围在技能释放时算。若伤害要与生存词条组合，明确乘算顺序，并继续传正确的 `DamageSource`。

植物若要抵抗强/超强台风的整格位移，让植物基类提供声明式“锚定格位”和直接撞击回调，
由 `Board::TriggerTyphoonPlantMove` 在既有前缘到后缘、逐格循环中调用：

- 源格任一活跃层声明锚定时，整个上下层组合留在原格；不要把承载层与上层植物拆开。
- 目标格取顶层锚定植物；只有来袭组合直接尝试进入该格才派发撞击，后方被普通植物占格阻挡
  不把压力继续传导。
- 每一格阻挡立即结算伤害，使锚定植物死亡后下一步能够重新读取腾出的 Cell；若同一阵风
  多步撞击需要减少噪声，只合并音效/粒子，不合并伤害。
- 这类能力由当前品种、生命与 Cell 状态派生，无需保存实体标志；最近直接阻挡格次可作为
  瞬态 AutoTest 观测值。测试同时覆盖双向、紧邻多步、间隔移动、连续链、水路组合和中途死亡。

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
- 天气 UI 的计时与展示状态经 `BoardPresentation` 捕获/恢复；它们是可重建瞬态，不能反向成为天气玩法权威。

## AutoTest 契约

现有命令：

- `set_weather`：固定天气并立即完成过渡；可传 `duration`、小雨的 `canIntensify`。
- `set_weather_forecast`：固定公开/真实天气和揭晓时刻。
- `advance_weather_phase`：用权重落点强制结束雨段，并立即完成过渡。
- `trigger_lightning`：只允许大雨；生成固定到本次放电结束的程序化主干与分叉，不复用寒冰菇的全屏白闪。

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
