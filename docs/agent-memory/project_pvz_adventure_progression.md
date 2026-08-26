---
name: project_pvz_adventure_progression
description: 九关制冒险进度统一；显式植物奖励表、七大关背景边界和关卡显示共用同一口径
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-26
---

# 九关制冒险进度与植物奖励表

主人删除原版铲子关后，冒险模式固定为每大关 9 小关。此前三个口径互相漂移：

- `Board` / `MainMenuScene` 用 `/9`、`%9` 显示关卡；
- `GameAPP::GetBackgroundID` 曾混用 9/10 关边界，导致内部 level 19 显示为 3-1 却仍是黑夜；
- `Trophy` 用 `static_cast<PlantType>(level)` 解锁，导致 1-8 提前拿到小喷菇。

2026-07-18 收敛到 `Game/AdventureProgression.h`：

- `LEVELS_PER_AREA=9`，显示与背景分段共用 `GetAreaNumber` / `GetLevelNumberInArea`；
- `PLANT_REWARD_BY_LEVEL` 显式列出 1..63，每项为具体 `PlantType` 或 `NO_PLANT_REWARD`；5-9 现解锁接地菇，供 6-1 起使用；第六大关的 6-1 无奖励、6-2 解锁忧郁菇、6-3 解锁双子向日葵、6-4 解锁避雷花盆、6-5 解锁冰瓜、6-6 解锁玉米加农炮、6-7 解锁磁暴菇、6-8 解锁模仿者，6-9 按当前定案为 `NO_PLANT_REWARD`；第七大关已接入 7-1 雪锚果、7-2 融雪投手、7-4 伏霜雷、7-6 警铃草、7-7 炉芯花，其余关无植物奖励；
- 奖励按关显式配置，不再假设“每大关第 8 小关都为空”：当前 1-8、4-8、5-8 无植物，2-8 解锁精英胆小菇，3-8 解锁毒囊射手；各大关第 9 小关仍解锁下一场景首株植物；
- `Trophy::AdvanceAdventureProgress` 无论有无植物都推进进度，仅在奖励不是 `NO_PLANT_REWARD` 时去重加入 `mHaveCards`；
- 禁止再通过插入/挪动 `PlantType` 调奖励顺序：`PlayerInfo.json.havecards` 和关卡存档都按整数保存枚举，改值会破坏旧档。

背景边界：1-9 白天、10-18 黑夜、19-27 泳池、28-36 雾夜泳池、37-45 白天屋顶、46-54 黑夜屋顶、55-63 冬日花园。生存模式 1000/1001/1002 保持独立。

2026-08-09 主人把 5-9 定为白天屋顶 BOSS 关。`AdventureProgression` 现以
`BossSlot::ROOF_MARSHAL` 显式登记；`Board::SummonNextWave()` 完成本波普通出怪后调用
`TrySummonAdventureBoss()`，只在 `mCurrentWave == mMaxWave == 15` 时额外创建一只中路、`x=1000`
的 `ZOMBIE_ROOF_MARSHAL`。越过最终波的开发者直调不会重复创建，普通关和其他槽位 no-op。
现有 `ZOMBIE_BOSS`（僵尸博士）不用于 5-9。早期曾提议把它留给 6-9，但当前第六大关定案不设
BOSS 槽，该提议不再作为待办。2026-08-10 最初只接入第六大关 46～54 的正式 `NIGHT_ROOF`
场景和奖励占位。2026-08-12 已将 `PLANT_GROUNDINGSHROOM`
登记为 5-9 奖励，专门供 6-1 起应对雷荷；2026-08-13 又把原版紫卡 `PLANT_GLOOMSHROOM`
与 `PLANT_TWINSUNFLOWER` 依次登记为 6-2/6-3 奖励，并显式锁定 6-1 为空；同日把自创紫卡
`PLANT_LIGHTNINGRODPOT` 登记为 6-4 奖励。2026-08-14 又把原版紫卡 `PLANT_WINTERMELON`
登记为 6-5 奖励；2026-08-15 又把原版紫卡 `PLANT_COBCANNON` 登记为 6-6 奖励，并把战斗版
`PLANT_GOLD_MAGNET` 磁暴菇登记为 6-7 奖励；2026-08-23 又把 `PLANT_IMITATER` 模仿者登记为
6-8 奖励。6-1 与 6-9 按当前设计无植物奖励，6-9 的 `BossSlot` 为 `NONE`；至此第六大关奖励表
与当前内容边界完成。

