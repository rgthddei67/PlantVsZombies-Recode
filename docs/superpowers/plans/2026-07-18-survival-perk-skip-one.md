# 生存词条“放弃本次” Implementation Plan

**Goal:** 把“结束选择”改为只放弃当前一次机会，两次机会分别结算。

## Task 1：失败测试

- 用 `completedSteps` 与 `completedPicks` 分别断言已消耗机会和实际选择数。
- 覆盖两次都放弃、先放弃后选择两条路径，并先用旧二进制确认失败。

## Task 2：拆分状态

- `GameScene` 新增 `mSurvivalPerkStepsCompleted` 及 AutoTest getter。
- 标题进度、当前步骤和流程结束条件改由 steps 驱动。
- 实际应用计数继续由 picks 驱动。

## Task 3：调整 UI 与文档

- 按钮改为“放弃本次”，保持 `autoClose=false` 和 `ApplyPerkSelection` 统一关闭权。
- 更新双选择测试、技能与项目记忆中的旧“结束整轮”语义。

## Task 4：验证与交付

- 获权运行标准 `clang-release` 配置与构建。
- 以主人桌面可见窗口运行三条选择测试及词条查看/数值回归。
- 检查截图、日志、状态 JSON 和差异后提交并推送。
