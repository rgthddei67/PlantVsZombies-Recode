---
name: project_pvz_autotest_assert_state_todo
description: "AutoTest assert_state 命令 ✅完成合master未push：dump字段断言根治smoke假绿"
metadata:
  node_type: memory
  type: project
  originSessionId: d3765641-51c5-4361-94ca-bc781aa1f8c1
---

2026-07-04 完成。`assert_state`（`{"op":"assert_state","path":"perks.stacks.PLANT_DAMAGE_UP","equals":3}`）落地于 TestDriver：dump_state 的内联序列化抽成 `BuildStateJson(opName, out)` 两 op 共用；path 点分嵌套，纯数字段作数组下标（`zombies.0.type`）；`equals` 用 nlohmann::json `==` 严格比对，缺段/越界/不匹配均 Fail → exit 1，run.log 记 `assert_state OK: path == value`。

已补断言：smoke_develop（devNoCooldown/devFreePlant/zombieNumber/zombies.0.type/wave/boardState/survivalRound）、smoke_perks（stacks 三项 + plantDamageOn100=130 + damageToZombieOn100=25）。验证三路径：带 -develop 绿 / perks 绿 / **不带 -develop 现在 exit 1 精确报 `devNoCooldown 期望=true 实际=false`**（原假绿场景根治）。

foot-gun：①浮点字段（zombieHealthMult=1.2000000029802322）不要 equals 断言，用整数投影字段（plantDamageOn100 等）②断言真值以先跑一遍的 dump 为准（如 dev 面板召唤的是 ZOMBIE_TRAFFIC_CONE 非 NORMAL、跳关后 boardState=CHOOSE_CARD 且 zombieNumber=1 但 zombies 数组为空——新关 pending 态）。用法权威在 CLAUDE.md AutoTest 节。起源见 [project_pvz_messagebox_builder](project_pvz_messagebox_builder.md)。
