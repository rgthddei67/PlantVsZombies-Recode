---
name: project-pvz-mainmenu-button-arbitration
description: 主菜单石碑排版、ButtonManager命中仲裁、全屏控制台，以及未到2-1时的一次性跳关入口
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

## 2026-07-31：右下角全屏控制台

主菜单右下角新增绿色「控制台」按钮，点击后打开覆盖整个 1100×600 逻辑画面的深色
设置页。目前只有“蒙特卡洛小丑选择植物”一项，使用
`GameMessageBox::Builder::Checkbox` 创建真实 `Button`，勾选态直接绑定
`GameAPP::mEnableMonteCarloAI`；关闭并重开面板会从当前字段恢复视觉状态，正式退出
游戏时继续由既有 `PlayerInfo.json` 保存。

模态页打开时由 `SetMainMenuButtonsEnabled(false)` 统一屏蔽石碑、图鉴、选项、退出、
跳关与控制台入口。单纯禁用不影响 `Button::Draw`：原退出按钮由 `UIManager` 在
GameMessageBox 之后绘制时曾穿透全屏背景，因此它也改为 `SetSkipDraw(true)`，与其他
主菜单小入口一起只在无模态页时由 `DrawButton` 手动绘制。

专项 `smoke_mainmenu_console` 走真实入口/CheckBox/关闭/重开点击链，断言开关
true→false、重开仍为 false，保存状态 JSON 并截图启用/禁用两态；可见
`clang-playtest` exit 0。同期重跑 `smoke_mainmenu_buttons` exit 0，并人工读取
`after_overlap_click_still_menu`，确认重叠带点击后仍停留主菜单。
