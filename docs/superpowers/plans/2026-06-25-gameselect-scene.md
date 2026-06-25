# GameSelectScene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增一个继承自 `Scene` 的 `GameSelectScene`，纯视觉复刻参考图（PvZ 挑战模式选择界面）：羊皮纸背景 + 顶部「选择关卡」标题 + 6+3 共 9 张关卡卡片网格（可悬停高亮、点击为占位）+ 左下「返回菜单」按钮。仅注册场景，主菜单入口暂不接。

**Architecture:** 仿照 `AlmanacScene` 的最小结构——重写 `BuildDrawCommands()`（布局）/`Update()`（切场景）/`OnEnter()`/`OnExit()`（生命周期）四个虚函数。背景图与卡片框是已存在的项目资源（`Challenge_Background.png` / `Challenge_Window(_Highlight).png`），键由 `ResourceManager` 从文件名自动派生。切场景走「`Update()` 里检测标志位再 `SwitchTo`」的既有惯例，不在按钮回调里直接销毁场景。

**Tech Stack:** C++17 / 自研 `Scene` + `UIManager`/`Button` UI 系统 / Vulkan `Graphics` / CMake `clang-release` 预设。

**验证方式说明（重要）：** 本仓库**无单元测试框架**。既定验证 = ①`clang-release` 构建 0 warning / 0 error；②`-AutoTest` 脚本驱动截图，用 Read 工具肉眼核对布局。因场景入口暂不接，截图验证靠**临时把启动场景重定向到 `GameSelectScene`**，验完**还原**、不进 commit（见 Task 5）。

**构建环境（每次 build 前先导入 VS 开发环境，下文以 `<BUILD>` 代指本段）：**
```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --build --preset clang-release
```
构建产物在 `build/clang-release/PlantsVsZombies.exe`。

---

## File Structure

| 文件 | 责任 | 动作 |
|---|---|---|
| `PlantVsZombies/Game/GameSelectScene.h` | 场景类声明（成员 + 4 个虚函数） | Create |
| `PlantVsZombies/Game/GameSelectScene.cpp` | 布局/切场景/生命周期实现 | Create |
| `PlantVsZombies/ResourceKeys.h` | 补 3 个 `IMAGE_CHALLENGE_*` 防手滑常量 | Modify（`namespace Textures` 内） |
| `PlantVsZombies/GameApp.cpp` | include + 注册场景 | Modify（include 段 + 注册段） |
| `autotest/scripts/gameselect_smoke.json` | 启动→等帧→截图→退出的最小验证脚本 | Create |

注：`CMakeLists.txt` 用 `GLOB_RECURSE CONFIGURE_DEPENDS`，新增 `.cpp` 无需改构建文件；但**新增源文件后需重新 `cmake --preset clang-release` configure 一次**让 glob 重新收集（仅 Task 2 需要，之后改内容只 build）。

---

## Task 1: 补资源键常量

**Files:**
- Modify: `PlantVsZombies/ResourceKeys.h`（`namespace Textures` 内，锚点 `RKEY(IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT);` 之后）

- [ ] **Step 1: 加入三个 RKEY 常量**

在 `ResourceKeys.h` 中找到这一行：
```cpp
		RKEY(IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT);
```
在其**紧接下一行**插入：
```cpp
		// 挑战模式选择界面（GameSelectScene）——键由 Challenge_*.png 文件名自动派生
		RKEY(IMAGE_CHALLENGE_BACKGROUND);
		RKEY(IMAGE_CHALLENGE_WINDOW);
		RKEY(IMAGE_CHALLENGE_WINDOW_HIGHLIGHT);
```

- [ ] **Step 2: 构建验证（确认头文件不破坏编译）**

`<BUILD>`
Run: 上述 `<BUILD>` 段
Expected: 链接成功，0 warning / 0 error（此步只增加未被引用的常量，必然通过；作为安全网）。

- [ ] **Step 3: Commit**

```powershell
git add PlantVsZombies/ResourceKeys.h
git commit -m "feat(ui): 补 IMAGE_CHALLENGE_* 资源键常量(GameSelectScene 用)"
```

---

## Task 2: 场景骨架（背景 + 返回按钮）+ 注册

本任务建出可编译、已注册的场景，先只铺背景和返回菜单按钮（最小可验证单元）。

**Files:**
- Create: `PlantVsZombies/Game/GameSelectScene.h`
- Create: `PlantVsZombies/Game/GameSelectScene.cpp`
- Modify: `PlantVsZombies/GameApp.cpp`（include 段第 10–13 行附近；注册段第 309–313 行附近）

