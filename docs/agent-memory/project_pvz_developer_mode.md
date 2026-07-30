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

## 2026-07-30 面板选择持久化与生存直达

- `PlayerInfo.json` 新增可选字段 `developerSelectedLevel` 与
  `developerSelectedZombie`；旧档分别回退关卡 1 / `ZOMBIE_NORMAL`，属于可由中性默认值
  表达的增量字段，未提升 player schema。关卡与僵尸箭头一经点击即调用
  `SavePlayerInfo()`，新 `GameScene` 在 `OnEnter()` 从 `GameAPP` 恢复。
- 僵尸保存枚举名字符串，不保存 `kDevZombieTable` 下标或 `ZombieType` 整数：新僵尸移入
  `NUM_ZOMBIE_TYPES` 哨兵前时会改变后续数值，名称可稳定跨版本；未知或删除的名称回退普通
  僵尸，并在下一次保存时规范化。
- 「进入无尽」「进入夜无尽」现在直接把 1000/1001 交给延迟切关入口，单击即进入对应
  生存选卡场景；快捷入口不改 `mDevLevelSel`，因此不会覆盖已保存的普通关卡选择。
- 2026-07-30 `clang-playtest` 全量增量构建零警告；`save-schema` 通过；当前桌面可见
  `smoke_develop` 61 条命令通过，窗口标题已确认，退出码 0。状态取证证明夜/日快捷按钮
  分别直达 1001/1000 且保存值持续为关卡 2 / `ZOMBIE_TRAFFIC_CONE`，两张同步截图已检查。

spec/plan 在 docs/superpowers/{specs,plans}/2026-07-03-developer-mode*。关联 [project_pvz_perk_system](project_pvz_perk_system.md)（面板 UI 模式来源）、[project_pvz_autotest_suite](project_pvz_autotest_suite.md)。
