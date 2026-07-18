# 生存模式每轮两次词条选择 Implementation Plan

**Goal:** 把轮间词条阶段从最多 1 次改为最多 2 次，每次仍同时获得一个植物增益与一个僵尸诅咒。

## Task 1：先写失败的 UI 状态断言

- 更新 `smoke_perk_select.json`，要求 `perkSelect.maxPicks==2`、第一次选择后仍激活并进入第二次。
- 用当前二进制运行，确认因新字段不存在而失败。

## Task 2：拆分初始化与单步渲染

- `GameScene.h` 增加每轮上限常量、完成计数、AutoTest getter 和私有 `RenderSurvivalPerkSelectStep()`。
- `BeginSurvivalPerkSelect()` 只初始化状态并调用单步渲染。
- 原弹窗构建主体迁入 `RenderSurvivalPerkSelectStep()`，标题显示第 N/2 次。

## Task 3：实现第二次选择和统一关闭

- 所有弹窗按钮设 `autoClose=false`。
- `ApplyPerkSelection()` 先应用合法配对，再统一关闭当前框。
- 未满两次则重新 roll 并渲染；主动结束或达到上限才解除暂停并进入选卡。

## Task 4：AutoTest、记录与技能

- `dump_state.perkSelect` 增加 offerCount/currentPick/completedPicks/maxPicks。
- 覆盖选满两次与选一次后结束两条路径。
- 更新 `project_pvz_perk_system.md`、索引摘要以及 `adding-survival-perk/SKILL.md` 的当前词条清单、两次选择契约和测试方法。

## Task 5：构建与回归

- 运行 `clang-release` 零警告构建。
- 运行 `smoke_perk_select.json`、`smoke_perk_view.json`、`smoke_perks_balance.json` 和 `smoke_perks.json`。
- 检查两次选择截图、diff 与工作树后提交；满足 fast-forward 条件时 push。
