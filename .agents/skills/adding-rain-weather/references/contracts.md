# 雨天天气扩展契约

本文件记录 2026-07-20 的当前实现。动手前用文中的搜索词核实源码；当前代码优先于本文件。

## 目录

- [天气状态与时间语义](#天气状态与时间语义)
- [源码钟点](#源码钟点)
- [雨天专属能力配方](#雨天专属能力配方)
- [雨天出生变异配方](#雨天出生变异配方)
- [可逆形态配方](#可逆形态配方)
- [存档契约](#存档契约)
- [AutoTest 契约](#autotest-契约)

## 天气状态与时间语义

`RainIntensity` 有 `CLEAR/LIGHT/MEDIUM/HEAVY` 四档。`Board` 是唯一权威：

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
| 僵尸天气动画倍率 | `Zombie::UpdateAnimSpeed` | 冻结 > 减速 × `mExtraSpeed` × rain |
| 植物天气行动倍率 | `Plant` 的 weather action helper | 不改变全局 delta |
| 正式波次选型/生成 | `Board::TrySummonZombie` | 出生变异的默认接入点 |
| 通用创建 | `Board::CreateZombie` | AutoTest、召唤等也可能调用，不默认随机变异 |
| 读档创建 | `Board::CreateZombieWithID` | 只还原已保存类型，永不重 roll |
| 预览僵尸 | `Board::CreatePreviewZombies` | 使用基础出怪表，默认不展示临时天气变异 |
| 天气存档 | `GameInfoSaver.cpp` 搜索 `rainIntensity` | Board 天气先恢复，再加载实体 |
| 天气 AutoTest 状态 | `TestDriver.cpp` 搜索 `out["weather"]` | 浮点另给整数投影 |

## 雨天专属能力配方

### 即时伤害、范围或抗性

在真正结算的唯一函数读取天气并选择倍率，不要永久改写基础字段。例如攻击伤害在命中时算；范围在技能释放时算。若伤害要与生存词条组合，明确乘算顺序，并继续传正确的 `DamageSource`。

### 技能冷却或蓄力

只缩放该能力自己的计时：

- 想随两秒天气过渡平滑增强，可使用 `GetZombieRainSpeedMultiplier()` 或新增语义明确的天气能力 getter。
- 想在雨开始瞬间切换，按 `GetRainIntensity()` 的命名档位选择倍率。
- 能力由动画帧事件触发时，雨天已经通过 Animator extra 层让事件更早到达；不要再次缩短同一轮动画的逻辑计时。
- 不要把倍率乘到整个 `ZombieUpdate(scaledDelta)`，否则移动、状态机和所有子技能一起被意外加速。

### 动画与移动

子类改变基础 `mExtraSpeed` 后调用 `UpdateAnimSpeed()`。不要直接设置 Animator extra 倍率，否则会覆盖冻结停格或丢失减速/雨天组合。`PlayTrack` 的 clip speed 是另一层，允许换轨但不能替代状态层倍率。

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
- 通用僵尸核心状态优先 `SaveProtectedData/LoadProtectedData`；某个派生类独有状态用其 `SaveExtraData/LoadExtraData`，不要让另一个类解释该字段。
- 若修改 Board 天气未来行为，保存所有会影响下一次抽取的资格和计时；瞬态粒子、水花不存档。

## AutoTest 契约

现有命令：

- `set_weather`：固定天气并立即完成过渡；可传 `duration`、小雨的 `canIntensify`。
- `set_weather_forecast`：固定公开/真实天气和揭晓时刻。
- `advance_weather_phase`：用权重落点强制结束雨段，并立即完成过渡。
- `trigger_lightning`：只允许大雨。

为雨天能力新增脚本时：

1. 晴天生成/使用能力，断言中性结果。
2. 固定目标雨势，生成新实体或触发技能，断言增强值。
3. 若能力跟随天气，切回晴天断言还原；若出生永久变异，断言它不还原。
4. 用减速和冻结覆盖 Animator 组合；有魅惑交互时同时覆盖立场。
5. 随机变异用 `-Seed` 固定，并在 dump 暴露实际类型/标志；测试 resolver 的 0%、命中和不符合雨势分支。
6. 需要自然过渡时不要依赖 `set_weather`，改用短倒计时的正式切档路径。
7. 检查退出码、`run.log`、状态 JSON、断言和必要截图。

标准 AutoTest 默认短路存档读写。存档契约应靠专门的只读问题档测试或源码审计补充，不能声称普通 smoke 已覆盖真实保存/读取。