`ZOMBIE_ROOF_MARSHAL` 的独立权重继续为 0，不进入普通波次随机池；正式最终波由 BOSS 槽位单独创建。
完整素材、生存层、指挥召唤、换行/残血推进、天气命令、军帽随头飞出的专属掉头粒子与验证见
`project_pvz_roof_marshal_prototype.md`。

验证：`smoke_adventure_progression.json` 使用 `set_adventure_level` + `force_trophy` 走真实 Trophy 点击结算，断言 1-8 只推进、1-9 解锁小喷菇、2-8 解锁精英胆小菇、3-8 解锁毒囊射手、4-8 只推进，并覆盖全部背景边界；`dump_state` 为此新增 level/background/adventureLevel/haveCards。2026-08-09 又增加 `isBossLevel/bossSlot` 投影和最小 `smoke_level_5_9_boss_slot.json`，现锁定 5-9 的白天屋顶、`ROOF_MARSHAL` 槽位、前 14 波不生成首领及第 15 波唯一正式督军。

2026-08-10 可见 `smoke_adventure_progression.json` 进一步锁定内部 46/54 显示为 6-1/6-9 且背景均为 `NIGHT_ROOF`；可见 `smoke_sixth_area_night_roof.json` 逐关覆盖 46～54，并确认白天生存仍能启用通用天气。这段证据只描述当日尚无专属出怪配置的骨架阶段；后续四种专属僵尸、逐关编排、6-8 模仿者和 6-9 完整迷雾均已补齐，不能再据此判断当前内容未完成。

2026-08-13 可见 `smoke_gloomshroom.json` 通过真实奖杯结算锁定内部 46（6-1）卡数不变、内部 47（6-2）新增 `PLANT_GLOOMSHROOM` 并推进到 48；既有 `smoke_grounding_shroom_reward.json` 仍确认 5-9 只解锁接地菇。

2026-08-13 可见 `smoke_twin_sunflower.json` 通过真实奖杯结算锁定内部 48（6-3）新增
`PLANT_TWINSUNFLOWER` 并推进到 49；卡数从 1 增至 2，新卡位为双子向日葵。

2026-08-13 可见 `smoke_lightning_rod_pot_reward.json` 通过真实奖杯结算锁定内部 49（6-4）新增
`PLANT_LIGHTNINGRODPOT` 并推进到 50（6-5）；17 条命令 exit 0，`run.log` 为 `script finished OK`。

2026-08-14 可见 `smoke_winter_melon.json` 通过真实奖杯结算锁定内部 50（6-5）新增
`PLANT_WINTERMELON` 并推进到 51（6-6）；默认与 `-NoInstance` 两路径 161 条命令均 exit 0，
`status.json=passed`，完整数值、减速与资源契约见 `project_pvz_winter_melon.md`。

2026-08-15 可见 `smoke_cob_cannon_night_roof.json` 通过真实奖杯结算锁定内部 51（6-6）新增
`PLANT_COBCANNON` 并推进到 52（6-7）；同一脚本共 87 条命令 exit 0、`status.json=passed`，并同时
覆盖双格植物在黑夜屋顶的单侧避雷花盆保护与存档，完整契约见 `project_pvz_cob_cannon.md`。

2026-08-15 可见 `smoke_gold_magnet_reward.json` 通过真实奖杯结算锁定内部 52（6-7）新增
`PLANT_GOLD_MAGNET` 并推进到 53（6-8）；17 条命令 exit 0、`status.json=passed`，完整能力与资源
契约见 `project_pvz_gold_magnet.md`。

2026-08-23 `PLANT_IMITATER` 已正式登记为内部 53（6-8）奖励，`AdventureProgression.h` 的奖励表与
编译期断言共同锁定该映射；6-9（内部 54）保持无奖励且无 BOSS 槽。选卡、同 ID 变身、存档与多后端
验证见 `project_pvz_imitater.md`。

2026-08-24 `PLANT_SNOWANCHORNUT` 已正式登记为内部 55（7-1）奖励。Release 专项
`smoke_snow_anchor_nut.json` 通过真实奖杯点击把进度推进到 56（7-2），并锁定初始仅有豌豆射手时
卡数从 1 增至 2、新卡为雪锚果；默认 Vulkan 实例路径与 `-NoInstance` 两次可见运行均为 82 条命令、
exit 0、`status=passed`、`script finished OK`。

