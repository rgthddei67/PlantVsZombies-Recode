---
name: project-pvz-scaredyshroom-and-adding-plant-skill
description: "胆小菇完成(2026-07-08,合master未push)+新增植物流程已固化为 .claude/skills/adding-plant；CLAUDE.md 的 Adding a New Plant 节已删改为指向 skill"
metadata:
  node_type: memory
  type: project
  originSessionId: a1ac87f2-aa42-45fd-bfa5-16826aed4836
---

2026-07-08 胆小菇 (ScaredyShroom) 完成：spec 2d53f00 / 实现 4df943a / 站位影子校正 4c18916 / 读档伸头修复 894fe5a，均在 master 未 push。四态害怕状态机 (READY→LOWERING→SCARED→RAISING)，帧事件第 25 帧（主人看预览指定）。

**新增植物的完整流程已固化为项目 skill `adding-plant`（.claude/skills/adding-plant/SKILL.md），后续加植物直接用它，不要凭记忆重走。** CLAUDE.md 的 "Adding a New Plant" 节已删，只留一行指向 skill；"Adding a New Zombie" 节保留（主人另开会话做僵尸 skill）。

本次沉淀的三个新 foot-gun（skill 里都有，此处备索引）：
- 帧事件帧号必须问主人（他看动画预览），从 reanim 活跃区间推断会错（推 28 实际 25）。
- 种完必须与小喷菇/向日葵同行截图比脚底基线：本体 offset 在 gamedata.json（免编译），影子 offset 在代码 ShadowComponent::SetOffset（要编译），两套独立。
- 被状态机消费的节流缓存 = 存读档地雷：缓存初值会给读档第一帧注入捏造状态（mScaredCached=false → SCARED 态读档先伸头再缩回）；修法 = 计时器初始即到期首帧真算。AutoTest 存档被短路测不到，只能请主人手动退出重进验证。

关联 [project_pvz_gamedata_json](project_pvz_gamedata_json.md) [project_pvz_animator_playstate_save_fix](project_pvz_animator_playstate_save_fix.md) [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)
