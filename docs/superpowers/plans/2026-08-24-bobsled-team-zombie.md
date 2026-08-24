# 雪橇车队僵尸实现计划

> 2026-08-24 已完成实现、Release 双渲染可见专项、雪锚果回归、存档测试与技能审计；最终提交信息见 Git 历史。

## 1. 原版资源闭环

- 新增带 SHA-256 清单的 `scripts/import_bobsled_team_assets.ps1`。
- 导入 `Zombie_bobsled.reanim`、19 张 reanim 部件图、5 张雪橇图和 `ZombieBobsledHead.png`。
- 新增断头/断臂掉落 XML；接入 `resources.xml`、manifest、`ResourceKeys` 与 AnimationType。

## 2. 独立僵尸与编队状态机

- 新增 `BobsledTeamZombie.h/.cpp`、枚举、工厂和 gamedata。
- 实现队长首更新生三名跟随者、稳定 ID 交叉引用和预览路径。
- 实现 `RIDING → LANDING → WALKING`、冻融线、车辆耐久、滑行速度和动态控制门禁。

## 3. 雪锚果通用交互

- 车辆碰撞只调用 `ResolveWinterGroundImpact(COLLISION)`。
- 同帧结算 1200 僵尸伤害和约束/未约束落点。
- 实现队伍共命、边缘行折回与无冻土安全下车。

## 4. 原版动画和部件表现

- 注册主人指定的 133/151/169 帧事件。
- 复刻车体三档损坏、坠毁淡出和槽位前后层。
- 落地后实现专属断头、断臂换图、粒子和读档外观恢复。

## 5. 编排、存档与诊断

- 追加 7-2/7-3/7-5/7-7/7-8/7-9 出怪和每波一队计数存档。
- 保存每名成员的角色、阶段、团队 ID 与落地计时。
- 扩展 TestDriver 的名称表、资源状态、车队状态和最小测试命令。

## 6. 验证和交付

- `clang-debug` 增量编译并运行范围最小的专项 AutoTest。
- 整体配置/编译 `clang-release`，运行资源、存档、7-2 编排以及默认实例/`-NoInstance` 可见专项。
- 审计 adding-zombie、adding-plant、adding-rain-weather、adding-particle 技能与 references；同步可复用契约并用 skill-creator 校验。
- 更新项目记忆，提交，并在仓库与上游允许常规 fast-forward 时推送。