- [ ] **Step 1: 写头文件 `GameSelectScene.h`**

```cpp
#pragma once
#ifndef _GAME_SELECT_SCENE_H
#define _GAME_SELECT_SCENE_H

#include "Scene.h"
#include <vector>
#include <memory>

class GameSelectScene : public Scene {
private:
	std::shared_ptr<Button> mBackMenuButton;
	std::vector<std::shared_ptr<Button>> mCards;   // 9 张占位关卡卡片
	bool mReadyToSwitchMainMenu = false;

public:
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

protected:
	void BuildDrawCommands() override;
};

#endif
```

- [ ] **Step 2: 写实现 `GameSelectScene.cpp`（骨架版：背景 + 返回按钮）**

```cpp
#include "GameSelectScene.h"
#include "SceneManager.h"
#include "../GameApp.h"
#include "AudioSystem.h"
#include "../Logger.h"

void GameSelectScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();

	// 背景：Challenge_Background.png 为 1280x720，缩放铺满 1100x600 逻辑画面
	AddTexture(ResourceKeys::Textures::IMAGE_CHALLENGE_BACKGROUND,
		0.0f, 0.0f, 1100.0f / 1280.0f, 600.0f / 720.0f, -1000, false);

	// 左下角「返回菜单」按钮（位置/尺寸/样式照搬 AlmanacScene）
	mBackMenuButton = mUIManager.CreateButton(Vector(7, 560), Vector(162, 26));
	mBackMenuButton->SetAsCheckbox(false);
	mBackMenuButton->SetImageKeys(
		"IMAGE_ALMANAC_INDEXBUTTON",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT");
	mBackMenuButton->SetText(u8"返回菜单", ResourceKeys::Fonts::FONT_FZJZ, 18);
	mBackMenuButton->SetTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetHoverTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetClickCallBack([this](bool) {
		this->mReadyToSwitchMainMenu = true;
		});

	SortDrawCommands();
}

void GameSelectScene::Update()
{
	Scene::Update();

	if (mReadyToSwitchMainMenu) {
		mReadyToSwitchMainMenu = false;
		SceneManager::GetInstance().SwitchTo("MainMenuScene");
		return;
	}
}

void GameSelectScene::OnEnter()
{
	Scene::OnEnter();
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);
}

void GameSelectScene::OnExit()
{
	mBackMenuButton.reset();
	mCards.clear();
	Scene::OnExit();
}
```

- [ ] **Step 3: 在 `GameApp.cpp` include 段加入头文件**

找到（第 10–13 行附近）：
```cpp
#include "./Game/MainMenuScene.h"
```
在其下一行加入：
```cpp
#include "./Game/GameSelectScene.h"
```

- [ ] **Step 4: 在 `GameApp.cpp` 注册段注册场景**

找到（第 309–313 行附近）：
```cpp
	sceneManager.RegisterScene<MainMenuScene>("MainMenuScene");
```
在其下一行加入：
```cpp
	sceneManager.RegisterScene<GameSelectScene>("GameSelectScene");
```

- [ ] **Step 5: 重新 configure（让 glob 收集新 .cpp）+ 构建**

```powershell
# 先导入 VS 环境（<BUILD> 段前半），然后：
cmake --preset clang-release
cmake --build --preset clang-release
```
Expected: configure 重新收集到 `GameSelectScene.cpp`；链接成功，0 warning / 0 error。

- [ ] **Step 6: Commit**

```powershell
git add PlantVsZombies/Game/GameSelectScene.h PlantVsZombies/Game/GameSelectScene.cpp PlantVsZombies/GameApp.cpp
git commit -m "feat(ui): GameSelectScene 骨架(背景+返回按钮)并注册到 SceneManager"
```

---

## Task 3: 9 张关卡卡片网格（6+3，含悬停高亮）

**Files:**
- Modify: `PlantVsZombies/Game/GameSelectScene.cpp`（`BuildDrawCommands()` 内，`SortDrawCommands();` 之前）

- [ ] **Step 1: 在 `BuildDrawCommands()` 里、返回按钮之后、`SortDrawCommands();` 之前插入卡片网格生成**

