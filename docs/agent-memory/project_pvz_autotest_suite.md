---
name: pvz-autotest-suite
description: AutoTest 脚本自动驾驶套件 — 已完成(7 Task 全过 + 最终 opus 整体审查 Ready)，2026-06-13 收工
metadata:
  node_type: memory
  type: project
  originSessionId: 69e31e04-74c3-4128-8db4-89c425da06e5
---

AutoTest 套件（Claude 自动跑游戏/截图/验证视觉）**已完成**：2026-06-12 两会话做 Task 1-4，2026-06-13 会话完成 Task 5-7 + 最终审查。范围 `7af303c..41f8c5c` 共 16 commits，全在 master（未 push，push 由主人决定）。

**Why:** 主人要求的自动化开发套件；从此 Claude 可独立完成「改代码→msbuild→`-AutoTest` 跑脚本→Read 截图/state.json 验证」闭环。

**How to apply（用法已写进 CLAUDE.md「AutoTest 自动化测试套件」一节，以那里为权威）:**
- cwd 必须是 `x64\Release`；`.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\xxx.json -Seed 42`；退出码 0/1/100；产物（PNG/state.json/run.log）在 `x64\Release\autotest\out\<脚本名>\`，run.log 是权威记录
- 12 ops 全落地；新植物/僵尸类型须在 `Game/AutoTest/TestDriver.cpp` 名表加一行
- 范例：`demo_peashooter.json`（验收）、`smoke_*.json`（冒烟）；端到端实测：demo 5/5 验收过，-Seed 42 两次跑僵尸 x 差 ~0.007px
- 计划/spec 存档：`docs/superpowers/plans/2026-06-12-autotest-suite.md`、`docs/superpowers/specs/2026-06-12-autotest-suite-design.md`

**收尾会话 (Task 5-7) 关键决策与教训:**
- Task 5 dump_state（0ce7f36 + 1003380 审查修复：`out.dump(2)` 加 try/catch 与 LoadScript 异常惯例对齐）；审查的 `slowCooldown` 改名/动画速度缺失两条裁决不采纳——字段集是主人批准计划原文，不重新设计
- Task 6 demo 验收（650e44b）；Task 7 CLAUDE.md（3a9bb24 + 7d83e59 修"three families"残句——子代理改文档后控制者要亲自复读连贯性）
- 最终 opus 整体审查：Ready 无 Critical/Important；唯一采纳 Minor = 交换链 TRANSFER_SRC 无条件 OR 进 usage 但规范不保证 supportedUsageFlags 含它（41f8c5c 修：能力位探测 + `SwapchainSupportsTransferSrc()` + EndFrame 优雅降级；RecreateSwapchain 复用 CreateSwapchain 故重建路径同覆盖）
- subagent 流程怪点重确认：质量审查代理两次以"I now have all the information"收尾不出报告 → 重派时在 prompt 里显式要求"final message MUST be the report"；SendMessage 工具仍不可用（ToolSearch 查无此名），修复/复审一律新派代理带全文 finding

相关：[build-permission-msbuild](feedback_build_permission_msbuild.md)（构建授权）、[collaboration-style](feedback_collaboration_style.md)
