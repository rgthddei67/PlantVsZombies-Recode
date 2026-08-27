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

## 2026-08-27 外壳啃食与非占格爆炸表现

玩家复现了已出土地雷被南瓜保护时，多只僵尸啃掉外壳后会继续把地雷啃掉。根因是武装态仍主要依赖 collider enter 和出土瞬间的 `mEaterCount`；僵尸从已销毁的南瓜重新选中内层地雷时不会产生新的 enter。现在武装态每个正式逻辑步按爆炸圆持续扫描，同步保留 `OnZombieBite` 的提交前兜底；命中后先置 `mIsBoom`，因此触发的这一口不会继续伤害地雷。

C# `DoSpecial` 在范围伤害、粒子和震屏后立即 `Die()`。本项目原先把植物切到 `anim_mashed` 保留 2 秒，导致 Cell 仍被占用。现在正式结算后立即死亡，并以原版 `PotatoMine_particles.png`、`PotatoMineFlash.png` 和本地化 `ExplosionSpudow` 组合成 `PotatoMine` 粒子；压扁主体仍显示 2 秒，但不再参与格位、啃食或承伤。旧档若含已经爆炸但仍存活的地雷，加载时直接结束实体。

`clang-release` 构建通过且 Win7 导入审计通过。默认 Vulkan 可见 `smoke_potatomine.json` 覆盖先啃后出土、四只僵尸啃南瓜、爆炸后原格立即可种、南瓜存活时同格种向日葵、粒子资源键、43 个 quad 与 300px 包围盒下界；exit 0，状态、日志和截图均核对通过。
