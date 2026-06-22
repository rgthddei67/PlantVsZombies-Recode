# 生存模式词条查看面板 — 分页扩展 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给生存模式「词条」查看面板加分页：每页最多 5 个 distinct 词条，多于则分页，按需出现「上一页」/「下一页」按钮，标题加「（第 m/n 页）」，其余行为不变。

**Architecture:** 把 `OpenPerkView` 的「建框主体」抽到新私有方法 `RenderPerkViewPage()`，由新成员 `mPerkViewPage` 驱动当前页；翻页按钮在回调里改页号并**原地重建**面板（依赖 `GameMessageBox::Close()` 帧末延后销毁的既有契约，新框先建、旧框后自销，安全）。`OpenPerkView` 瘦身为「守卫+暂停+置页 0+RenderPerkViewPage」入口，`ClosePerkView` 不变。

**Tech Stack:** C++17，SDL2_ttf 量宽，自研 `GameMessageBox`/`UIManager` 纯色面板，AutoTest（`-AutoTest` JSON + 截图 Read）验证。无 C++ 单测框架——本仓库的「测试」即 AutoTest 脚本 + clang-release 零警告构建 + 逐张截图人工核验。

**测试说明（重要）：** 本仓库无断言式单测，故每个改代码的 Task 以「构建零警告 + 跑 AutoTest + Read 截图核对预期」作为验收。Task 1 是纯无行为变更重构，用**既有** `smoke_perk_view` 回归（截图须与改前一致）；Task 2/3 用扩展后的脚本验证分页路径。

---

## 现状锚点（实现者必读）

- 落点文件：`PlantVsZombies/Game/GameScene.cpp`（`OpenPerkView` 现位于 `:798`，`ClosePerkView` 现位于 `:905`）、`PlantVsZombies/Game/GameScene.h`、`autotest/scripts/smoke_perk_view.json`。
- `GameScene.cpp` 顶部 include 现只有 `<cmath>`/`<cstdio>`（无 `<algorithm>`）→ 本计划 Task 2 Step 0 补 `#include <algorithm>` 后用 `std::min`（主人已批准加头文件）。
- 面板几何（逻辑坐标，沿用现值）：`boxW=560, boxH=420, padX=30, padY=26, cx=550, cy=300`；
  `boxLeft=cx-boxW/2=270`，`boxRight=cx+boxW/2=830`，`boxTop=cy-boxH/2=90`。
  底部按钮行 `btnY = boxTop+boxH-padY-closeBtnSize.y = 90+420-26-44 = 440`。
- 按钮中心（AutoTest click 用）：
  - 「词条」按钮 `(990,95)` 尺寸 `(112.5,46.8)` → 中心 `≈(1046,118)`（既有脚本已用此值）。
  - 「关闭」`160×44` 居中 → 中心 `(550,462)`（既有脚本已用此值，分页后仍居中不变）。
  - 「上一页」`110×44` 左 x=`boxLeft+padX=300` → 中心 `(355,462)`。
  - 「下一页」`110×44` 右 x=`boxRight-padX-110=690` → 中心 `(745,462)`。
- 全部 6 个词条类型（`PerkType::COUNT==6`）：`PLANT_DAMAGE_UP, ZOMBIE_HEALTH_UP, ZOMBIE_DAMAGE_RESIST, ZOMBIE_DAMAGE_UP, ZOMBIE_INVULN_HITS, PLANT_REGEN`。全加齐 → `ceil(6/5)=2` 页（5+1）。
- AutoTest 直接喂词条的 op：`{ "op":"add_perk", "type":"<PerkType名>", "count":N }`（直调 `AddPerk`，无需走选词条 UI）。
- 构建（VS dev env + clang-release，须零警告零错误）：

  ```powershell
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
  cmake --build --preset clang-release
  ```

- 跑 AutoTest（工作目录=exe 所在）：

  ```powershell
  Push-Location build\clang-release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_view.json -Seed 42
  $LASTEXITCODE   # 0=成功
  Pop-Location
  ```

  截图产物在 `build\clang-release\autotest\out\smoke_perk_view\*.png`，用 Read 工具直接看。

---

## Task 1: 重构 —— 抽出 `RenderPerkViewPage()`（零行为变更）

