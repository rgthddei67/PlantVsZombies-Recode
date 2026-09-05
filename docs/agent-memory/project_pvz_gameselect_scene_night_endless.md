---
name: project_pvz_gameselect_scene_night_endless
description: GameSelectScene 挑战风格的冒险分页选关与八种地形无尽入口
metadata:
  node_type: memory
  type: project
  originSessionId: c8640840-222c-44ad-9164-cf95a1f498e8
---

2026-06-25：新增 `GameSelectScene`（继承 `Scene`，挑战模式风格选择界面）+ **黑夜无尽模式**，已在 master（scaffold merge `64535c1` + 接线 `3aba327`，**未 push**）。spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-25-gameselect-scene*`。

**场景本体**：素材是仓库已有的 `Challenge_Background.png`(1280×720,背景含顶部木牌横幅)/`Challenge_Window(_Highlight).png`(118×120,卡框常/悬停)；键由文件名自动派生(`IMAGE_CHALLENGE_*`，已补 RKEY)。逻辑分辨率 **1100×600**，背景按 1100/1280、600/720 缩放铺满。返回按钮复用年鉴 `IMAGE_ALMANAC_INDEXBUTTON(HIGHLIGHT)`。木牌中心实测 **(551,88)**。在 `GameApp.cpp` `RegisterScene<GameSelectScene>("GameSelectScene")`。

**文字自适应**：标题/卡片标签都走文件内匿名 namespace 的 `DrawFittedCenteredText`——`ResourceManager::GetFont(key,size)`+`TTF_SizeUTF8` 真实测宽，从大字号往下试到放得下再水平+垂直居中（**抄 PlantAlmanacScene 的字号自适应思路**，主人点名要的）。卡标签居中于卡框灰色标签条=卡高 **0.73**（实测,别用 kCardH-18 会贴底边偏下）。

**黑夜无尽（新模式）**：原本只有 `SURVIVAL_ENDLESS_LEVEL=1000`(白天)。新增 `SURVIVAL_ENDLESS_NIGHT_LEVEL=1001`(Board.h)；`Board::mIsSurvival` 放宽为 `==1000||==1001`(Board.cpp:26)——**`mIsSurvival` 是唯一行为闸**(波次/词条/GameInfoSaver/血量倍率全查它,非 `==1000`)，放宽一行即全覆盖，两关 level 号不同存档天然分离(`level{1000/1001}_data.json`)；`GetBackgroundID(1001)→GROUND_NIGHT`(GameApp.cpp)，复用现有夜晚机制(蘑菇夜醒/天上不掉阳光)。

**入口接线**：主菜单「生存模式」按钮（现由 `MainMenuScene` 独占的 `MainMenuButtons::Initialize` 创建）→`mReadyToSwitchSurvival`→`MainMenuScene::Update` 改为 `SwitchTo("GameSelectScene")`(不再直进无尽关)。GameSelectScene 卡1「白天无尽」→1000、卡2「黑夜无尽」→1001：点击置 `mPendingEnterLevel`，`Update` 里 `SetGlobalData("EnterLevel",N)+SwitchTo("GameScene")`（不在回调内切场景，沿用本仓库惯例）。其余 7 张卡 **注释保留**待后续模式接入(取消注释 makeCard+同步 kLabels/kRow2Y 即可)。

**卡片地面预览**(commit 1834a5a)：两无尽卡图标开口下各垫地面图(卡1 `Almanac_GroundDay` 绿/卡2 `Almanac_GroundNight` 暗,键用字面量 `IMAGE_ALMANAC_GROUND{DAY,NIGHT}`)。靠 `AddTexture(...,drawOrder=-900,...)` 夹在羊皮纸(-1000)与卡框(LAYER_UI)之间——**复用基类分层,无需新渲染队列/裁剪**;地面填入卡框透明开口(实测 native x[20..96] y[8..66] of 118×120,×kCardScale+2px bleed),溢出被不透明边框/标签条吃掉。

**AutoTest 全链路验证**(关键坐标)：主菜单生存按钮中心 ≈(689,400)、卡1(171,207)、卡2(321,207)；`gameselect_smoke.json` 已改成 主菜单→生存→选择界面→黑夜无尽 的可达流程。实测白天卡→白天草坪、黑夜卡→夜晚草坪，clang-release 0 warn。相关坑见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)、生存机制见 [project_pvz_perk_system](project_pvz_perk_system.md)。

## 2026-07-29 泳池无尽

- 新增 `SURVIVAL_ENDLESS_POOL_LEVEL=1002`，`GameAPP::GetBackgroundID()` 映射到
  `WATER_POOL`；`Board::mIsSurvival` 与关卡名分支同步接入，因此沿用无尽轮次、词条、
  血量成长和 `level1002_data.json` 独立存档，同时直接复用泳池六行、水路僵尸替换、
  动态水面、睡莲与水面阳光经济。
- `Board::SupportsWeather()` 对 1002 返回 true，与第三大关日间泳池保持一致；天气玩法状态
  仍只由 Board 持有，没有新增展示或存档副本。
- 选择页第三张卡使用 `IMAGE_ALMANAC_GROUNDPOOL`，标签为“泳池无尽”，中心为 `(471,207)`。
  AutoTest 现用 `click target=main_menu_survival` 从真实按钮中心解析坐标；不再依赖硬编码点穿过
  斜石碑的重叠矩形命中区。
- `clang-playtest` 配置与完整增量构建退出码 0。可见 `gameselect_smoke` 从主菜单点击生存、
  点击第三张卡并进入 1002，18 条命令全部通过、退出码 0；状态锁定
  `WATER_POOL`、6 行、`poolRows=[2,3]`、第 1 轮、天气支持与小雨生效，选择页和泳池截图
  均已人工检查。

## 2026-08-27 冒险分页选关

- 主菜单“冒险模式”不再直接进入 `mAdventureLevel`，而是给 `GameSelectScene` 写入
  `GameSelectMode=adventure`；生存入口显式写入 `survival`，两种模式共享场景、卡框和延迟切场景
  生命周期，但各自生成独立入口。
- 冒险入口只生成内部关卡 `1..min(mAdventureLevel, AdventureProgression::LAST_ADVENTURE_LEVEL)`；
  页数按每页 6×3 自动计算，显示名统一走 `GetAreaNumber` / `GetLevelNumberInArea`，没有在 UI
  写死 63 或七大关。初始页按实际生成入口的最后一项计算，因此进度 58（7-4）进入时直接打开
  第 4 页、显示 55..58，不存在 59（7-5）；全部通关后自然定位最后一页。
- 上一页箭头固定在左下 `(180,535)`、下一页固定在右下 `(1015,535)`，均为 60×60；返回菜单
  仍位于 `(7,560)` 的 162×26 范围，因此上一页与返回按钮不重叠。页按钮回调只登记 delta，
  `Update` 离开 `ButtonManager` 遍历后才销毁旧页并仅创建新页卡片。
- 卡片预览优先使用已有 `Almanac_GroundDay/Night/Pool`。没有专用缩略图的黑夜泳池、白天屋顶、
  黑夜屋顶与冬日花园改从各自正式背景的同一归一化区域（left 0.56、top 0.12、width 0.19）
  等比放大后裁进卡框开口，避免伪装成其他地形，也不维护重复缩略资源。
- 冒险关仅当 `level < mAdventureLevel` 时在卡框左上按经典偏移绘制
  `IMAGE_MINIGAME_TROPHY`；当前待挑战关不标记，生存无尽模式永不标记。资源文件原已在
  `resources.xml` 注册，本次补齐强类型 `ResourceKeys` 键与 AutoTest 加载断言。
- `smoke_adventure_game_select.json` 通过真实主菜单入口、初始第 4 页、三次向前和三次向后翻页、
  当前页关卡点击，覆盖入口总数、页边界、按钮位置/朝向、通关标记、四类页面截图及进入 7-4；
  `clang-release` 默认 Vulkan 与 `-NoInstance` 可见路径均为 71 条命令、exit 0、`status=passed`、
  `script finished OK`。连续点击
  固定位置的翻页按钮前先把合成鼠标移出并等待两帧，避免渲染负载差异造成测试输入边沿偶发丢失。
  `gameselect_smoke.json` 回归确认生存关完成标记均为 false，并继续进入泳池无尽；
  三项 CTest 全部通过，最终 Release 编译零警告且 Win7 导入审计为 378 项。

## 2026-08-27 七种无尽地形

- `Board.h` 用 `SURVIVAL_ENDLESS_DEFINITIONS` 集中登记无尽关卡号、背景、显示名和解锁大关；现有 1000～1002
  之外新增 1003 黑夜泳池、1004 白天屋顶、1005 黑夜屋顶和 1006 雪原。`GameAPP` 背景映射、
  `Board::mIsSurvival`、关卡名与生存选关页均消费同一张表，后续加模式不再同步维护多份名单。
- 四张新卡继续复用冒险选关的预览策略：没有专用缩略图时从对应正式背景固定位置裁剪。生存页当前
  7 张卡都不显示通关奖杯；不同内部关卡号让正式 `level1003`～`level1006` 存档天然相互隔离。
- 场景能力仍按背景生效：1003 为六行夜泳池并启用基础雾/独立雾势；1004/1005 为五行屋顶并
  启用坡面径流，1005 额外启用夜屋顶雷荷；1006 为五行冬日花园并启用寒潮温度、冻融线和降雪。
  通用雨势对所有集中登记的无尽模式启用；无尽不再按模式禁用台风，前六种背景由玩家总开关决定，雪原仍因 `WINTER_GARDEN` 背景禁用。
- 无尽随机出怪池的粉色橄榄球按 `GetBackgroundIsNight(mBackGround)` 过滤，因而三种夜景无尽可抽取，
  白天屋顶与雪原无尽不会抽取；该筛选只存在于 `BuildSurvivalSpawnList()`，冒险关仍完全由
  `spawnlists.json` 决定，不影响雪原白天冒险配置的粉色橄榄球。
- `smoke_survival_endless_terrains.json` 从生存页真实点击 1003，再切换 1004～1006，锁定各关显示名、
  背景、行数、水路、天气、雾、径流、雷荷和冬季资格；`clang-release` 默认 Vulkan 与 `-NoInstance`
  可见运行均执行 55 条命令、exit 0、`status=passed`、`script finished OK`，五张截图已目验。
  更新后的 `gameselect_smoke` 与 `smoke_adventure_game_select` 同源回归也通过。

## 2026-08-27 无尽解锁门槛

- 每种无尽只在对应大关全部通关后创建：白天/黑夜/泳池/黑夜泳池/白天屋顶/黑夜屋顶/雪原
  分别要求完成第 1～7 大关。统一判断为
  `AdventureProgression::HasCompletedArea(mAdventureLevel, requiredAdventureArea)`，即进度必须严格
  越过该大关第 9 关；停在 1-9 或 7-9 本身时仍不算打完，领取奖杯推进到下一关后才解锁。
- `smoke_survival_endless_unlocks.json` 锁定四个边界：进度 9 无入口、进度 10 仅白天无尽、进度
  58（7-4）显示前六种且不创建雪原、进度 64 显示全部七种。`clang-release` 默认 Vulkan 与
  `-NoInstance` 可见路径均为 34 条命令、exit 0、`status=passed`、`script finished OK`；四张
  截图目验与状态一致。更新后的 `gameselect_smoke` 为 26 条命令，地形专项为 55 条命令，均通过。

## 2026-09-04 极夜无尽

- 新增 `SURVIVAL_ENDLESS_POLAR_LEVEL=1007`，集中定义映射到
  `Background::POLAR_NIGHT_SNOWFIELD`、显示名“极夜无尽”和第 8 大关解锁条件；选关入口、
  `Board::mIsSurvival`、关卡名、独立 `level1007` 存档与背景映射继续只消费同一张表。
- 解锁严格使用 `HasCompletedArea(adventureLevel, 8)`：进度停在 72（8-9）时仍只显示前七种，
  奖杯把进度推进到 73 后才显示第八张卡。这只是当前第八大关的完成边界，不把第八大关定义成
  最终区域；以后增加第九、十区域时扩展 `AdventureProgression` 和集中定义即可。
- 极夜无尽复用三仪表、雪穴与白毛风环境，但 `mIsSurvival` 明确隔离 8-1～8-3 教学保证和
  8-9 特殊终波。旧雨势、雾、冬日花园寒潮、径流和雷荷仍按极夜背景集中禁用。
- `smoke_survival_endless_unlocks.json` 锁定进度 72/73 两侧；
  `smoke_survival_endless_terrains.json` 锁定真实进入 1007、极夜环境、五种极夜僵尸在第 22 轮
  的统一生存池资格以及第 22 轮快照往返；`gameselect_smoke.json` 锁定当前八张入口仍为单页。
- 2026-09-04 `clang-release` 默认 Vulkan 可见验证：解锁专项 50 条、地形专项 92 条、
  `gameselect_smoke` 26 条均 exit 0、`status=passed`、`script finished OK`；目验
  `06_area_eight_finished.png` 的八张入口，以及 `06_polar_endless.png` /
  `07_polar_endless_round_22_reloaded.png` 的极夜棋盘、观测站和轮次文字均正确。CTest 3/3 通过。

## 2026-09-05 小游戏选关复用

`GameSelectScene::SelectMode::MINIGAMES` 复用相同卡框和延迟导航，从主菜单小游戏按钮进入；当前仅直接开放 2000「最后的家底」。冒险分页与生存解锁表保持独立，玩法与验证见 [最后的家底](project_pvz_minigame_last_savings.md)。

## 2026-09-05 冒险跳过本关

- 冒险选关右下 (800,552)、190×32 的“跳过本关”仅在当前页包含待通关冒险关时可见并启用；旧页、生存、小游戏及越过当前登记关卡边界时隐藏。沿用图鉴导航按钮纹理，不新增资源。
- 标准 GameMessageBox 使用 1.2 倍率与 FZJT 字体，正文明确显示关卡号。UIManager 统一隔离卡片/翻页输入，取消不改进度；确认只登记捕获的关卡号，Update 复核后再结算，不能在按钮回调中销毁场景。
- 奖励/进度复用 AdventureProgression::AdvanceProgress 与 SavePlayerInfo。保存失败恢复原进度/卡数并提示重试；成功后按关卡号删除续局存档。有新卡进入共用 PlantRewardScene，无新卡留在选关页并定位新关所在页。
- AutoTest smoke_adventure_skip 覆盖取消、背景卡片/翻页拦截、新卡与重复卡、无奖励跨页、当前登记末关、生存/小游戏隐藏；状态新增 gameSelectSkippableLevel 与 gameSelectSkipLevelButton，haveCards/haveCardCount 提升为跨场景投影。AutoTest 仍不写入或删除真实玩家存档。
验收：clang-release 编译及 Win7 378 项导入审计通过；可见默认 Vulkan 的 smoke_adventure_skip（最终 156 条）、smoke_plant_reward_almanac（81 条）、mainmenu_options_shot（15 条）和 adaptive_messagebox_confirm（23 条）均 exit 0、status passed，日志 script finished OK，相关截图已逐张检查。中途补拍遇到合成点击偶发未触发，增加移动后的处理帧、场景稳定等待及可选 click trace 后，最终跳关全流程通过；没有更改真实输入逻辑或玩家存档隔离。构建的 vcpkg applocal 阶段报告缺少依赖检查工具，但全静态主程序链接、Win7 导入审计和实际运行均成功。技能审计更新 adding-plant 的共用推进合同并通过 quick_validate；未新增植物美术或运行时视觉占位。