---
name: project_pvz_developer_mode
description: 2026-07-03 开发者模式(-develop) 完成合 master 未push——D键面板/无冷却/无视阳光/跳关/点草坪召唤/下一波
metadata:
  node_type: memory
  type: project
  originSessionId: 6a96c2e9-5043-4655-b933-4873a40fb805
---

2026-07-03 开发者模式全套完成（fea154a..09bfa45，master 未 push）。`-develop` 启动 → GameScene 按 D 呼出 GameMessageBox 面板（照词条选择框模式，autoClose=true 重建刷新文字）。

- 作弊守卫在收费点：`CardComponent::Update/StartCooldown`（无冷却）、`CardSlotManager::CanAfford(移到.cpp)/SpendSun`（无视阳光），全部 `GameAPP::mDevelopMode && mDevXxx` 双条件，无 -develop 零行为变化。
- 召唤放置模式：面板选类型→点草坪，`GetMouseWorldPosition()` 的 y 对 `GetZombieSpawnY(r)` 取最近行，x 直传 `Board::CreateZombie`，实测像素级准确。
- 跳关：回调内不可 SwitchTo（销毁自身），置 `mDevPendingLevel` 由 Update 尾部执行（同 mReadyToBackMenu 模式）。下一波=`mZombieCountDown=0`。
- dump_state 新增 devNoCooldown/devFreePlant；验收脚本 `autotest/scripts/smoke_develop.json`（点击坐标依赖面板布局，改布局须同步改）。

15e109d 补两修：①CardDisplayComponent::UpdateCardState 曾裸比阳光绕过 CanAfford→作弊开了卡不亮（收费点守卫要连显示层一起查）；②下一波改直调新提取的 `Board::SummonNextWave()`（原 Update 出波序列提取成公有方法），按钮 autoClose=false 暂停中连点连出。

**foot-gun**：①FZCQ 字体没有 ◀▶ 字形，按钮文字须用 ASCII `<` `>`；②AutoTest screenshot 产物无 .png 扩展名，Read 前须拷贝改名；③放置模式 ESC 与暂停菜单 ESC 同帧冲突，用局部 devConsumedEsc 旗标挡菜单分支。

2026-07-04 追加（0b4ee25）：「暂停刷怪」开关 `GameAPP::mDevSpawnPaused` —— 在 `Board::UpdateLevel` 于 `mZombieCountDown -= dt` 前 return，冻结出波倒计时与"本波清空提前出波"；面板「下一波」直调 `SummonNextWave` 不受影响。左上角 (5,5) 常驻角标 "开发者模式（刷怪已暂停）"（`BuildDrawCommands` 注册 DevModeBadge，LAYER_UI+100000）。dump_state 增 devSpawnPaused；验收 `smoke_dev_spawn_pause.json`（timescale 10 等 90 游戏秒断言 wave==0）。

spec/plan 在 docs/superpowers/{specs,plans}/2026-07-03-developer-mode*。关联 [project_pvz_perk_system](project_pvz_perk_system.md)（面板 UI 模式来源）、[project_pvz_autotest_suite](project_pvz_autotest_suite.md)。
