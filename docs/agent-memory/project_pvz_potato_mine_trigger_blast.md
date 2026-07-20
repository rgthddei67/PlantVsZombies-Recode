---
name: project_pvz_potato_mine_trigger_blast
description: 土豆地雷先啃后出土不爆与单目标爆炸修复；对齐原版同排半径 60 范围结算
metadata:
  node_type: memory
  type: project
---

# 土豆地雷出土触发与范围爆炸

2026-07-20 修复两个同源缺陷：当前 `PotatoMine` 只在 `onCollisionEnter` 中检查 `mIsRise`，僵尸若在埋地阶段已经进入碰撞并开始啃食，出土后不会再次收到 enter；同一回调又只对触发者执行 `Die()`，因此贴近的第二只僵尸不会受爆炸影响。

原版 C# `UpdatePotato` 在 `PotatoArmed` 态持续 `FindTargetZombie`，`DoSpecial` 再调用 `KillAllZombiesInRadius(row, centerX, centerY, 60, 0, ...)`。本项目保持既有“命中即死”和压扁动画时序，只对齐两项关键契约：

- `mReadyTimer` 跨过 20 秒并进入出土态时，若 `mEaterCount > 0`，主动调用统一的 `Detonate()`，补回已存在碰撞对不会再触发 enter 的缺口。
- `Detonate()` 先设置 `mIsBoom` 防重入，再用同排行索引遍历僵尸；以爆心 `GetPosition()+(-20,-10)` 和半径 60 做圆×当前僵尸 collider 矩形判定，一次清除全部非魅惑、非濒死目标。遍历期间 `Zombie::Die()` 只延迟销毁对象，不会使行桶迭代器失效，并会平衡正在啃食本植物的 `mEaterCount`。

回归脚本 `autotest/scripts/smoke_potatomine.json` 分两段：第一段在 19 秒时让普通僵尸开啃，先断言 `anim_eat`，再断言出土后地雷为 `anim_mashed` 且目标消失；第二段让地雷先出土，再生成两只相邻僵尸，断言从“自然波对照 1 + 测试目标 2”回到只剩远端自然波对照 1。`clang-release` 零警告，可见桌面运行标题“植物大战僵尸中文版”、退出码 0；`run.log`、两份状态 JSON 与四张截图均核对通过。
