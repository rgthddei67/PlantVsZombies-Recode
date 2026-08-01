# 精英矿工僵尸实现计划

> 日期：2026-08-01
> 依据：`docs/superpowers/specs/2026-08-01-elite-digger-zombie-design.md`

## Task 1：收窄普通矿工继承点

1. 移除 `DiggerZombie final`。
2. 增加持镐步速、眩晕结束、丢镐、装备贴图/粒子窄虚拟钩子。
3. 让普通矿工默认行为保持不变，并修正 AutoTest 对派生类的类型误计数风险。

## Task 2：实现精英逻辑与存档

1. 追加 `ZOMBIE_ELITE_DIGGER` 和动画/资源键，注册 `EliteDiggerZombie`。
2. 实现 600+250 耐久、0.15 px/tick 持镐折返、一次性固定格爆破。
3. 在眩晕结束和丢镐钩子实现结算/取消；保存并恢复 `blastResolved`。

## Task 3：生成独立资源

1. 添加可重复运行且带源/目标 SHA-256 锁的 `recolor_elite_digger.ps1`。
2. 生成独立 reanim、身体/袖子/安全帽/镐和断臂贴图。
3. 新建橙红+青蓝 `EliteDiggerBlast.xml` 及精英掉帽/断臂粒子配置。
4. 用资源加载和图片检查验证键名、透明度、色彩区分与粒子生命周期。

## Task 4：完整接入

1. 更新 `gamedata.json`、`info.txt`、开发者面板和 AutoTest 类型表/状态投影。
2. 在 Board 增加每波 1 只计数、存档恢复、波次与生存轮次重置。
3. 将 ID 26 加入冒险 4-6，并按 4-6 通关边界加入图鉴。

## Task 5：验证

1. 配置、构建 `clang-release`。
2. 可见运行精英逻辑/粒子、波次上限、冒险图鉴测试，并回归普通矿工。
3. 默认实例化和 `-NoInstance` 各取证，检查进程窗口、exit code、日志、状态和截图。

## Task 6：收尾

1. 更新矿工主题记忆与记忆注册表。
2. 审计 `adding-zombie`、`adding-particle` 及引用；只记录可复用的契约变化。
3. 若技能有修改，使用 skill-creator 的 `quick_validate.py` 校验。
4. 核对 diff，提交已验证改动；上游明确且可 fast-forward 时 push。
