---
name: adding-rain-weather
description: Use when adding or tuning ANY rain-weather-dependent feature in PvZ — 雨天专属植物/僵尸能力、精英僵尸雨势强化、仅在雨天发生的变异或条件生成、天气状态机/权重/过渡/预报/UI/存档，以及其他系统接入 Board 小雨/中雨/大雨状态。
---

# 给雨天天气增加功能

雨天状态由 `Board` 统一持有。优先在效果的唯一结算点读取天气，不要让每个实体复制一份可由 `Board` 推导的雨势状态；只有“随机结果只判定一次”“进入/离开雨天触发一次”或“可逆形态”才新增实体字段。

开始前先读仓库根目录 `AGENTS.md`，并按任务范围阅读 `docs/agent-guide/PROJECT_GUIDE.md`。必须完整阅读 [references/contracts.md](references/contracts.md)，再以当前源码核实其中的路径和行为；历史文档只作上下文。

若任务同时涉及以下内容，必须一并使用对应技能：

- 新僵尸或僵尸类型变体：`adding-zombie`。
- 新植物或植物类型变体：`adding-plant`。
- 任意粒子效果新增/调参：`adding-particle`。
- 生存模式词条：`adding-survival-perk`。

新增动画帧事件前，按仓库规则先询问主人。

## 先判断功能原型

| 原型 | 状态归属 | 典型需求 | 首选接入方式 |
|---|---|---|---|
| A. 即时派生效果 | 不新增状态 | 雨天伤害、技能范围、抗性、冷却倍率 | 在攻击/技能结算点读取 `Board::GetRainIntensity()` |
| B. 平滑倍率 | 不新增状态 | 技能频率随雨势渐变 | 复用天气插值 getter，只缩放该技能自己的计时 |
| C. 出生时变异 | 实际类型或一次性标志 | 雨天有概率把基础僵尸替换为精英变体 | 在正式波次选型后、创建前只 roll 一次 |
| D. 可逆形态 | 每实体状态 | 下雨变异、放晴还原 | 幂等 Apply/Revert + 边沿检测 + Save/Load |
| E. 天气系统扩展 | `Board` / `GameScene` | 新权重、过渡、预报规则或 UI | 同步真实抽取、合法预报候选、存档与测试 |

能用 A/B 就不要选 D。可逆血量尤其容易造成反复下雨回血或舍入漂移，优先改伤害、技能冷却、范围或临时护盾；确需改血量时先定义当前血量如何映射。

## 实现流程

1. 明确语义：哪些雨势生效、天气开始/结束的边界、效果是永久还是可逆、是否受两秒过渡影响、随机判定发生几次。
2. 选择唯一钟点：攻击时结算伤害，技能计时器只推进自身，正式波次变异只在 `TrySummonZombie()` 的选型结果上解析。
3. 保持天气源唯一：离散条件读取目标雨势；平滑效果读取插值倍率。不要直接读取私有过渡计时器或复制整套天气状态机。
4. 处理组合关系：冻结仍优先停格，减速与雨天倍率继续乘算，`PlayTrack` clip speed 不得覆盖天气 extra 层；帧事件能力已经随动画加速，不要再把同一倍率乘到逻辑计时器。
5. 处理持久化：纯派生效果不存；一次性 roll、剩余次数、可逆形态必须存。读档只恢复结果，不能重新随机或再次叠加。
6. 增加 AutoTest 可观测字段与最小脚本，覆盖晴天、目标雨势、放晴、减速/冻结组合，以及随机变异的固定种子结果。
7. 更新 `docs/agent-memory/project_pvz_night_rain_weather.md`、对应僵尸/植物主题和 `docs/agent-memory/MEMORY.md`。
8. 按项目指南完成 `clang-release` 与范围最小的可见 AutoTest；仅改技能文档时无需构建游戏。

## 不可破坏的契约

- `GetRainIntensity()` 是目标档位，切档时立即改变；玩法倍率、暗幕和雨声音量再用两游戏秒平滑到目标。离散触发默认以目标档位为准。
- 不要修改全局 `DeltaTime`，也不要整体加速 `Zombie::Update()`；只缩放明确属于该能力的计时或结算值。
- 僵尸动画速度统一经 `Zombie::UpdateAnimSpeed()` 收敛：`mExtraSpeed × slowFactor × rain`，冻结优先为 0。禁止用 `SetExtraSpeedMultiplier` 绕开它。
- 雨天出生变异不要塞进通用 `CreateZombie()`：该入口也被 AutoTest 显式生成和其他玩法调用。默认在 `TrySummonZombie()` 中对 `selected` 解析为实际类型；`CreateZombieWithID()` 永不重新变异。
- 不要按天气改写持久的 `mSpawnZombieList`。生存模式会保存该列表，预览僵尸也使用它；天气变异应是波次生成时的一次性解析。
- 随机变异只 roll 一次，并把实际类型或结果标志保存。禁止在 `Update()`、绘制、读档或每次技能检查中重 roll。
- 若改动真实天气转移，必须同步 `BuildPlausibleForecasts()`；公开错误预报只能来自真实可达候选，无候选时强制报准。
- 新增可调权重、概率、持续时间和倍率时，集中放在 `Board.cpp` 匿名命名空间，并在声明行末写中文用途/单位注释。

## 验证清单

- `set_weather CLEAR/LIGHT/MEDIUM/HEAVY` 固定最终档位；该测试入口会立即完成过渡，适合精确数值断言。
- 需要验证自然两秒过渡时，用短天气倒计时触发正式切档，再断言 `previousIntensity`、`transitionOn` 和结束状态。
- 出生变异先固定天气，再走专用生产入口或新增一个只调用正式解析器的 AutoTest 命令；不要用通用 `spawn_zombie` 冒充波次随机生成。
- 实体 dump 至少暴露实际类型、变异标志、技能计时/次数或可精确断言的整数投影。
- 至少验证：白天/晴天 no-op、目标雨势生效、放晴后的语义、读档不重 roll、魅惑立场、减速/冻结、暂停与倍速。
- 视觉或 UI 改动检查截图；同时检查退出码、`run.log` 的 `script finished OK`、状态 JSON 和断言。

## 关联资料

- [references/contracts.md](references/contracts.md)：当前天气接口、源码钟点、雨天能力与变异配方、存档和 AutoTest 细节。
- `docs/agent-memory/project_pvz_night_rain_weather.md`
- `docs/superpowers/specs/2026-07-20-night-rain-weather-design.md`
