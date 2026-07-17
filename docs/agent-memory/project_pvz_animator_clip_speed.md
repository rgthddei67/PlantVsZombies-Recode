---
name: project_pvz_animator_clip_speed
description: Animator 三层速度模型重构（clip 绝对覆盖层替代 mOriginalSpeed/RestoreSpeed），已合并推送 master e74bc76
metadata:
  node_type: memory
  type: project
  originSessionId: 6e0975f4-4d4f-4af2-9f12-22059935687f
---

2026-06-07 完成并已 push（master `e74bc76`，fast-forward）。重构 Animator 速度系统，消除主人吐槽的 `mOriginalSpeed` + 全局 `RestoreSpeed` 两步舞。

**核心模型**——有效播放速度统一为一个内联助手：
```
EffectiveSpeed() = (mClipSpeed != 0 ? mClipSpeed : mSpeed) * mExtraSpeedMultiplier
```
三层职责分离：
- `mSpeed`（base，每实例随机走速）：**PlayTrack 永不写它**，只有 `SetSpeed`/`SetAnimationSpeed` 写（init、Polevaulter 跳后重设）。
- `mClipSpeed`（当前轨道**绝对速度覆盖**，0=回落 base）：每次 `PlayTrack(speed)` 重设；`speed>0`→clip=speed，`speed==0`→clip=0。切轨道自动失效。`SetClipSpeed` **递归到附加子动画**（照抄旧 SetSpeed 递归，保证铁桶/报纸/头部跟随）。
- `mExtraSpeedMultiplier`（正交状态层，减速/冻结）：跨 PlayTrack 存活，本次未动。

**关键设计抉择**：clip 是「**绝对覆盖**」不是乘数。乘数会让 eat=2.1 变成 2.1×base 还逐实例抖动，破坏手感；绝对覆盖 + 0=回落 base 才能与改动前一字不差（最初我向主人提的是乘数版，自查调用点数值后主动改正——见 [feedback_collaboration_style](feedback_collaboration_style.md) 的 honest framing）。`0.0` 哨兵被复用成「无覆盖」信号，与既有约定自洽。

**连带改动**：删 `mOriginalSpeed` 成员 + `RestoreSpeed`（6 处 `PlayTrack(0.0)+RestoreSpeed` 两步舞清零）；`Polevaulter::EndJump` 跳后降速改 `SetAnimationSpeed` 重设 base；`WallNut` 被啃暂停改 `SetExtraSpeedMultiplier(0)/(1)`；存档 `animOriginalSpeed→animClipSpeed`（读档序 PlayTrack→SetAnimationSpeed→SetClipSpeed，旧档缺字段默认回落 base）；`GetTrackVelocity` 同步走 EffectiveSpeed。

**Why**：未来再碰动画速度别重新推一遍；改 PlayTrack/加状态效果时记得是「clip 绝对、base 受保护、extra 正交」三层。
**How to apply**：想临时改某轨道速度→PlayTrack 带 speed（绝对）；想改单位长期走速→SetAnimationSpeed（写 base）；想加减速/冻结类状态→SetExtraSpeedMultiplier（别碰 base/clip）。新增会附加子动画的轨道，clip 已自动递归无需额外维护。

**执行记录**：subagent-driven，1 实现代理落地 9 文件 exact edits + 我做 spec/质量双审；本仓库无测试框架、禁 MCP build，编译+实机验证由主人在 VS 完成。PaperZombie.cpp/.h 当时是主人未提交 WIP（重构只删其 2 处 RestoreSpeed，无法与功能拆分），故**未纳入重构 commit**，留给主人随报纸僵尸功能单独提交。计划文档：`docs/superpowers/plans/2026-06-07-animator-clip-speed-refactor.md`（已入库）。