```cpp
	// ===== 9 张占位关卡卡片：第 1 行 6 张、第 2 行 3 张（复刻参考图） =====
	const float kCardScale = 0.95f;           // Challenge_Window 原始 118x120
	const float kPitchX = 150.0f;             // 列间距
	const float kCol0X = 115.0f;              // 第一列左上角 x
	const float kRow1Y = 150.0f;              // 第 1 行 y
	const float kRow2Y = 320.0f;              // 第 2 行 y
	const float kCardW = 118.0f * kCardScale;
	const float kCardH = 120.0f * kCardScale;

	auto makeCard = [this, kCardW, kCardH](int index, float x, float y) {
		auto card = mUIManager.CreateButton(Vector(x, y), Vector(kCardW, kCardH));
		card->SetAsCheckbox(false);
		card->SetImageKeys(
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT);
		card->SetClickCallBack([index](bool) {
			// 纯视觉脚手架：点击仅打日志，不跳转（Release 下 INFO 被裁，等同 no-op）
			LOG_INFO("UI") << "GameSelectScene card " << index << " clicked";
			});
		mCards.push_back(card);
	};

	for (int i = 0; i < 6; ++i) {             // 第 1 行 6 张
		makeCard(i, kCol0X + kPitchX * i, kRow1Y);
	}
	for (int i = 0; i < 3; ++i) {             // 第 2 行 3 张（左对齐前 3 列）
		makeCard(6 + i, kCol0X + kPitchX * i, kRow2Y);
	}
```

- [ ] **Step 2: 构建验证**

`<BUILD>`
Expected: 0 warning / 0 error。（lambda 捕获 `kCardW/kCardH` 按值；`index` 按值——确认无 `-Wunused`/捕获告警。）

- [ ] **Step 3: Commit**

```powershell
git add PlantVsZombies/Game/GameSelectScene.cpp
git commit -m "feat(ui): GameSelectScene 加 6+3 关卡卡片网格(悬停高亮+占位点击)"
```

---

## Task 4: 顶部标题「选择关卡」+ 9 个占位标签

**Files:**
- Modify: `PlantVsZombies/Game/GameSelectScene.cpp`（`BuildDrawCommands()` 内，`SortDrawCommands();` 之前；并复用 Task 3 的卡片坐标常量）

- [ ] **Step 1: 把卡片坐标常量提取为成员可见的局部，并注册文字绘制命令**

在 Task 3 插入的卡片网格代码**之后**、`SortDrawCommands();` **之前**，插入文字绘制命令。它在 lambda 内重算卡片中心，避免依赖循环局部变量：

```cpp
	// ===== 顶部标题 + 9 个占位标签（每帧绘制；坐标为起始值，按截图微调） =====
	RegisterDrawCommand("DrawSelectTexts",
		[kCol0X, kPitchX, kRow1Y, kRow2Y, kCardW, kCardH](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();

			// 顶部标题：居中于背景横幅之上（x/y 起始值，截图后微调）
			gameApp.DrawText(u8"选择关卡", Vector(470, 28),
				glm::vec4(248, 236, 122, 255));

			// 9 个占位标签「关卡N」，画在每张卡下方小条中央
			static const char* kLabels[9] = {
				u8"关卡一", u8"关卡二", u8"关卡三", u8"关卡四", u8"关卡五",
				u8"关卡六", u8"关卡七", u8"关卡八", u8"关卡九" };
			const float kLabelFontApproxW = 13.0f;   // 估算每个汉字宽度，用于水平居中
			auto drawLabel = [&](int index, float x, float y) {
				float cx = x + kCardW * 0.5f;
				float cy = y + kCardH - 18.0f;        // 下方小条
				float textW = 3 * kLabelFontApproxW;  // "关卡N" 3 字
				gameApp.DrawText(kLabels[index], Vector(cx - textW * 0.5f, cy),
					glm::vec4(70, 60, 40, 255));
			};
			for (int i = 0; i < 6; ++i) drawLabel(i, kCol0X + kPitchX * i, kRow1Y);
			for (int i = 0; i < 3; ++i) drawLabel(6 + i, kCol0X + kPitchX * i, kRow2Y);
		},
		LAYER_UI + 100);
```

> 注意：`kCol0X` 等常量在 Task 3 中是 `BuildDrawCommands()` 的局部 `const`，本 lambda 在同函数内、`SortDrawCommands()` 之前定义，按值捕获它们即可。

- [ ] **Step 2: 构建验证**

`<BUILD>`
Expected: 0 warning / 0 error。

- [ ] **Step 3: Commit**

```powershell
git add PlantVsZombies/Game/GameSelectScene.cpp
git commit -m "feat(ui): GameSelectScene 加顶部标题与 9 个占位标签"
```

---

## Task 5: 截图验证 + 坐标微调 + 收尾

本任务用临时重定向把场景跑起来截图，核对并微调坐标，**还原重定向**后提交。

