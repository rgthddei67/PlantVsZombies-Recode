# 生存模式「词条」查看面板 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在生存模式右上角加一个「词条」按钮，点开弹出固定尺寸、字号自动缩放的纯色面板，列出已选词条 + 各自选择次数 + 顶部总计，打开时暂停游戏。

**Architecture:** 全部落在 `GameScene`。镜像既有 `BeginSurvivalPerkSelect` 的「纯色面板 + 绝对坐标文字 + TTF 量宽」做法，但把尺寸策略反过来——面板固定、字号在 18→10 之间线性搜索取最大可塞进者。按钮仅在 `mBoard->mIsSurvival` 时创建，普通关卡零改动。验证走本仓库既有 AutoTest 闭环（build → 跑脚本 → Read 截图）。

**Tech Stack:** C++17 / CMake(clang-release preset) / SDL2_ttf 量宽 / 自研 `GameMessageBox` 纯色面板 / AutoTest JSON 脚本。

参考 spec：`docs/superpowers/specs/2026-06-22-survival-perk-view-panel-design.md`

---

## 文件结构

- **修改** `PlantVsZombies/Game/GameScene.h` — 新增 2 个方法声明 + 2 个 weak_ptr 成员 + 1 个 bool。
- **修改** `PlantVsZombies/Game/GameScene.cpp` — 新增 `OpenPerkView()` / `ClosePerkView()` 定义；`OnEnter` 加按钮；`OnExit` 加 reset；`OpenMenu` 加守卫。
- **新建** `autotest/scripts/smoke_perk_view.json` — 端到端冒烟脚本（真实按钮点击 → 截图核验）。

每个 Task 结束都用 **clang-release 构建成功（0 error）** 作为关卡；Task 4 用截图作为行为验收。

### 构建命令（每个 Task 的关卡，CLAUDE.md 权威配方）

```powershell
# 1) 导入 VS dev 环境（Installer 先入 PATH 消除 vswhere 噪声）
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
# 2) 构建
cmake --build --preset clang-release
```
Expected: 末尾 `PlantsVsZombies.vcxproj -> ...PlantsVsZombies.exe`，0 error / 0 warning。

---

## Task 1: GameScene.h — 声明与成员

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.h`

- [ ] **Step 1: 在 ApplyPerkSelection 声明后加两个方法声明**

定位（约 76–77 行）：
```cpp
	void BeginSurvivalPerkSelect();          // 轮清后弹词条选择框（选卡之前）
	void ApplyPerkSelection(int index);      // index<0 或越界 = 跳过；应用后链式进选卡
```
在其后插入：
```cpp
	void OpenPerkView();                      // 生存模式：弹出已选词条查看面板（固定面板+字号自动缩放，打开即暂停）
	void ClosePerkView();                     // 关闭词条查看面板并恢复
```

- [ ] **Step 2: 在 mSurvivalPerkSelectActive 成员后加三个成员**

定位（约 112–114 行）：
```cpp
	std::weak_ptr<GameMessageBox> mPerkSelectBox;
	std::vector<PerkPairing>      mCurrentPerkOffer;        // 本轮展示的配对（AutoTest dump 用）
	bool                          mSurvivalPerkSelectActive = false;
```
在其后插入：
```cpp
	std::weak_ptr<Button>         mPerkViewButton;          // 生存模式右上角「词条」按钮（仅生存关创建）
	std::weak_ptr<GameMessageBox> mPerkViewBox;             // 词条查看面板
	bool                          mPerkViewActive = false;  // 面板打开中（守卫暂停叠态）
```

- [ ] **Step 3: 构建（关卡）**

Run: 上方「构建命令」。
Expected: 0 error。声明未被引用，链接通过。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/GameScene.h
git commit -m "feat(survival): 词条查看面板的声明与成员"
```

---

