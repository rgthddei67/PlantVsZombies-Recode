# PvZ 架构边界与存档版本入口

## 2026-08-29 Board 极夜环境第二批拆分

`Board` 的极夜三仪表导演、真假计划、白毛风阶段、雪穴状态与延迟事务推进、强风偏行、
极夜地面绘制和测试入口已机械迁入 `Game/BoardPolarNight.cpp`。公共接口、成员布局、
Board 唯一玩法权威和关卡 JSON 键保持不变；`CreateOrQueueWaveZombie`、`SummonNextWave`
和通用索敌仍在 `Board.cpp`，分别保留正式波次创建、全局波次推进与跨地图目标查询边界。

开发者直调最终波的即时白毛风初始化收敛为私有窄入口，正常 8-9 大波警告仍走原平滑预热；
没有新增状态或 schema。验证使用当前 `clang-release`：完整构建、Win7 378 项导入审计、
CTest 3/3，以及桌面可见极夜 core 84、dynamics 59、navigation 65、UI 24 条命令均通过。

## 2026-08-29 Board 冬日气候首批拆分

`Board` 的公共接口、成员布局、唯一玩法权威和关卡 JSON 键保持不变；寒潮初始化与推进、
冻融资格、冬季测试入口和霜线绘制实现从 `Board.cpp` 机械迁入
`Game/BoardWinterClimate.cpp`。冬日温区、寒潮权重和阶段时长常量也随唯一消费域迁移，
基础雨势代码通过 `GetWinterFreezingTemperatureC()` 读取冰点，不复制调参值。

这一步只拆翻译单元，不引入第二份天气状态、不改变 `GameInfoSaver`、不提升 schema，
也不把 `CreateBoom`、`GetNormalPlantAt` 等棋盘核心门面移出 `Board`。后续环境子域继续采用
“先保持 `Board::` 接口机械拆文件，再单独评估状态封装”的顺序，禁止一次提交同时改玩法语义。

验证使用当前 `clang-release`：完整构建和 Win7 378 项导入审计通过，CTest 3/3；
桌面可见 `smoke_winter_garden` 143/143、`smoke_winter_opening_cold_wave` 68/68，
退出码、`status.json`、`run.log` 和冬季截图均已核对。

## 2026-07-29 渐进式解耦

`GameScene` 继续用 `unique_ptr` 独占 `Board`，但 `Board` 不再包含或暴露
`GameScene*`。新的 `BoardPresentation` 是非拥有窄端口，只承接图片/文字提示、天气 UI、
闪屏、铲子、胜负转场、生存词条页和波次进度条等展示请求。天气强度、预报锁定、
台风、波次与生存模式仍由 `Board` 唯一持有；场景只保存可重建的 UI 计时瞬态。
`GameInfoSaver` 也经该端口捕获/恢复 UI 瞬态和轮间卡片冷却，不再依赖具体场景类型。
`WeatherTypes.h` 提供双方共享枚举，避免为了几个值重新包含整个 `Board.h`。

`GameProgress` 原有的 `GameScene*` 从未参与行为，现只接收非拥有 `Board*`。
`Board` 的读档标记仍为私有状态，场景通过 `CompleteLoadRestore()` 完成生命周期转换，
不再依赖 `friend GameScene` 修改内部字段。

## 场景生命周期

`Scene::OnExit()` 会清空 UI、粒子和全局 `GameObjectManager`，所以旧
`SceneManager::PushScene/PopScene` 的“保留并恢复旧场景”语义并不成立，而且仓库没有调用方。
`SceneManager` 现明确只持有一个 `unique_ptr<Scene>`；覆盖式界面应由当前场景内的
`UIManager` 管理，`GameMessageBox` 是其直接拥有的普通模态对象，关闭请求在控件遍历后统一清理。若未来真的需要场景栈，必须先把对象管理器和资源清理
改成场景作用域，不能直接恢复旧 API。

## 存档 schema

`PlayerInfo.json` 与关卡 JSON 从 v1 起在根节点写入各自的 `schemaVersion`。
纯逻辑 `SaveSchema` 在反序列化修改 `GameAPP` / `Board` 前事务式升级文档：
缺版本旧档按 v0 补为 v1并保留全部历史字段；未来版本、非法根节点和非法版本号拒绝加载，
且不会留下半迁移 JSON。迁移逻辑由独立 `SaveSchemaTests` 覆盖，后续结构变化必须增加
逐版本步骤和对应纯测试。