**Files:**
- Create: `autotest/scripts/gameselect_smoke.json`
- Modify（临时、最终还原）：`PlantVsZombies/GameApp.cpp`（`SwitchTo` 启动行）
- Modify（如需微调）：`PlantVsZombies/Game/GameSelectScene.cpp`（坐标常量）

- [ ] **Step 1: 写最小验证脚本 `autotest/scripts/gameselect_smoke.json`**

```json
{
  "name": "gameselect_smoke",
  "commands": [
    { "op": "wait_frames", "value": 60 },
    { "op": "screenshot", "name": "gameselect" },
    { "op": "wait_frames", "value": 5 },
    { "op": "quit" }
  ]
}
```
> 字段名是 `"value"` 不是 `"frames"`（写错会静默当 0 立即完成）。脚本不依赖入口，启动后截当前场景。

- [ ] **Step 2: 临时把启动场景重定向到 GameSelectScene**

在 `GameApp.cpp` 找到：
```cpp
	sceneManager.SwitchTo("MainMenuScene");
```
**临时**改为：
```cpp
	sceneManager.SwitchTo("GameSelectScene");
```
（这行**不会进任何 commit**，Step 7 还原。）

- [ ] **Step 3: 构建 + 跑脚本截图**

```powershell
# 先 <BUILD> 导入环境并 cmake --build --preset clang-release
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\gameselect_smoke.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
Expected: 退出码 0，产物在 `build/clang-release/autotest/out/gameselect_smoke/gameselect.png`。

- [ ] **Step 4: 用 Read 工具看截图核对**

Read: `build/clang-release/autotest/out/gameselect_smoke/gameselect.png`
核对清单：
- 羊皮纸背景铺满画面、顶部撕纸横幅可见。
- 标题「选择关卡」落在横幅上、大致居中。
- 6 张卡片整齐排第 1 行、3 张排第 2 行（左对齐），不溢出、不重叠。
- 每张卡下方有「关卡N」标签、大致居中。
- 左下角「返回菜单」按钮可见。

- [ ] **Step 5: 按需微调坐标并复验**

若标题/标签/卡片偏位：编辑 `GameSelectScene.cpp` 中 `kCol0X`/`kPitchX`/`kRow1Y`/`kRow2Y`/标题 `Vector(470,28)`/`kLabelFontApproxW` 等常量 → 重新 `cmake --build --preset clang-release` → 重跑 Step 3 → 重看 Step 4。重复至布局与参考图整体一致。

- [ ] **Step 6:（可选）验证卡片悬停高亮**

在脚本里临时加一条 `{ "op": "click", "x": <某卡中心x>, "y": <某卡中心y>, "hold_frames": 10 }` 并在 hold 期间 `screenshot`，确认该卡切到蓝色高亮框；确认后移除该临时命令（或保留一张验证截图说明）。悬停机制与 AlmanacScene 返回按钮同款，主要靠此步抽查。

- [ ] **Step 7: 还原启动场景重定向**

把 `GameApp.cpp` 的：
```cpp
	sceneManager.SwitchTo("GameSelectScene");
```
改回：
```cpp
	sceneManager.SwitchTo("MainMenuScene");
```

- [ ] **Step 8: 终验构建（确认还原后仍 0 warning）+ 确认 diff 干净**

```powershell
# <BUILD> 构建
git diff -- PlantVsZombies/GameApp.cpp
```
Expected: 0 warning / 0 error；`git diff GameApp.cpp` **只**剩 include + 注册两处改动，`SwitchTo` 行已是 `MainMenuScene`（重定向已还原，不在 diff 中）。

- [ ] **Step 9: Commit（脚本 + 任何坐标微调）**

```powershell
git add autotest/scripts/gameselect_smoke.json PlantVsZombies/Game/GameSelectScene.cpp
git commit -m "test(ui): GameSelectScene 截图验证脚本并定稿布局坐标"
```

---

## 完成判据（对齐 spec §8 验收标准）

- [ ] `clang-release` 构建 0 warning / 0 error（Task 2/3/4/5 各 build 均过）。
- [ ] 截图布局与参考图整体一致：顶部「选择关卡」标题、6+3 卡片网格、左下「返回菜单」按钮、羊皮纸背景。
- [ ] 卡片悬停切到高亮框（Task 5 Step 6 抽查）；点击不崩。
- [ ] 「返回菜单」逻辑置 `mReadyToSwitchMainMenu` → `Update()` 切回 `MainMenuScene`（代码路径与 AlmanacScene 同款）。
- [ ] 启动场景重定向已还原（`git diff` 确认）；除三常量 + include + 注册行外不改既有场景。
- [ ] spec/plan 已入库；实现 commit 在 master（未 push，等主人指示）。
