---
name: project_pvz_adventure_progression
description: 2026-07-18 九关制冒险进度统一；显式关卡植物奖励表取代Trophy关卡号强转PlantType，背景边界和关卡显示共用同一口径
metadata:
  node_type: memory
  type: project
---

# 九关制冒险进度与植物奖励表

主人删除原版铲子关后，冒险模式固定为每大关 9 小关。此前三个口径互相漂移：

- `Board` / `MainMenuScene` 用 `/9`、`%9` 显示关卡；
- `GameAPP::GetBackgroundID` 曾混用 9/10 关边界，导致内部 level 19 显示为 3-1 却仍是黑夜；
- `Trophy` 用 `static_cast<PlantType>(level)` 解锁，导致 1-8 提前拿到小喷菇。

2026-07-18 收敛到 `Game/AdventureProgression.h`：

- `LEVELS_PER_AREA=9`，显示与背景分段共用 `GetAreaNumber` / `GetLevelNumberInArea`；
- `PLANT_REWARD_BY_LEVEL` 显式列出 1..45，每项为具体 `PlantType` 或 `NO_PLANT_REWARD`；
- 当前每大关第 8 小关无植物，第 9 小关解锁下一场景首株植物；1-8 无植物、1-9 小喷菇；
- `Trophy::AdvanceAdventureProgress` 无论有无植物都推进进度，仅在奖励不是 `NO_PLANT_REWARD` 时去重加入 `mHaveCards`；
- 禁止再通过插入/挪动 `PlantType` 调奖励顺序：`PlayerInfo.json.havecards` 和关卡存档都按整数保存枚举，改值会破坏旧档。

背景边界：1-9 白天、10-18 黑夜、19-27 泳池、28-36 雾夜泳池、37-44 屋顶、45（5-9 Boss）黑夜屋顶。生存模式 1000/1001 保持独立。

验证：`smoke_adventure_progression.json` 使用 `set_adventure_level` + `force_trophy` 走真实 Trophy 点击结算，断言 1-8 只推进、1-9 解锁小喷菇，并覆盖全部背景边界；`dump_state` 为此新增 level/background/adventureLevel/haveCards。

旧存档不做自动删卡迁移：已经提前获得小喷菇的档会保留该卡，避免误删开发者或手动授予的卡；验证新流程使用 AutoTest 隔离状态或新档。
