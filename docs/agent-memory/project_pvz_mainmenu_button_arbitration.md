---
name: project-pvz-mainmenu-button-arbitration
description: 主菜单石碑紧凑排版、ButtonManager命中仲裁，以及未到2-1时的一次性跳关入口
metadata:
  node_type: memory
  type: project
  originSessionId: a52f35c5-40db-482b-b658-161219bc38ea
---

2026-07-07 (e65d869..43241cf, 未push) 主菜单四石碑按原版放大紧凑，定稿=缩放1.0、x=545、y=85/175/252/325（主人5轮视觉迭代：0.82紧凑→0.95嫌大→0.90嫌偏上→1.0铺满碑面→左移13px）。

核心机制：`Button::ContainsPoint` 是轴对齐矩形而石碑贴图是斜的+带透明边距，紧贴后判定框必然重叠。解法=**ButtonManager::UpdateAll 两遍命中仲裁**：先收集所有 `CanReceiveHit(鼠标点)` 的按钮，只把 hover/click 判给中心距离最近者；`Button::Update(input, bool hitAllowed=true)` 默认参数保证其他调用点行为不变。所有按钮更新都走 ButtonManager::UpdateAll（UIManager 委托），仲裁一处覆盖全局。

foot-gun：
- "LEVEL x-x" 数字坐标（MainMenuScene.cpp DrawLevel）与冒险按钮位置/缩放**硬绑定**，动石碑必须按贴图内相对坐标换算数字位置（rel=(旧坐标-旧pos)/旧scale，新坐标=新pos+rel*新scale）。
- smoke_mainmenu_buttons.json 的重叠带点击测试有假绿风险：若仲裁失效误进冒险，后面 wait_state CHOOSE_CARD 照样过——必须 Read 中间截图 after_overlap_click_still_menu 确认仍在主菜单。
- 石碑贴图各自透明边距不同，间距均匀靠截图迭代调 y，不是等差数列。

## 2026-07-28：未到 2-1 时显示跳关按钮

主菜单左下角增加绿色文字的「跳到 2-1」按钮，仅在 `mAdventureLevel < 10`
时创建。点击后由 `MainMenuScene::Update` 消费 pending 状态，避免在按钮回调内
切场景；入口固定进入内部关卡 10（2-1），同时把永久冒险进度只提升、不回退到
至少 10。

跳关会先保证初始豌豆射手存在，再按 `AdventureProgression::GetPlantReward`
补齐 1-1～1-9 的正式植物奖励，去重后立即 `SavePlayerInfo`。达到或超过 2-1
后返回主菜单不再创建该按钮。

专项 `smoke_mainmenu_skip_second_area.json` 可见验证关卡 10、黑夜背景、
`supportsWeather=true`、冒险进度 10、9 张应有植物，以及返回主菜单后按钮消失；
同时重跑 `smoke_mainmenu_buttons.json` 并人工查看
`after_overlap_click_still_menu`，确认原石碑重叠命中仍正确。
