---
name: feedback-frame-event-numbering
description: 帧事件帧号口径：AddFrameEvent真实帧号=预览帧号-1；主人报的帧号默认已-1过，代码直接用不许再减
metadata:
  node_type: memory
  type: feedback
  originSessionId: 8fc581f0-a093-4424-b02d-ee4303ad460f
---

主人 2026-07-14 定死的帧号口径（寒冰菇实施期间纠正）：

- 动画预览工具显示的帧号比 `Animator::AddFrameEvent` 的真实帧号**大 1**（预览 25 帧触发 → 代码写 24）。
- **主人以后报给 Claude 的帧号默认已经 −1 过**（即已是代码口径），`AddFrameEvent` 直接用，**不许再减**。
- 只有 Claude 自己从预览/reanim 读数推帧号时才需要手动 −1。
- 旧说法"末-1 帧不触发"（舞王 Die@99）其实就是这个偏移的表象，勿再当作独立规则记。

**Why:** 两边口径不一致时，事件会差一帧触发或（死亡动画停帧处）永不触发——症状=僵尸血 0 卡 anim_death、10 秒后 WATCHDOG force-die。

**How to apply:** 主人给的帧号原样写进 AddFrameEvent；已同步进 [project_pvz_iceshroom_freeze](project_pvz_iceshroom_freeze.md) 与 .claude/skills/adding-plant、adding-zombie 两个 SKILL.md。