把 `OpenPerkView` 的建框主体原样迁入新方法 `RenderPerkViewPage()`，`OpenPerkView` 只留「守卫+暂停+`mPerkViewPage=0`+调用」。此步**不**改任何显示行为，用既有 `smoke_perk_view` 回归。

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.h`（加成员与方法声明）
- Modify: `PlantVsZombies/Game/GameScene.cpp:798-903`（拆 `OpenPerkView`）

- [ ] **Step 1: 头文件加方法声明与页号成员**

`GameScene.h` 中 `void ClosePerkView();`（现 `:79`）之后加一行声明：

```cpp
	void ClosePerkView();                     // 关闭词条查看面板并恢复
	void RenderPerkViewPage();                // 按 mPerkViewPage 重建词条查看面板（分页；OpenPerkView 主体）
```

并在私有成员区 `bool mPerkViewActive = false;`（现 `:119`）之后加：

```cpp
	bool                          mPerkViewActive = false;  // 面板打开中（守卫暂停叠态）
	int                           mPerkViewPage   = 0;      // 词条查看面板当前页（0-based）
```

- [ ] **Step 2: 拆分 `OpenPerkView`，主体迁入 `RenderPerkViewPage`**

把 `GameScene.cpp` 现有的整个 `OpenPerkView`（`:798-903`）替换为下面**两个**函数。本步代码逐字等于原主体，仅：把 `OpenPerkView` 的「守卫+置 active+暂停」后面全部内容剪到 `RenderPerkViewPage`，并在 `OpenPerkView` 末尾加 `mPerkViewPage = 0; RenderPerkViewPage();`。

```cpp
void GameScene::OpenPerkView()
{
	if (!mBoard) return;
	// 三向守卫：暂停菜单 / 轮间选词条模态 / 自身已开，均不叠开
	if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive) return;

	mPerkViewActive = true;
	DeltaTime::SetPaused(true);
	mPerkViewPage = 0;
	RenderPerkViewPage();
}

