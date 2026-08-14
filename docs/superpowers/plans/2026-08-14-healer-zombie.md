# 急救员僵尸实施计划

> 对应规格：`docs/superpowers/specs/2026-08-14-healer-zombie-design.md`

## Task 1：类型、工厂与状态机

- 在枚举尾部追加 `ZOMBIE_HEALER`，同步 GameDataManager、开发者生成表、AutoTest 名称表和 gamedata 五字段。
- 新建 `HealerZombie : Zombie`，实现 800 本体生命、普通移动/啃咬与 `IDLE/AREA/FOCUSED/DISABLED` 状态。
- 复用普通僵尸 reanim 和既有啃咬/死亡帧事件，不新增帧事件。

## Task 2：治疗决策、预留与中断

- 集中声明 5 秒冷却、1 秒前摇、0.5 秒重试、140/280 像素范围、3 人阈值和 100/400 治疗量。
- 按同阵营、现存生命层和最低剩余比例选择目标；锁定劫持者拥有单疗最高优先级。
- 给 EntityManager 增加按 ID 有序的急救员弱索引，用于单疗预留与同时就绪时的稳定 ID 顺序门禁；高成本推演另由 Board 固定逻辑步预算分摊。
- 实现魅惑、死亡、断头、难度分支断臂与预览的原子取消/禁疗规则。

## Task 3：啃食恢复与护具可逆显示

- 施法开始时保存并停止啃食，结算后严格复核原植物/僵尸目标再恢复。
- 治疗本体、现存头盔和现存盾牌，排除 extra health 和已销毁装备。
- 把各防具 `CheckHelmImage` / `CheckShieldImage` 修正为由当前耐久重新派生损伤阶段，保证治疗跨阈值后外观能够复原。

## Task 4：资产、粒子与声音

- 用可复现 PowerShell/System.Drawing 脚本生成灰白背心、四态机械急救标志、绿色加号和浅绿光环。
- 注册身体/标志纹理、粒子纹理和 `HealerAreaHeal` / `HealerFocusedHeal` XML；两种效果持续 0.9 秒并在结算点取样。
- 接入每施法者一次的开始/结算音效，不按治疗目标数叠加。

## Task 5：出怪、图鉴与存档

- 6-6 出怪池加入急救员，第 3 波额外保底一只且不限制每波数量。
- 权重 2400、`appearWave=3`、`survivalRound=7`；补齐中文图鉴说明与引语。
- 存读状态、各计时器、单疗目标、待恢复啃食目标和永久禁疗标记，读档修复无效组合。

## Task 6：AutoTest 与可见验证

- 增加资源、单疗、群疗、护具、断肢难度、魅惑、啃食恢复、存档和 6-6 出怪脚本及状态投影。
- 默认完成 `clang-release` 配置与构建，从 `build/clang-release` 在当前桌面可见运行最小专项和相关回归。
- 检查进程退出码、`run.log`、状态 JSON、断言和截图；逐张目视核对最终低分辨率资源与游戏合成。

## Task 7：契约审计与交付

- 更新仓库记忆主题及索引；审计 `adding-zombie`、`adding-particle` 和 references，把可复用契约同步回技能。
- 如技能有修改，完整读取 `skill-creator/SKILL.md` 并运行 `quick_validate.py`。
- 审查全部差异，强制加入已复核的忽略资源，验证完成后提交；上游明确且可常规快进时推送。
