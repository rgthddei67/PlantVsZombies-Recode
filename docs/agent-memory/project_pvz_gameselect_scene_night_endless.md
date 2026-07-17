---
name: project_pvz_gameselect_scene_night_endless
description: GameSelectScene(挑战模式风格选择界面)+黑夜无尽模式 已落地 master(3aba327)
metadata:
  node_type: memory
  type: project
  originSessionId: c8640840-222c-44ad-9164-cf95a1f498e8
---

2026-06-25：新增 `GameSelectScene`（继承 `Scene`，挑战模式风格选择界面）+ **黑夜无尽模式**，已在 master（scaffold merge `64535c1` + 接线 `3aba327`，**未 push**）。spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-25-gameselect-scene*`。

**场景本体**：素材是仓库已有的 `Challenge_Background.png`(1280×720,背景含顶部木牌横幅)/`Challenge_Window(_Highlight).png`(118×120,卡框常/悬停)；键由文件名自动派生(`IMAGE_CHALLENGE_*`，已补 RKEY)。逻辑分辨率 **1100×600**，背景按 1100/1280、600/720 缩放铺满。返回按钮复用年鉴 `IMAGE_ALMANAC_INDEXBUTTON(HIGHLIGHT)`。木牌中心实测 **(551,88)**。在 `GameApp.cpp` `RegisterScene<GameSelectScene>("GameSelectScene")`。

**文字自适应**：标题/卡片标签都走文件内匿名 namespace 的 `DrawFittedCenteredText`——`ResourceManager::GetFont(key,size)`+`TTF_SizeUTF8` 真实测宽，从大字号往下试到放得下再水平+垂直居中（**抄 PlantAlmanacScene 的字号自适应思路**，主人点名要的）。卡标签居中于卡框灰色标签条=卡高 **0.73**（实测,别用 kCardH-18 会贴底边偏下）。

**黑夜无尽（新模式）**：原本只有 `SURVIVAL_ENDLESS_LEVEL=1000`(白天)。新增 `SURVIVAL_ENDLESS_NIGHT_LEVEL=1001`(Board.h)；`Board::mIsSurvival` 放宽为 `==1000||==1001`(Board.cpp:26)——**`mIsSurvival` 是唯一行为闸**(波次/词条/GameInfoSaver/血量倍率全查它,非 `==1000`)，放宽一行即全覆盖，两关 level 号不同存档天然分离(`level{1000/1001}_data.json`)；`GetBackgroundID(1001)→GROUND_NIGHT`(GameApp.cpp)，复用现有夜晚机制(蘑菇夜醒/天上不掉阳光)。

**入口接线**：主菜单「生存模式」按钮(GameButton::Start,MainMenuScene.h)→`mReadyToSwitchSurvival`→`MainMenuScene::Update` 改为 `SwitchTo("GameSelectScene")`(不再直进无尽关)。GameSelectScene 卡1「白天无尽」→1000、卡2「黑夜无尽」→1001：点击置 `mPendingEnterLevel`，`Update` 里 `SetGlobalData("EnterLevel",N)+SwitchTo("GameScene")`（不在回调内切场景，沿用本仓库惯例）。其余 7 张卡 **注释保留**待后续模式接入(取消注释 makeCard+同步 kLabels/kRow2Y 即可)。

**卡片地面预览**(commit 1834a5a)：两无尽卡图标开口下各垫地面图(卡1 `Almanac_GroundDay` 绿/卡2 `Almanac_GroundNight` 暗,键用字面量 `IMAGE_ALMANAC_GROUND{DAY,NIGHT}`)。靠 `AddTexture(...,drawOrder=-900,...)` 夹在羊皮纸(-1000)与卡框(LAYER_UI)之间——**复用基类分层,无需新渲染队列/裁剪**;地面填入卡框透明开口(实测 native x[20..96] y[8..66] of 118×120,×kCardScale+2px bleed),溢出被不透明边框/标签条吃掉。

**AutoTest 全链路验证**(关键坐标)：主菜单生存按钮中心 ≈(689,400)、卡1(171,207)、卡2(321,207)；`gameselect_smoke.json` 已改成 主菜单→生存→选择界面→黑夜无尽 的可达流程。实测白天卡→白天草坪、黑夜卡→夜晚草坪，clang-release 0 warn。相关坑见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)、生存机制见 [project_pvz_perk_system](project_pvz_perk_system.md)。
