---
name: project_pvz_adventure_progression
description: 2026-07-18 九关制冒险进度统一；显式关卡植物奖励表取代Trophy关卡号强转PlantType，背景边界和关卡显示共用同一口径
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-09
---

# 九关制冒险进度与植物奖励表

主人删除原版铲子关后，冒险模式固定为每大关 9 小关。此前三个口径互相漂移：

- `Board` / `MainMenuScene` 用 `/9`、`%9` 显示关卡；
- `GameAPP::GetBackgroundID` 曾混用 9/10 关边界，导致内部 level 19 显示为 3-1 却仍是黑夜；
- `Trophy` 用 `static_cast<PlantType>(level)` 解锁，导致 1-8 提前拿到小喷菇。

2026-07-18 收敛到 `Game/AdventureProgression.h`：

- `LEVELS_PER_AREA=9`，显示与背景分段共用 `GetAreaNumber` / `GetLevelNumberInArea`；
- `PLANT_REWARD_BY_LEVEL` 显式列出 1..45，每项为具体 `PlantType` 或 `NO_PLANT_REWARD`；
- 奖励按关显式配置，不再假设“每大关第 8 小关都为空”：当前 1-8、4-8、5-8 无植物，2-8 解锁精英胆小菇，3-8 解锁毒囊射手；各大关第 9 小关仍解锁下一场景首株植物；
- `Trophy::AdvanceAdventureProgress` 无论有无植物都推进进度，仅在奖励不是 `NO_PLANT_REWARD` 时去重加入 `mHaveCards`；
- 禁止再通过插入/挪动 `PlantType` 调奖励顺序：`PlayerInfo.json.havecards` 和关卡存档都按整数保存枚举，改值会破坏旧档。

背景边界：1-9 白天、10-18 黑夜、19-27 泳池、28-36 雾夜泳池、37-45 全部为白天屋顶。生存模式 1000/1001 保持独立。

2026-08-09 主人把 5-9 定为白天屋顶 BOSS 关。`AdventureProgression` 以 `BossSlot::RESERVED`
正式登记该关性质，但只预留身份/机制扩展位：当前不绑定出怪、不实现 BOSS 特性，也明确不使用
现有 `ZOMBIE_BOSS`（僵尸博士）。僵尸博士计划放到未来 6-9；第六大关尚未接入当前五大关流程，
因此此时只更新设计记忆，不提前写入关卡号 54 的运行逻辑。

2026-08-09 已注册权重为 0 的 `ZOMBIE_ROOF_MARSHAL` 第一阶段视觉样机，用于验证普通僵尸时间线
与独立军官分体素材；它仍未绑定 `BossSlot::RESERVED`，不会改变正式 5-9 的空 BOSS 槽语义。
完整素材、军帽随头飞出的专属掉头粒子与验证见 `project_pvz_roof_marshal_prototype.md`。

验证：`smoke_adventure_progression.json` 使用 `set_adventure_level` + `force_trophy` 走真实 Trophy 点击结算，断言 1-8 只推进、1-9 解锁小喷菇、2-8 解锁精英胆小菇、3-8 解锁毒囊射手、4-8 只推进，并覆盖全部背景边界；`dump_state` 为此新增 level/background/adventureLevel/haveCards。2026-08-09 又增加 `isBossLevel/bossSlot` 投影和最小 `smoke_level_5_9_boss_slot.json`，锁定 5-9 的白天屋顶、`RESERVED` 槽位及未生成僵尸博士的空实现。

2026-08-09 当前证据：`clang-release` 配置、编译与 LTO 链接退出 0；主人当前桌面可见
`smoke_level_5_9_boss_slot.json`（11 条命令/6 项断言）与完整
`smoke_adventure_progression.json`（99/38）均 exit 0、`script finished OK`、日志 0 ERROR/WARN。
后续提交 `b5765be` 已把 5-9 调整为 15 波，正式出怪池为普通、路障、铁桶，因此当前预览顺序为
`ZOMBIE_NORMAL/ZOMBIE_TRAFFIC_CONE/ZOMBIE_BUCKET`；`smoke_level_5_9_boss_slot.json` 已同步断言
`maxWave=15` 和三只预览。BOSS 槽仍为 `RESERVED`，同步截图继续确认选卡场景使用晴朗白天屋顶。

旧存档不做自动删卡迁移：已经提前获得小喷菇的档会保留该卡，避免误删开发者或手动授予的卡；验证新流程使用 AutoTest 隔离状态或新档。

2026-08-03 将毒囊射手从 4-8 前移至 3-8，4-8 恢复为无植物奖励。玩家 schema 升至 v2：旧档 `adventureLevel >= 27` 且卡组尚无毒囊射手时补发一次，26 不提前发、已有卡不重复，`save-schema` 纯测试 1/1 通过。`clang-release` 配置与最终构建退出 0；桌面可见 `smoke_adventure_progression.json` 共 94 条命令、窗口标题“植物大战僵尸中文版”、exit 0。`run.log` 证明 3-8 奖杯结算后冒险进度变为 27、卡片数增至 5 且新卡为 `PLANT_TOXICPEASHOOTER`；随后 4-8 结算只把进度推进到 36，卡片数仍为 5。同步截图 `reward_toxic_peashooter_3_8.png` 显示泳池背景、3-8 关卡标签与奖杯。