void GameScene::RenderPerkViewPage()
{
	if (!mBoard) return;
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
```

`ClosePerkView`（`:905`）保持原样不动。

- [ ] **Step 3: 构建（零警告）**

Run（见「现状锚点」构建块）：`cmake --build --preset clang-release`
Expected: 0 warning / 0 error，链接出 `build\clang-release\PlantsVsZombies.exe`。

- [ ] **Step 4: 回归 —— 跑既有 smoke_perk_view，截图须与改前一致**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_view.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected: `$LASTEXITCODE == 0`。Read `build\clang-release\autotest\out\smoke_perk_view\perk_view_open.png`：标题「已强化：3 种词条 · 累计 6 层」，3 行词条，底部单个「关闭」（**无**翻页按钮、**无**页码后缀——此脚本只 3 个 distinct，单页）。`perk_view_closed.png`：面板已消失、回到游戏。与重构前行为一致即通过。

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/GameScene.h PlantVsZombies/Game/GameScene.cpp
git commit -m "refactor(survival): 词条查看面板建框主体抽出 RenderPerkViewPage（零行为变更）"
```

---

## Task 2: 分页逻辑 + 上一页/下一页按钮

在 `RenderPerkViewPage` 内加分页：每页 ≤5 行、标题页码、底部一行三按钮。

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`（顶部 include + `RenderPerkViewPage` 内）

- [ ] **Step 0: 补 `#include <algorithm>`（for `std::min`）**

在 `GameScene.cpp` 顶部 include 区，把：

```cpp
#include <cmath>
#include <cstdio>
```

改为：

```cpp
#include <cmath>
#include <cstdio>
#include <algorithm>
```

- [ ] **Step 1: 收集 perkLines 之后、组装 title 之前，插入分页量计算**

定位 `RenderPerkViewPage` 中 `perkLines.push_back(ln); }`（收集循环的右花括号）与其下 `const std::string title = ...` 之间，插入：

```cpp
	// 分页：每页最多 5 个 distinct 词条（distinct == perkLines.size()）
	constexpr int kPerksPerPage = 5;
	const int totalPages = (distinct > 0) ? ((distinct + kPerksPerPage - 1) / kPerksPerPage) : 1;
	if (mPerkViewPage < 0) mPerkViewPage = 0;
	if (mPerkViewPage > totalPages - 1) mPerkViewPage = totalPages - 1;
	const int pageStart = mPerkViewPage * kPerksPerPage;
	const int pageEnd   = std::min(distinct, pageStart + kPerksPerPage);
```

- [ ] **Step 2: 标题加页码（仅多页）；把 title 改为可变 + N 改为本页行数**

把原 `const std::string title = (distinct > 0) ? (...) : (...);` 改成去掉 `const`，并在其后追加页码段；同时把 `const int N = static_cast<int>(perkLines.size());` 改为本页行数。

将这两处：

```cpp
	const std::string title = (distinct > 0)
		? (std::string(u8"已强化：") + std::to_string(distinct) + u8" 种词条 · 累计 " + std::to_string(total) + u8" 层")
		: std::string(u8"尚未选择任何强化词条");
```

替换为：

```cpp
	std::string title = (distinct > 0)
		? (std::string(u8"已强化：") + std::to_string(distinct) + u8" 种词条 · 累计 " + std::to_string(total) + u8" 层")
		: std::string(u8"尚未选择任何强化词条");
	if (totalPages > 1)
		title += std::string(u8"（第 ") + std::to_string(mPerkViewPage + 1) + u8"/" + std::to_string(totalPages) + u8" 页）";
```

并把：

```cpp
	const int   N       = static_cast<int>(perkLines.size());
```

替换为：

```cpp
	const int   N       = pageEnd - pageStart;   // 本页行数（≤5）
```

- [ ] **Step 3: 字号自适应循环改为只量本页切片**

把字号循环里的：

```cpp
		float maxW = measureW(title, tf);
		for (const Line& ln : perkLines) {
			float w = measureW(ln.text, fnt);
			if (w > maxW) maxW = w;
		}
```

替换为（遍历 `[pageStart, pageEnd)`）：

```cpp
		float maxW = measureW(title, tf);
		for (int li = pageStart; li < pageEnd; ++li) {
			float w = measureW(perkLines[li].text, fnt);
			if (w > maxW) maxW = w;
		}
```

- [ ] **Step 4: 词条行布局改为取本页切片**

把：

```cpp
	float y = blockTop + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		texts.push_back({ Vector(boxLeft + padX, y), static_cast<float>(rowFont), perkLines[i].text, perkLines[i].color });
		y += rowLineH + rowGap;
	}
```

替换为：

```cpp
	float y = blockTop + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		const Line& ln = perkLines[pageStart + i];
		texts.push_back({ Vector(boxLeft + padX, y), static_cast<float>(rowFont), ln.text, ln.color });
		y += rowLineH + rowGap;
	}
```

- [ ] **Step 5: 底部按钮：关闭（恒显居中）+ 上一页/下一页（按需）**

把原来的单个关闭按钮：

```cpp
	// 关闭按钮（底部居中，autoClose 自行帧末销毁面板）
	buttons.push_back({ u8"关闭", Vector(cx - closeBtnSize.x / 2.0f, boxTop + boxH - padY - closeBtnSize.y),
		closeBtnSize, 20, [this]() { this->ClosePerkView(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, true });
```

替换为三按钮一行（新增 `boxRight`/`navBtnSize`/`btnY` 局部量）：

```cpp
	// 底部一行三按钮：上一页（左·仅非首页）· 关闭（中·恒显）· 下一页（右·仅有下一页）
	const float    boxRight   = cx + boxW / 2.0f;
	const Vector   navBtnSize(110.0f, 44.0f);
	const float    btnY       = boxTop + boxH - padY - closeBtnSize.y;

	buttons.push_back({ u8"关闭", Vector(cx - closeBtnSize.x / 2.0f, btnY),
		closeBtnSize, 20, [this]() { this->ClosePerkView(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

	if (mPerkViewPage > 0) {
		buttons.push_back({ u8"上一页", Vector(boxLeft + padX, btnY), navBtnSize, 18,
			[this]() { --mPerkViewPage; RenderPerkViewPage(); },
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	}
	if (mPerkViewPage < totalPages - 1) {
		buttons.push_back({ u8"下一页", Vector(boxRight - padX - navBtnSize.x, btnY), navBtnSize, 18,
			[this]() { ++mPerkViewPage; RenderPerkViewPage(); },
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	}
```

> 翻页回调先 `RenderPerkViewPage()`（建新框、覆写 `mPerkViewBox`），再由 `autoClose=true` 让旧框 `Close()`（帧末延后销毁，安全）。回调全程不碰 `mPerkViewActive`/暂停。

- [ ] **Step 6: 构建（零警告）**

Run: `cmake --build --preset clang-release`
Expected: 0 warning / 0 error。（特别留意 `-Wunused-variable`：`pageEnd`/`boxRight`/`navBtnSize`/`btnY` 均被使用。）

- [ ] **Step 7: 提交**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "feat(survival): 词条查看面板分页（每页5个+上一页/下一页+页码）"
```

---

## Task 3: AutoTest 覆盖分页路径 + 截图核验

扩展 `smoke_perk_view.json`，凑 6 个 distinct 词条（2 页），走「开→下一页→上一页→关闭」并逐张截图。

**Files:**
- Modify: `autotest/scripts/smoke_perk_view.json`

- [ ] **Step 1: 改写脚本，加齐 6 词条并覆盖翻页**

把 `autotest/scripts/smoke_perk_view.json` 整体替换为：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1000 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME" },
    { "op": "add_perk", "type": "PLANT_DAMAGE_UP", "count": 3 },
    { "op": "add_perk", "type": "ZOMBIE_HEALTH_UP", "count": 2 },
    { "op": "add_perk", "type": "ZOMBIE_DAMAGE_UP", "count": 1 },
    { "op": "add_perk", "type": "ZOMBIE_DAMAGE_RESIST", "count": 1 },
    { "op": "add_perk", "type": "ZOMBIE_INVULN_HITS", "count": 1 },
    { "op": "add_perk", "type": "PLANT_REGEN", "count": 1 },
    { "op": "click", "x": 1046, "y": 118 },
    { "op": "wait_frames", "value": 15 },
    { "op": "screenshot", "name": "perk_view_page1.png" },
    { "op": "click", "x": 745, "y": 462 },
    { "op": "wait_frames", "value": 15 },
    { "op": "screenshot", "name": "perk_view_page2.png" },
    { "op": "click", "x": 355, "y": 462 },
    { "op": "wait_frames", "value": 15 },
    { "op": "screenshot", "name": "perk_view_page1_again.png" },
    { "op": "click", "x": 550, "y": 462 },
    { "op": "wait_frames", "value": 15 },
    { "op": "screenshot", "name": "perk_view_closed.png" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 跑脚本**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_view.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected: `$LASTEXITCODE == 0`。

- [ ] **Step 3: Read 四张截图核验**

依次 Read `build\clang-release\autotest\out\smoke_perk_view\` 下：
- `perk_view_page1.png`：标题含「已强化：6 种词条 · 累计 9 层（第 1/2 页）」；5 行词条；底部右侧有「下一页」、**无**「上一页」、中间「关闭」。
- `perk_view_page2.png`：标题含「（第 2/2 页）」；1 行词条；底部左侧有「上一页」、**无**「下一页」、中间「关闭」。
- `perk_view_page1_again.png`：与 `perk_view_page1.png` 一致（回到第 1 页）。
- `perk_view_closed.png`：面板消失、回到游戏画面。

任一不符 → 回 Task 2 调几何/条件（不要改本脚本去迁就 bug）。

- [ ] **Step 4: 提交**

```bash
git add autotest/scripts/smoke_perk_view.json
git commit -m "test(autotest): smoke_perk_view 覆盖词条面板分页翻页"
```

---

## Self-Review（写计划后自查）

**1. Spec coverage：**
- 每页≤5 distinct（重复折叠）→ Task 2 Step 1（`kPerksPerPage=5`，distinct==perkLines.size()，重复早已折叠为「已选 N 次」）。✅
- 多于 5 分页 → Task 2 Step 1（totalPages）。✅
- 有下一页才显「下一页」/ 非首页才显「上一页」→ Task 2 Step 5（`mPerkViewPage<totalPages-1` / `>0`）。✅
- 标题「（第 m/n 页）」仅多页 → Task 2 Step 2。✅
- 一行三按钮布局（300/中/690）→ Task 2 Step 5 + 现状锚点几何。✅
- 其余不变（守卫/暂停/尺寸/0 词条/OpenMenu）→ Task 1 纯重构保形 + Task 2 不动这些路径；Task 1 Step 4 回归 3 词条单页。✅
- 重建安全性 → Task 2 Step 5 注释 + 架构段。✅
- 验证 6 词条 2 页闭环 → Task 3。✅

**2. Placeholder scan：** 无 TBD/TODO；所有改代码步均给完整 before/after 代码块与精确行锚点。✅

**3. Type/命名一致性：** `RenderPerkViewPage()`、`mPerkViewPage`、`kPerksPerPage`、`pageStart/pageEnd`、`totalPages`、`navBtnSize`、`boxRight`、`btnY` 全计划一致；`std::min` 用 `<algorithm>`（Task 2 Step 0 已加）；`perkLines`/`distinct`/`total`/`N` 沿用原名。✅
