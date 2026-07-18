# 生存词条“放弃本次”设计

- 日期：2026-07-18
- 前置：每轮已有两次独立的正负词条配对机会。
- 本文取代双选择设计中“任一步结束都会放弃整轮剩余选择”的规则。

## 目标行为

每轮固定提供两次选择机会，每次都可以“选择一对”或“放弃本次”：

1. 第一次放弃只消耗第 1 次机会，重新随机并进入第 2/2 次。
2. 第二次放弃才结束词条阶段；两次都放弃时本轮获得 0 对词条。
3. 第一次放弃、第二次选择时，本轮获得 1 对词条。
4. 两次都选择时，本轮获得 2 对词条。

按钮文字由“结束选择”改为“放弃本次”，避免界面暗示会直接结束整轮。

## 状态模型

原来的 `mSurvivalPerkPicksCompleted` 只能表示实际获得的配对数，不能再兼任 UI 步骤。新增：

- `mSurvivalPerkStepsCompleted`：已经消耗的选择机会数，范围 0~2；选择或放弃都会加 1。
- `mSurvivalPerkPicksCompleted`：实际选择并应用的配对数，范围 0~2；只有合法选择才加 1。

当前标题和是否进入下一步由 `stepsCompleted` 决定；最终统计由 `picksCompleted` 决定。两者都只是轮间临时状态，不写入存档。

## `ApplyPerkSelection` 流程

1. 合法 index 时应用植物与僵尸词条，并增加 `picksCompleted`。
2. 无论选择还是放弃，都增加 `stepsCompleted`。
3. 先把当前消息框设为 inactive，再调用延迟 `Close()`，避免下一步新框出现时旧框仍绘制一帧。
4. 若 `stepsCompleted < 2`，重新 roll 3 项并渲染下一步。
5. 否则清空候选、解除暂停并进入选卡。

## AutoTest

`dump_state.perkSelect` 增加 `completedSteps`，保留 `completedPicks` 分别表达机会数与实际获得数。

- `smoke_perk_select.json`：两次都选，最终 steps=2、picks=2。
- `smoke_perk_select_skip_all.json`：第一次放弃后仍激活并进入第 2/2 次；第二次再放弃后 steps=2、picks=0。
- `smoke_perk_select_skip_then_pick.json`：第一次放弃、第二次选择，最终 steps=2、picks=1。
