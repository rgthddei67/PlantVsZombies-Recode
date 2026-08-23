---
name: project_pvz_last_selected_cards
description: PlayerInfo 上次选卡持久化、代理卡复合身份、ChooseCardUI 一键动画恢复与 AutoTest 契约
metadata:
  node_type: memory
  type: project
updated_at: 2026-08-23
---

# 上次选卡持久化与一键动画恢复

2026-08-12 `GameAPP::mLastSelectedCards` 按玩家点击顺序保存最近一次正式提交卡组的稳定植物枚举名；
`ChooseCardComplete()` 在转移卡片前立即写 `PlayerInfo.json.lastSelectedCards`，因此进入战斗后直接退出
仍保留记录。玩家 schema v4 为旧档补空数组并保留预发布已有字段；加载只接收字符串且限制数量，
避免损坏档造成无限扩张，具体植物解析延后到 `GameDataManager` 已初始化的选卡界面。

`ChooseCardUI` 右上角的“上次选卡”按钮使用 0.8 倍 `SeedChooser_Button2` /
`SeedChooser_Button2_Glow`，坐标始终相对面板而非屏幕。恢复只接受当前面板实际存在的卡，按稳定名
解析、去重、跳过未知/未注册/未拥有项并遵守 11 张上限；整组替换选择后统一调用既有
`Card::SetTargetPosition` 路径，让卡片可见飞入卡槽，不直接瞬移。

`smoke_last_selected_cards.json` 通过真实按钮点击锁定面板相对位置、运动中与终态，并在进入下一关后
再次恢复；2026-08-12 主人当前桌面可见 Vulkan 运行 exit 0，31 条命令全部完成，三张同步截图已检查，
`run.log` 以 `script finished OK` 结束且无 ERROR/WARN。`clang-release` 配置、LTO 链接和 Win7 导入
审计通过，`save-schema` 1/1 通过。

2026-08-23 模仿者需要把稳定卡牌身份与被模仿目标一起保存；序列化格式扩展为
`PLANT_IMITATER:PLANT_<TARGET>`，普通卡仍只写单个稳定枚举名。恢复时先按左侧身份查找面板中唯一的模仿者 Card，
再验证目标已注册、已拥有、不是模仿者且不是紫色升级卡，最后写入 target 并走既有飞入动画。取消该卡后 target 清空，
卡面恢复原始模仿者；不允许把目标植物误当成独立卡恢复或与普通目标卡去重。