2026-08-25 `PLANT_FROSTMINE` 已正式登记为内部 58（7-4）奖励。专项先通过内部 57（7-3）
真实奖杯结算确认只推进到 58、卡数保持 1，再通过 7-4 奖杯把进度推进到 59（7-5）、卡数从 1
增至 2且新增卡为伏霜雷。`clang-debug` 与最终 `clang-release` 默认 Vulkan 可见路径均完整执行
96 条命令至 command 95，`-NoInstance` 短路径均执行 17 条命令至 command 16，全部 exit 0、
`status=passed`、`script finished OK`。最终 Release 全量 O2/LTO 构建零编译器警告，主程序
Win7 导入审计通过 378 项，三项 CTest 全部通过。

2026-08-09 当前证据：`clang-release` 配置、编译与 LTO 链接退出 0；主人当前桌面可见
`smoke_level_5_9_boss_slot.json`（11 条命令/6 项断言）与完整
`smoke_adventure_progression.json`（99/38）均 exit 0、`script finished OK`、日志 0 ERROR/WARN。
后续提交 `b5765be` 已把 5-9 调整为 15 波，正式出怪池为普通、路障、铁桶，因此当前预览顺序为
`ZOMBIE_NORMAL/ZOMBIE_TRAFFIC_CONE/ZOMBIE_BUCKET`；`smoke_level_5_9_boss_slot.json` 已同步断言
`maxWave=15` 和三只预览。BOSS 槽现为 `ROOF_MARSHAL`，同步截图继续确认选卡场景使用晴朗白天屋顶。

旧存档不做自动删卡迁移：已经提前获得小喷菇的档会保留该卡，避免误删开发者或手动授予的卡；验证新流程使用 AutoTest 隔离状态或新档。

2026-08-24 第七大关 55～63 接入 `WINTER_GARDEN` 背景和逐关出怪表时最初只复用既有僵尸且不发植物奖励；截至 2026-08-26 已正式登记 7-1 雪锚果、7-2 融雪投手、7-4 伏霜雷、7-6 警铃草、7-7 炉芯花，并接入雪橇车队、冰墙工程师、冰裂钻机、气象干扰僵尸和冰像处刑者。冬日花园的寒潮、冻融线、降雪和禁台风契约见 `project_pvz_winter_garden.md`，完整内容顺序见 `project_pvz_winter_area_content_plan.md`。

2026-08-25 `PLANT_ALARMBELLFLOWER` 已正式登记为内部 60（7-6）奖励；奖杯专项位于
`smoke_alarm_bell_flower.json`，通过真实 Trophy 点击把进度推进到 61（7-7），并锁定新增卡为警铃草。
最终 `clang-release` 默认 Vulkan 与 `-NoInstance` 两路径均执行 120 条命令至 command 119、
exit 0、`status=passed`。完整中断、动画和资源合同见 `project_pvz_alarm_bell_flower.md`。

2026-08-26 `PLANT_FURNACECOREFLOWER` 已正式登记为内部 61（7-7）奖励；`smoke_furnace_core_flower.json`
覆盖真实 7-7 奖杯结算，并在 `clang-release` 默认 Vulkan 与 `-NoInstance` 两路径均执行 120 条命令、
exit 0、`status=passed`。完整充能、冰封阻断、资源和存档合同见 `project_pvz_furnace_core_flower.md`。

2026-08-03 将毒囊射手从 4-8 前移至 3-8，4-8 恢复为无植物奖励。玩家 schema 升至 v2：旧档 `adventureLevel >= 27` 且卡组尚无毒囊射手时补发一次，26 不提前发、已有卡不重复，`save-schema` 纯测试 1/1 通过。`clang-release` 配置与最终构建退出 0；桌面可见 `smoke_adventure_progression.json` 共 94 条命令、窗口标题“植物大战僵尸中文版”、exit 0。`run.log` 证明 3-8 奖杯结算后冒险进度变为 27、卡片数增至 5 且新卡为 `PLANT_TOXICPEASHOOTER`；随后 4-8 结算只把进度推进到 36，卡片数仍为 5。同步截图 `reward_toxic_peashooter_3_8.png` 显示泳池背景、3-8 关卡标签与奖杯。