2026-08-03 玩家 schema 升至 v2，以兼容毒囊射手奖励由 4-8 前移到 3-8：v0/v1 玩家档
在 `adventureLevel >= 27` 且 `havecards` 尚无该枚举时补发一次；尚未通关3-8不补发，
已经拥有时不重复。迁移仍只在 JSON 副本上执行，成功后才提交；`SaveSchemaTests` 覆盖
26/27 边界、已有卡去重和当前版本不重复执行历史迁移，`save-schema` 1/1 通过。

2026-08-11 玩家 schema 升至 v3，新增 `advancedPauseEnabled`。v2 及更旧玩家档补为
`false`，使普通空格暂停默认禁止卡槽与落种；迁移若遇到已经存在的同名字段则保留玩家
选择。`SaveSchemaTests` 覆盖默认补值、既有 `true` 不被覆盖与其他设置保持不变，
`save-schema` 1/1 通过。

2026-08-12 玩家 schema 升至 v4，新增稳定枚举名数组 `lastSelectedCards`。旧档补空数组，
预发布档已有值不覆盖；读取时只接收字符串并限制数量，具体植物有效性延后到 GameData 已加载的
选卡界面过滤。最近一次正式提交选卡会立即写 PlayerInfo，因此进入战斗后直接退出仍可在下一局恢复。

2026-08-24 玩家 schema 升至 v5，新增冒险关卡号数组 `crazyDaveTutorialsSeen`。v4 及更早
旧档补空数组，预发布档已有值不覆盖；读取时只接收当前冒险流程内的整数并排序去重。戴夫闲聊
完整看完或主动跳过会立即写 PlayerInfo，场景中断则不记录，避免一次意外退出永久跳过教程。

2026-08-25 关卡 schema 升至 v5，为气象干扰僵尸新增雨雪预报、雾势预报的公开干扰标记和
本波气象干扰僵尸计数。v4 及更早关卡档默认补 `false/false/0`，预发布档已有值不覆盖；读取时
只有对应原始预报仍处于公开窗口才恢复干扰标记，避免陈旧标志隐藏下一次新预报。

2026-08-26 关卡 schema 升至 v6，为红眼巨人新增冒险本波接纳计数。v5 及更早关卡档补
`redeyeGargantuarsSpawnedThisWave: 0`，预发布档已有值不覆盖；读取时按 `[0,3]` 夹紧，生存/无尽
明确重置为 0，以保证其不消费冒险上限。

2026-08-26 关卡 schema 升至 v7，新增 `openingColdWavePlanInitialized`。v6 及更早关卡档缺字段时补
`false`，表示 7-8/7-9 仍有一次开幕强寒潮可消费；预发布档已有值不覆盖。恢复后
`Board::InitializeWinterTemperature()` 若发现该两关仍未消费，即把温度状态重建为 +6°C 的 12 秒
预报倒计时，再锁定 15 秒降温、50 秒极寒与 30 秒回暖，因此旧档不会只是保存一个永远不生效的标志。

## 所有权约束

- `GameScene` 独占 `Board`；`BoardPresentation*` 仅为非拥有回调端口。
- 运行对象主要由 `GameObjectManager` 持有；`Board::mCells` 是 `Cell*` 寻址表，
  奖杯与弹坑使用 `weak_ptr`，不得把这些索引误写成所有权。
- `Board` 是天气与关卡玩法状态的唯一权威；展示端口不得建立第二份玩法状态。

## 验证

2026-07-29 `clang-playtest` 完整构建退出码 0；`save-migration` 与新增 `save-schema`
纯测试全部通过。当前桌面可见 `smoke_zombie_ability_speed` 31 条通过，确认 v1 快照写入、
销毁旧场景、创建新 `GameScene` 并恢复 5 个僵尸；`smoke_weather_forecast` 58 条通过；
`smoke_crater_card_select` 33 条通过，覆盖 `Board` 触发生存轮清、两次词条机会、
轮间选卡与恢复。三项退出码均为 0，状态 JSON、run.log 与截图均已检查。

## 技能同步门禁

2026-07-29 已把 `BoardPresentation`、`WeatherTypes`、`SaveSchema` 和默认
`clang-playtest` 约定同步到天气、植物、僵尸与生存词条技能。根 `AGENTS.md` 现要求
每次任务改动完成后、提交前审计相关 `.agents/skills/` 及 references；发现通用契约变化
必须同步技能，并用 skill-creator 的 `quick_validate.py` 校验所有改动过的技能。