## Task 2: GameScene.cpp — OpenPerkView / ClosePerkView + OpenMenu 守卫

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`

- [ ] **Step 1: 在 ApplyPerkSelection 定义之后追加两个方法定义**

定位 `ApplyPerkSelection` 函数结尾（它以 `BeginSurvivalCardSelect();` + `}` 收尾，约 781 行）。在该 `}` 之后、`void GameScene::BeginSurvivalCardSelect()` 之前插入下面整段：

```cpp
void GameScene::OpenPerkView()
{
	if (!mBoard) return;
	// 三向守卫：暂停菜单 / 轮间选词条模态 / 自身已开，均不叠开
	if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive) return;

	mPerkViewActive = true;
	DeltaTime::SetPaused(true);

	auto& pm = mBoard->GetPerkManager();

	// 与 DrawText 同字体量逻辑像素宽（取不到字体时按半宽兜底）
	auto measureW = [](const std::string& s, int fontSize) -> float {
		TTF_Font* f = ResourceManager::GetInstance().GetFont(ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		if (!f) return static_cast<float>(s.size()) * fontSize * 0.5f;
		int w = 0, h = 0;
		TTF_SizeUTF8(f, s.c_str(), &w, &h);
		return static_cast<float>(w);
	};

	const glm::vec4 green{ 53, 191, 61, 255 };
	const glm::vec4 red  { 200, 60, 60, 255 };
	const glm::vec4 titleColor{ 245, 214, 127, 255 };

	// 收集已选词条（stacks>0），按 enum 顺序；descZh 已自带效果描述
	struct Line { std::string text; glm::vec4 color; };
	std::vector<Line> perkLines;
	int distinct = 0, total = 0;
	for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
		PerkType t = static_cast<PerkType>(i);
		int n = pm.GetStacks(t);
		if (n <= 0) continue;
		const PerkInfo& info = SurvivalPerkManager::GetInfo(t);
		++distinct;
		total += n;
		Line ln;
		ln.text  = std::string(u8"· ") + info.descZh + u8"（已选 " + std::to_string(n) + u8" 次）";
		ln.color = (info.category == PerkCategory::PLANT_BUFF) ? green : red;
		perkLines.push_back(ln);
	}

	const std::string title = (distinct > 0)
		? (std::string(u8"已强化：") + std::to_string(distinct) + u8" 种词条 · 累计 " + std::to_string(total) + u8" 层")
		: std::string(u8"尚未选择任何强化词条");

	// 固定面板（逻辑像素，居中于 550,300）
	const float boxW = 560.0f, boxH = 420.0f;
	const float padX = 30.0f, padY = 26.0f;
	const Vector closeBtnSize(160.0f, 44.0f);
	const float closeGap = 18.0f;
	const float cx = static_cast<float>(SCENE_WIDTH)  / 2.0f;
	const float cy = static_cast<float>(SCENE_HEIGHT) / 2.0f;
	const float boxLeft = cx - boxW / 2.0f;
	const float boxTop  = cy - boxH / 2.0f;
	const float innerW  = boxW - 2.0f * padX;
	const float availH  = boxH - 2.0f * padY - closeGap - closeBtnSize.y;
	const int   N       = static_cast<int>(perkLines.size());

	// 字号自动缩放：rowFont 18→10，titleFont=rowFont+4，挑「最大且能塞进固定面板」者；
	// 一路不满足则落到 floor=10（容忍轻微挤压，仍优于溢出可视边框）
	int   rowFont = 10, titleFont = 14;
	float titleLineH = 0.0f, rowLineH = 0.0f, titleGap = 0.0f, rowGap = 0.0f, contentH = 0.0f;
	for (int fnt = 18; fnt >= 10; --fnt) {
		const int   tf  = fnt + 4;
		const float tlh = tf  * 1.4f;
		const float rlh = fnt * 1.4f;
		const float tg  = fnt * 0.9f;
		const float rg  = fnt * 0.5f;
		const float ch  = tlh + (N > 0 ? (tg + N * rlh + (N - 1) * rg) : 0.0f);
		float maxW = measureW(title, tf);
		for (const Line& ln : perkLines) {
			float w = measureW(ln.text, fnt);
			if (w > maxW) maxW = w;
		}
		const bool fits = (ch <= availH) && (maxW <= innerW);
		if (fits || fnt == 10) {
			rowFont = fnt; titleFont = tf;
			titleLineH = tlh; rowLineH = rlh; titleGap = tg; rowGap = rg; contentH = ch;
			if (fits) break;   // 否则 fnt==10 兜底，循环自然结束
		}
	}

	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig>   texts;

	// 内容块在 availH 区域内垂直居中，词条少时不孤悬顶部
	const float blockTop = boxTop + padY + (availH - contentH) / 2.0f;
	const float titleW   = measureW(title, titleFont);
	texts.push_back({ Vector(cx - titleW / 2.0f, blockTop), static_cast<float>(titleFont), title, titleColor });

	float y = blockTop + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		texts.push_back({ Vector(boxLeft + padX, y), static_cast<float>(rowFont), perkLines[i].text, perkLines[i].color });
		y += rowLineH + rowGap;
	}

	// 关闭按钮（底部居中，autoClose 自行帧末销毁面板）
	buttons.push_back({ u8"关闭", Vector(cx - closeBtnSize.x / 2.0f, boxTop + boxH - padY - closeBtnSize.y),
		closeBtnSize, 20, [this]() { this->ClosePerkView(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

	// 背景留空 + explicitSize → 纯色面板（同选词条框，规避墓碑花边内缩导致文字溢出）
	mPerkViewBox = mUIManager.CreateMessageBox(
		Vector(cx, cy), "", buttons, sliders, texts, "", 1.0f, "", Vector(boxW, boxH));
}

void GameScene::ClosePerkView()
{
	mPerkViewActive = false;
	DeltaTime::SetPaused(false);
	// 面板由「关闭」按钮 autoClose=true 自行 Close（延后帧末安全销毁），同 ApplyPerkSelection。
}
```

- [ ] **Step 2: 在 OpenMenu 顶部追加守卫**

定位（约 253–256 行）：
```cpp
void GameScene::OpenMenu()
{
	if (mOpenMenu) return;
	if (mSurvivalPerkSelectActive) return;   // 选词条模态期间禁止打开暂停菜单（否则会在框下解除暂停）
```
在该 `if (mSurvivalPerkSelectActive) return;` 之后插入：
```cpp
	if (mPerkViewActive) return;             // 词条查看面板打开期间禁止叠开暂停菜单
```

- [ ] **Step 3: 构建（关卡）**

Run: 上方「构建命令」。
Expected: 0 error。`OpenPerkView`/`ClosePerkView` 已定义但暂未被调用，链接通过。
若报 `SCENE_WIDTH` / `PerkType` / `TTF_SizeUTF8` 未定义：确认本文件顶部已 `#include "Perk/SurvivalPerkManager.h"`（第 14 行）且与 `BeginSurvivalPerkSelect` 同文件——这些符号在该函数已使用，应无新增 include。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(survival): OpenPerkView/ClosePerkView 实现 + OpenMenu 守卫"
```

---

## Task 3: GameScene.cpp — 创建按钮并接线

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`

- [ ] **Step 1: 在 OnEnter 的速度按钮（button2）块之后、读档之前插入按钮创建**

定位 button2 的 `SetClickCallBack` 闭合（约 183 行 `});`）与注释 `// 读档`（约 185 行）之间，插入：
```cpp
	// 生存模式专属：右上角第三个按钮「词条」，点开已选词条查看面板（普通关卡不创建）
	if (mBoard->mIsSurvival) {
		auto button3 = mUIManager.CreateButton(Vector(990, 95), Vector(125 * 0.9f, 52 * 0.9f));
		mPerkViewButton = button3;
		button3->SetText(u8"词条");
		button3->SetAsCheckbox(false);
		button3->SetTextColor(glm::vec4{ 53, 191, 61, 255 });
		button3->SetHoverTextColor(glm::vec4{ 53, 240, 61, 255 });
		button3->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);
		button3->SetClickCallBack([this](bool) { this->OpenPerkView(); });
	}
```

- [ ] **Step 2: 在 OnExit 的 mMainMenuButton.reset() 旁追加 reset**

定位（约 246–247 行）：
```cpp
	mSpeedSettingsButton.reset();
	mMainMenuButton.reset();
```
在其后插入：
```cpp
	mPerkViewButton.reset();
```

- [ ] **Step 3: 构建（关卡）**

Run: 上方「构建命令」。
Expected: 0 error。按钮回调现已调用 `OpenPerkView`。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(survival): 右上角「词条」按钮接线 + OnExit 清理"
```

---

## Task 4: AutoTest 冒烟脚本 + 行为验收

**Files:**
- Create: `autotest/scripts/smoke_perk_view.json`

- [ ] **Step 1: 写冒烟脚本**

`add_perk` 直接调 `AddPerk`（不触发选卡转场），混入植物增益(绿)与僵尸增难(红)两类；
`click (1046,118)` 命中按钮中心（按钮 top-left (990,95)+size(112.5,46.8)）；
`click (550,462)` 命中关闭按钮中心（top-left (470,440)+size(160,44)）。

```json
{
  "commands": [
    { "op": "goto_level", "level": 1000 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "add_perk", "type": "PLANT_DAMAGE_UP", "count": 3 },
    { "op": "add_perk", "type": "ZOMBIE_HEALTH_UP", "count": 2 },
    { "op": "add_perk", "type": "ZOMBIE_DAMAGE_UP", "count": 1 },
    { "op": "click", "x": 1046, "y": 118 },
    { "op": "wait_frames", "frames": 2 },
    { "op": "screenshot", "name": "perk_view_open.png" },
    { "op": "click", "x": 550, "y": 462 },
    { "op": "wait_frames", "frames": 2 },
    { "op": "screenshot", "name": "perk_view_closed.png" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 运行脚本**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_view.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
Expected: `$LASTEXITCODE` = 0；产物在 `build\clang-release\autotest\out\smoke_perk_view\`。

- [ ] **Step 3: Read 截图核验**

Read `build\clang-release\autotest\out\smoke_perk_view\perk_view_open.png`：
- 屏幕中央纯色面板；
- 顶部金色总计行 `已强化：3 种词条 · 累计 6 层`；
- 词条明细行：`植物伤害 +10%/层` 行为**绿色**、两条僵尸行为**红色**，各带「已选 N 次」（3/2/1）；
- 文字全部在面板可视边框内、无溢出；底部「关闭」按钮在位。

Read `build\clang-release\autotest\out\smoke_perk_view\perk_view_closed.png`：
- 面板已消失，回到正常生存对局画面（右上角「词条」按钮仍在）。

若 `perk_view_open.png` 无面板：多半是点击未命中按钮——核对按钮 top-left=(990,95)/size=(112.5,46.8) 与点击中心 (1046,118)；必要时改点 (1040,118)。若仍不稳，回退用直接调用核验（在脚本中临时换用一个直调 `OpenPerkView` 的钩子排查），但**最终验收必须是真实 `click` 命中按钮**（主人要的就是这个按钮）。

- [ ] **Step 4: Commit**

```bash
git add autotest/scripts/smoke_perk_view.json
git commit -m "test(autotest): smoke_perk_view 端到端验证词条查看面板"
```

---

## 收尾

四个 Task 全绿后：clang-release 0 error/0 warning，`smoke_perk_view` 双截图核验通过（面板内容/配色/字号适配/开关），普通关卡（非 1000）无新按钮、行为零改动。按 CLAUDE.md 提交策略，任务完成即 commit；**push 等主人指示**。

可选后续（本计划不含）：`dump_state` 增 `perkView` 字段（已选词条 key + 次数列表）以便断言化——目前截图核验已足够。
