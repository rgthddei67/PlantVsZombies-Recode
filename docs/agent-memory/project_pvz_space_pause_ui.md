---
name: project_pvz_space_pause_ui
description: 空格轻量暂停、顶部文字与暂停期间倍速待选的交互契约
metadata:
  node_type: memory
  type: project
---

# 空格轻量暂停与暂停倍速待选

2026-08-11 按玩家反馈收口战斗暂停交互：

- `GameScene::mSpacePauseActive` 只表示空格键拥有的轻量暂停。空格不再创建
  `GameMessageBox`，只在种子槽下方的画面上中部绘制一行“游戏暂停”；再次按空格恢复。
- Esc 与右上“主菜单”继续打开包含返回、重开、图鉴、音量和血量显示的完整菜单。
  从轻量暂停进入完整菜单时全程保持冻结，只转移暂停 UI 所有权，避免一帧误恢复。
- `DeltaTime::SetSelectedTimeScale` / `GetSelectedTimeScale` 区分“实际倍速”和“玩家待选倍速”。
  暂停时实际 `timeScale` 始终为 0；点击倍速按钮仍按 `1x → 2x → 0.5x → 1x`
  循环更新按钮与待恢复值，只有恢复游戏后才应用。
- 空格不会解除词条选择、词条查看、开发者面板、重开/退出确认等其他模态持有的暂停。
  泳池水面逐帧相位也在全局暂停时冻结，避免轻量暂停暴露出仍在运动的背景。

验证：默认 `clang-release` 配置与构建退出码 0；vcpkg applocal 后处理打印了找不到
`dumpbin/objdump` 的非阻断提示。当前桌面可见运行 `smoke_space_pause.json`，窗口标题为
“植物大战僵尸中文版”，75 条命令全部完成、退出码 0、`run.log` 以
`script finished OK` 结束且无 FAIL/ERROR/WARN。状态证明暂停中
`timeScaleOn1000=0`、`selectedTimeScaleOn1000=2000`、完整菜单未打开，恢复后两者分别为
`2000/2000`；三张同步截图确认顶部文字、暂停中按钮切换和恢复后文字消失。泳池状态在暂停
前后相位均为 429，恢复后推进到 433；Esc 转完整菜单及词条选择模态不被空格解除也已断言。
