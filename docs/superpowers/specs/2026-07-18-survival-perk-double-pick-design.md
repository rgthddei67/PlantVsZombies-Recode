# 生存模式每轮两次词条选择设计

- 日期：2026-07-18
- 前置：生存模式当前有 10 个词条（6 个植物增益、4 个僵尸诅咒），每个可选项仍是一个正负配对。
- 本文档取代旧设计中“每轮 3 选 1、最多选择一次”的上层规则；词条数据、效果钟点和存档格式不变。

## 目标规则

每轮清场后、进入选卡前，玩家最多进行两次词条选择：

1. 第一次展示 3 个 `{植物增益, 僵尸诅咒}` 配对。
2. 选择后两侧各增加 1 层，立即重新随机生成第二组 3 个配对。
3. 第二次选择后结束词条阶段，进入选卡。
4. 任一步点击“结束选择”都会放弃本轮剩余次数并直接进入选卡，因此每轮实际可选 0、1 或 2 次。
5. 两次之间不去重；同一配对可再次出现并继续叠层。已满层词条仍由 `AvailablePerks` 自动排除。

## 状态与流程

`GameScene` 新增：

- `SURVIVAL_PERK_PICKS_PER_ROUND = 2`：本轮最多选择次数。
- `mSurvivalPerkPicksCompleted`：本轮已成功选择的配对数，`BeginSurvivalPerkSelect` 时清零。
- `RenderSurvivalPerkSelectStep()`：只负责重新 roll offer 和构建当前第 N/2 次弹窗。

`BeginSurvivalPerkSelect()` 初始化一次流程并暂停游戏；`ApplyPerkSelection(index)` 负责：

- 合法 index：应用正负两侧，已完成数 +1；未到 2 次则重建下一步弹窗。
- 负数或越界：视为“结束选择”，不增加层数。
- 达到 2 次或主动结束：清空 offer、解除暂停并调用 `BeginSurvivalCardSelect()`。

## 弹窗生命周期

旧实现依靠按钮 `autoClose=true` 在回调结束后关闭框，但 AutoTest 直接调用 `ApplyPerkSelection`，不会触发该自动关闭。两次选择会使真实点击与 AutoTest 的生命周期不同。

新实现让所有选择按钮与“结束选择”按钮使用 `autoClose=false`，并由 `ApplyPerkSelection` 统一调用当前 `mPerkSelectBox->Close()`。关闭仍是帧末延迟销毁；方法可以在同一回调内立即创建第二个框，旧框析构时只移除自己的按钮，不影响新框。

## UI

- 标题追加进度：`第 N 轮 · 选择强化（第 1/2 次）`。
- 底部按钮由“跳过本轮”改为“结束选择”。
- 其余三组配对、颜色、内容自适应尺寸均保持不变。

## AutoTest

`dump_state.perkSelect` 新增：

- `offerCount`
- `currentPick`：激活时为 1 或 2，未激活时为 0。
- `completedPicks`
- `maxPicks`：恒为 2。

`smoke_perk_select.json` 验证：打开时为第 1/2 次；第一次选后仍激活且进入第 2/2 次；第二次选后关闭且 `completedPicks==2`。另加结束选择路径，确认第一次选后主动结束得到 `completedPicks==1`。

## 存档

不新增存档字段。两次配对的词条层数都在进入 `BeginSurvivalCardSelect` 前写进 manager，既有延后一帧存档会一次保存最终层数。选择进度是临时 UI 状态，读档不会落在词条弹窗中。
