# GameMessageBox Builder 化简 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `GameMessageBox::Builder` 流式构建器并迁移全部 7 个调用点，消灭 9 参构造调用与隐式规则（空key+explicitSize=纯色面板、CHECKBOX 纹理嗅探、friend 槽位戳勾选态）。

**Architecture:** Builder 是纯新增的嵌套类（字段即攒下的构造参数，`Show()` 组装并经 GameObjectManager 创建）；`GameMessageBox` 本体唯一改动 = `ButtonConfig` 尾部追加 `initChecked` 字段 + `Start()` checkbox 分支一行 `SetChecked`（这是对 spec“本体零改动”的一处经主人确认方向的微小扩展，用于删除 friend 槽位戳勾选的 hack）。迁移完成后删除 `UIManager::CreateMessageBox` 与两个 friend 声明。

**Tech Stack:** C++17 / CMake preset `clang-release` / AutoTest JSON 脚本验证。

**Spec:** `docs/superpowers/specs/2026-07-04-messagebox-builder-design.md`

## Global Constraints

- 行为与外观逐像素等价（纯调用方式改写）；坐标、字号、颜色、纹理 key、autoClose 值必须原样搬运。
- `clang-release` 构建零警告（msvc 不报这些警告，必须用 clang 验证）。
- 本仓库无单元测试框架：每个任务的"测试"= 构建 + AutoTest 脚本 + Read 截图肉眼比对。
- AutoTest 运行目录必须是 `build\clang-release\`；截图输出在 `build\clang-release\autotest\out\<script>\`（文件名无 .png 扩展也能 Read）。
- commit 属于 Claude 的职责，git push 不做。

---

### Task 1: 新增 Builder + ButtonConfig.initChecked

**Files:**
- Modify: `PlantVsZombies/UI/GameMessageBox.h`
- Modify: `PlantVsZombies/UI/GameMessageBox.cpp`

**Interfaces:**
- Produces: `GameMessageBox::Builder`（下列全部方法签名，后续任务逐字使用）；`ButtonConfig::initChecked`（bool，默认 false）。

- [ ] **Step 1: ButtonConfig 追加 initChecked 字段**

`GameMessageBox.h` 中 `ButtonConfig` 改为（仅尾部加一行，现有 ≤7 项花括号初始化不受影响）：

```cpp
	struct ButtonConfig {
		std::string text;
		Vector pos;
		Vector size;   // 大小，如果是Vector::zero就是按照NormalButton处理
		float fontsize;
		std::function<void()> callback;
		std::string texture;
		bool autoClose = true;             // 是否自动关闭
		bool initChecked = false;          // 仅 checkbox 有效：创建时的初始勾选态
	};
```

- [ ] **Step 2: Start() checkbox 分支应用 initChecked**

`GameMessageBox.cpp` `Start()` 的 checkbox 分支（`SetImageKeys(... IMAGE_OPTIONS_CHECKBOX1)` 之后）加一行：

```cpp
			button->SetAsCheckbox(true);
			button->SetImageKeys
			(config.texture, config.texture, config.texture,
				ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX1);
			button->SetChecked(config.initChecked);
```

- [ ] **Step 3: 头文件声明 Builder**

`GameMessageBox.h` 中 `GameMessageBox` 类体末尾（`private:` 之前的 public 区）加 `class Builder;` 前置声明；类体外、`#endif` 前加完整定义：

```cpp
// 流式构建器：把 9 参构造与隐式规则（空key+explicitSize=纯色面板、CHECKBOX纹理嗅探）
// 显式化为命名方法。终结方法 Show() 创建对象并返回 shared_ptr。
class GameMessageBox::Builder {
public:
	explicit Builder(const Vector& pos) : m_pos(pos) {}

	// —— 背景（不调用 = 默认 IMAGE_MESSAGEBOX 纹理）；后调覆盖先调 ——
	Builder& Panel(float w, float h) {                    // 纯色面板
		m_bgKey.clear(); m_explicitSize = Vector(w, h); return *this;
	}
	Builder& Background(const std::string& key) {         // 纹理，原始尺寸×scale
		m_bgKey = key; m_explicitSize = Vector(0.0f, 0.0f); return *this;
	}
	Builder& Background(const std::string& key, const Vector& size) {  // 纹理+显式尺寸居中
		m_bgKey = key; m_explicitSize = size; return *this;
	}

	Builder& Title(const std::string& t)   { m_title = t;   return *this; }
	Builder& Message(const std::string& m) { m_message = m; return *this; }

	Builder& Text(const Vector& pos, float fontSize, const std::string& text,
		const glm::vec4& color, const std::string& font = ResourceKeys::Fonts::FONT_FZCQ) {
		m_texts.push_back({ pos, fontSize, text, color, font });
		return *this;
	}

	Builder& Button(const std::string& text, const Vector& pos, const Vector& size,
		float fontSize, std::function<void()> cb,
		const std::string& texture = ResourceKeys::Textures::IMAGE_BUTTONSMALL,
		bool autoClose = true) {
		m_buttons.push_back({ text, pos, size, fontSize, std::move(cb), texture, autoClose, false });
		return *this;
	}

	Builder& Checkbox(const Vector& pos, const Vector& size,
		std::function<void()> cb, bool initChecked = false) {
		m_buttons.push_back({ "", pos, size, 1.0f, std::move(cb),
			ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0, false, initChecked });
		return *this;
	}

	Builder& Slider(const Vector& pos, const Vector& size, float minVal, float maxVal,
		float initValue, std::function<void(float)> cb, bool integerOnly = false) {
		m_sliders.push_back({ pos, size, minVal, maxVal, initValue, std::move(cb), integerOnly });
		return *this;
	}

	Builder& Scale(float s) { m_scale = s; return *this; }

	std::shared_ptr<GameMessageBox> Show();   // 实现在 .cpp（依赖 GameObjectManager）

private:
	Vector m_pos;
	std::string m_title;
	std::string m_message;
	std::string m_bgKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
	float m_scale = 1.0f;
	Vector m_explicitSize{ 0.0f, 0.0f };
	std::vector<GameMessageBox::ButtonConfig> m_buttons;
	std::vector<GameMessageBox::SliderConfig> m_sliders;
	std::vector<GameMessageBox::TextConfig>   m_texts;
};
```

⚠️ 默认背景是 `IMAGE_MESSAGEBOX` 而非空串——`GameMessageBox` 构造函数会用传入值**覆盖**成员默认值，传空串会变成"无背景"。

- [ ] **Step 4: .cpp 实现 Show()**

`GameMessageBox.cpp` 末尾追加：

```cpp
std::shared_ptr<GameMessageBox> GameMessageBox::Builder::Show()
{
	return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<GameMessageBox>(
		LAYER_UI, m_pos, m_message, m_buttons, m_sliders, m_texts,
		m_title, m_bgKey, m_scale, m_explicitSize);
}
```

- [ ] **Step 5: 构建验证**

Run（VS 环境已导入的会话中）: `cmake --build --preset clang-release`
Expected: 构建成功，0 warning。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/UI/GameMessageBox.h PlantVsZombies/UI/GameMessageBox.cpp
git commit -m "feat(ui): GameMessageBox 增加流式 Builder 与 ButtonConfig.initChecked"
```

---

### Task 2: 迁移 GameScene 暂停/重开/退出菜单

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp:283-410`（OpenMenu / OpenRestartMenu / OpenQuitMenu）
- Test: 新建 `autotest/scripts/pause_menu_shot.json`

**Interfaces:**
- Consumes: Task 1 的 `GameMessageBox::Builder` 全部方法。

- [ ] **Step 1: 重写 OpenMenu 的消息框构建（283-361 行函数体后半）**

守卫与 `mOpenMenu/SetPaused` 前缀不动；从 `std::vector<...> buttons;` 起到函数末尾整段替换为：

```cpp
	auto& gameApp = GameAPP::GetInstance();
	const glm::vec4 labelColor{ 107, 109, 144, 255 };
	mMenu = GameMessageBox::Builder(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f))
		.Background(ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK)
		.Button(u8"返回游戏", Vector(400, 430), Vector(360, 100), 40, [this]() {
			mOpenMenu = false;
			DeltaTime::SetPaused(false);
		}, ResourceKeys::Textures::IMAGE_OPTIONS_BACKTOGAMEBUTTON0)
		.Button(u8"重新开始", Vector(485, 330), Vector(213 * 0.9f, 50 * 0.9f), 21,
			[this]() { this->OpenRestartMenu(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Button(u8"主菜单", Vector(485, 371), Vector(213 * 0.9f, 50 * 0.9f), 21,
			[this]() { this->OpenQuitMenu(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Button(u8"查看图鉴", Vector(485, 289), Vector(213 * 0.9f, 50 * 0.9f), 21, [this]() {
			DeltaTime::SetPaused(false);
			this->mLendToAlmanacScene = true;
		}, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Checkbox(Vector(455, 250), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowPlantHP = !app.mShowPlantHP;
		}, gameApp.mShowPlantHP)
		.Checkbox(Vector(590, 250), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowZombieHP = !app.mShowZombieHP;
		}, gameApp.mShowZombieHP)
		.Slider(Vector(530, 175), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetMusicVolume(),
			[](float v) { AudioSystem::SetMusicVolume(v); })
		.Slider(Vector(530, 200), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetSoundVolume(),
			[](float v) { AudioSystem::SetSoundVolume(v); })
		.Slider(Vector(530, 225), Vector(135, 10), 1, 7,
			static_cast<float>(GameAPP::GetInstance().Difficulty),
			[](float v) { GameAPP::GetInstance().Difficulty = static_cast<int>(v); }, true)
		.Text(Vector(480, 165), 22, u8"音乐", labelColor)
		.Text(Vector(480, 190), 22, u8"音效", labelColor)
		.Text(Vector(498, 256), 14, u8"植物血量显示", labelColor)
		.Text(Vector(634, 256), 14, u8"僵尸血量显示", labelColor)
		.Text(Vector(480, 215), 22, u8"难度", labelColor)
		.Show();
```

原函数末尾的 `buttonChecks` 槽位戳勾选段（353-360 行）**整段删除**——初始态已由 `Checkbox(..., initChecked)` 承担。

注意：原代码 checkbox 的 `autoClose` 为 `false`，Builder 的 `Checkbox()` 内部固定 `autoClose=false`，语义一致。

- [ ] **Step 2: 重写 OpenRestartMenu（363-385 行）**

```cpp
void GameScene::OpenRestartMenu()
{
	if (this->mOpenRestartMenu) return;
	this->mOpenRestartMenu = true;

	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Message(u8"    确定重新开始游戏吗?")
		.Scale(1.5f)
		.Button(u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mOpenMenu = false;
			this->mOpenRestartMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToRestart = true;
			this->mOpenRestartMenu = false;
			this->mOpenMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Show();
}
```

（texture 省略即默认 `IMAGE_BUTTONSMALL`，与原值相同；背景省略即默认 `IMAGE_MESSAGEBOX`，与原 UIManager 默认相同。）

- [ ] **Step 3: 重写 OpenQuitMenu（387-410 行）** —— 同 Step 2 模式：

```cpp
void GameScene::OpenQuitMenu()
{
	if (this->mOpenQuitMenu) return;
	this->mOpenQuitMenu = true;

	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Message(u8"    确定退出这把游戏吗?")
		.Scale(1.5f)
		.Button(u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mOpenMenu = false;
			this->mOpenQuitMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToBackMenu = true;
			this->mOpenQuitMenu = false;
			this->mOpenMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Show();
}
```

- [ ] **Step 4: 构建**

Run: `cmake --build --preset clang-release` → 0 warning。

- [ ] **Step 5: 新建暂停菜单截图脚本并运行**

`autotest/scripts/pause_menu_shot.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_PEASHOOTER"] },
    { "op": "wait_state", "state": "GAME", "timeout": 30 },
    { "op": "wait_frames", "value": 40 },
    { "op": "key", "name": "escape" },
    { "op": "wait_frames", "value": 40 },
    { "op": "screenshot", "name": "pause_menu" },
    { "op": "quit" }
  ]
}
```

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\pause_menu_shot.json -Seed 42
$LASTEXITCODE   # Expected: 0
Pop-Location
```
Read 截图 `build\clang-release\autotest\out\pause_menu_shot\pause_menu`：暂停菜单背景/按钮/滑块/两复选框及其勾选态与迁移前一致（复选框默认状态下应为勾选，mShowPlantHP/mShowZombieHP 默认值为准）。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp autotest/scripts/pause_menu_shot.json
git commit -m "refactor(ui): GameScene 暂停/重开/退出菜单迁移到 MessageBox Builder"
```

---

### Task 3: 迁移词条选择 / 词条查看 / 开发者面板 / GameOver

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.cpp`（RenderPerkSelect ~806-836、RenderPerkViewPage ~955-994、GameOver ~1167-1182、RenderDevPanel ~1216-1279）

**Interfaces:**
- Consumes: Task 1 Builder。此任务全部是"vector push_back → 链式调用"的机械改写，**循环内动态加按钮/文字的站点保留 Builder 局部变量再链式调用**。

- [ ] **Step 1: RenderPerkSelect 尾段改写**

前面的布局计算全部不动；`std::vector<...> buttons;` 三行起替换。循环内动态追加 → 先建 builder 再在循环里调用：

```cpp
	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxW, boxH);   // 纯色面板：尺寸由内容自动决定，面板矩形与文字坐标严格对齐

	// 标题（顶部居中）
	builder.Text(Vector(cx - titleW / 2.0f, boxTop + padY), static_cast<float>(titleFont), title, titleColor);

	// 配对行：绿=植物增益、红=僵尸增难，右侧「选择」按钮
	const float rowsTop = boxTop + padY + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		const float blockTop = rowsTop + i * (rowBlockH + rowGap);
		builder.Text(Vector(boxLeft + padX, blockTop),         static_cast<float>(rowFont), rows[i].plant,  green);
		builder.Text(Vector(boxLeft + padX, blockTop + lineH), static_cast<float>(rowFont), rows[i].zombie, red);

		const float btnY = blockTop + (rowBlockH - selectBtnSize.y) / 2.0f;
		builder.Button(u8"选择", Vector(boxRight - padX - selectBtnSize.x, btnY), selectBtnSize, 16,
			[this, i]() { this->ApplyPerkSelection(i); });
	}

	// 跳过按钮（底部居中）
	builder.Button(u8"跳过本轮", Vector(cx - skipBtnSize.x / 2.0f, boxTop + boxH - padY - skipBtnSize.y),
		skipBtnSize, 20, [this]() { this->ApplyPerkSelection(-1); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG);

	mPerkSelectBox = builder.Show();
```

- [ ] **Step 2: RenderPerkViewPage 尾段改写**（同模式）

```cpp
	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxW, boxH);   // 纯色面板，同选词条框，规避墓碑花边内缩导致文字溢出

	const float blockTop = boxTop + padY + (availH - contentH) / 2.0f;
	const float titleW   = measureW(title, titleFont);
	builder.Text(Vector(cx - titleW / 2.0f, blockTop), static_cast<float>(titleFont), title, titleColor);

	float y = blockTop + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		const Line& ln = perkLines[pageStart + i];
		builder.Text(Vector(boxLeft + padX, y), static_cast<float>(rowFont), ln.text, ln.color);
		y += rowLineH + rowGap;
	}

	// 底部一行三按钮：上一页（左·仅非首页）· 关闭（中·恒显）· 下一页（右·仅有下一页）
	const float    boxRight   = cx + boxW / 2.0f;
	const Vector   navBtnSize(110.0f, 44.0f);
	const float    btnY       = boxTop + boxH - padY - closeBtnSize.y;

	builder.Button(u8"关闭", Vector(cx - closeBtnSize.x / 2.0f, btnY), closeBtnSize, 20,
		[this]() { this->ClosePerkView(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG);
	if (mPerkViewPage > 0)
		builder.Button(u8"上一页", Vector(boxLeft + padX, btnY), navBtnSize, 18,
			[this]() { --mPerkViewPage; RenderPerkViewPage(); });
	if (mPerkViewPage < totalPages - 1)
		builder.Button(u8"下一页", Vector(boxRight - padX - navBtnSize.x, btnY), navBtnSize, 18,
			[this]() { ++mPerkViewPage; RenderPerkViewPage(); });

	mPerkViewBox = builder.Show();
```

- [ ] **Step 3: GameOver 尾段改写**

```cpp
	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Title(u8"游戏结束")
		.Message(u8"僵尸吃掉了你的脑子！")
		.Scale(1.5f)
		.Button(u8"返回菜单", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToBackMenu = true;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"重新开始", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToRestart = true;
			DeltaTime::SetPaused(false);
		})
		.Show();
```

- [ ] **Step 4: RenderDevPanel 尾段改写**

布局常量不动；`std::vector<...>` 三行起替换（`toggleText` lambda 保留）：

```cpp
	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxSize.x, boxSize.y);
	builder.Text(Vector(cx - 70.0f, 110.0f), 22.0f, u8"开发者面板", titleColor);

	// 作弊开关（点击翻转后重建面板刷新文字）
	builder.Button(toggleText(u8"无冷却种植", GameAPP::mDevNoCooldown),
		Vector(340.0f, 160.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevNoCooldown = !GameAPP::mDevNoCooldown; RenderDevPanel(); });
	builder.Button(toggleText(u8"无视阳光", GameAPP::mDevFreePlant),
		Vector(340.0f, 206.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevFreePlant = !GameAPP::mDevFreePlant; RenderDevPanel(); });

	// 僵尸类型选择行
	const int zn = static_cast<int>(kDevZombieTable.size());
	builder.Button("<", Vector(340.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + zn - 1) % zn; RenderDevPanel(); });
	builder.Text(Vector(395.0f, 260.0f), 14.0f, kDevZombieTable[mDevZombieIndex].second, textColor);
	builder.Button(">", Vector(560.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + 1) % zn; RenderDevPanel(); });
	builder.Button(u8"召唤", Vector(620.0f, 252.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->BeginDevSpawnMode(); });

	// 关卡选择行
	builder.Button(u8"-", Vector(340.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { if (mDevLevelSel > 1) --mDevLevelSel; RenderDevPanel(); });
	builder.Text(Vector(420.0f, 310.0f), 16.0f,
		std::string(u8"关卡 ") + std::to_string(mDevLevelSel), textColor);
	builder.Button(u8"+", Vector(560.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { ++mDevLevelSel; RenderDevPanel(); });
	builder.Button(u8"进入", Vector(620.0f, 302.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->DevJumpToLevel(); });
	builder.Button(u8"无尽1000", Vector(340.0f, 348.0f), Vector(110.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1000; RenderDevPanel(); });
	builder.Button(u8"夜无尽1001", Vector(460.0f, 348.0f), Vector(130.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1001; RenderDevPanel(); });

	// 底部：下一波 / 关闭
	builder.Button(u8"下一波", Vector(360.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { this->DevTriggerNextWave(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, false);   // 不自动关面板，可连点
	builder.Button(u8"关闭", Vector(600.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { mDevPanelActive = false; DeltaTime::SetPaused(false); mDevPanelBox.reset(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG);

	mDevPanelBox = builder.Show();
```

- [ ] **Step 5: 构建 + 三个现有 smoke 回归**

```powershell
cmake --build --preset clang-release
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_select.json -Seed 42; $LASTEXITCODE
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_view.json   -Seed 42; $LASTEXITCODE
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_develop.json     -Seed 42; $LASTEXITCODE
Pop-Location
```
Expected: 三个脚本 `$LASTEXITCODE` 均为 0；Read 各自 out 目录截图，面板外观与迁移前一致。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/Game/GameScene.cpp
git commit -m "refactor(ui): 词条选择/查看、开发者面板、GameOver 迁移到 MessageBox Builder"
```

---

### Task 4: 迁移 MainMenuScene + 删除 UIManager::CreateMessageBox 与 friend

**Files:**
- Modify: `PlantVsZombies/Game/MainMenuScene.cpp:128-217`（OpenMenu）
- Modify: `PlantVsZombies/UI/UIManager.h:57-69`（删 CreateMessageBox）
- Modify: `PlantVsZombies/UI/GameMessageBox.h:17-18`（删两个 friend）

**Interfaces:**
- Consumes: Task 1 Builder。完成后仓库内不再有任何 `CreateMessageBox` 调用与 `m_buttons` 外部访问。

- [ ] **Step 1: 重写 MainMenuScene::OpenMenu 的消息框构建**

守卫、SetPaused、三个按钮 SetEnabled(false) 前缀不动；`std::vector` 三行起到函数末尾（含 `buttonChecks` 槽位戳段与其注释）整段替换为：

```cpp
	auto& gameApp = GameAPP::GetInstance();
	const glm::vec4 labelColor{ 107, 109, 144, 255 };
	mMenu = GameMessageBox::Builder(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f))
		.Background(ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK)
		.Button(u8"返回游戏", Vector(400, 430), Vector(360, 100), 40, [this]() {
			if (mAlmanacButton) mAlmanacButton->SetEnabled(true);
			if (mOpitionButton) mOpitionButton->SetEnabled(true);
			if (mGameButton) mGameButton->SetEnabled(true);
			mOpenMenu = false;
			DeltaTime::SetPaused(false);
		}, ResourceKeys::Textures::IMAGE_OPTIONS_BACKTOGAMEBUTTON0)
		.Checkbox(Vector(510, 250), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.ApplyVsync(!app.mVsync);
		}, gameApp.mVsync)
		.Checkbox(Vector(510, 290), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.SetFullscreen(!app.IsFullscreen());
		}, gameApp.IsFullscreen())
		.Checkbox(Vector(510, 330), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowPlantHP = !app.mShowPlantHP;
		}, gameApp.mShowPlantHP)
		.Checkbox(Vector(510, 370), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowZombieHP = !app.mShowZombieHP;
		}, gameApp.mShowZombieHP)
		.Slider(Vector(530, 175), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetMusicVolume(),
			[](float v) { AudioSystem::SetMusicVolume(v); })
		.Slider(Vector(530, 200), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetSoundVolume(),
			[](float v) { AudioSystem::SetSoundVolume(v); })
		.Slider(Vector(530, 225), Vector(135, 10), 1, 7,
			static_cast<float>(GameAPP::GetInstance().Difficulty),
			[](float v) { GameAPP::GetInstance().Difficulty = static_cast<int>(v); }, true)
		.Text(Vector(480, 165), 22, u8"音乐", labelColor)
		.Text(Vector(480, 190), 22, u8"音效", labelColor)
		.Text(Vector(480, 215), 22, u8"难度", labelColor)
		.Text(Vector(555, 254), 18, u8"垂直同步", labelColor)
		.Text(Vector(555, 294), 18, u8"全屏", labelColor)
		.Text(Vector(555, 334), 18, u8"植物血量显示", labelColor)
		.Text(Vector(555, 374), 18, u8"僵尸血量显示", labelColor)
		.Show();
```

注意保留原修复语义：四个复选框初始态来自**各自不同**的状态变量（mVsync / IsFullscreen / mShowPlantHP / mShowZombieHP），Builder 写法天然按项绑定，原"槽位错位"bug 类别被结构性消除。

- [ ] **Step 2: 删除 `UIManager::CreateMessageBox`（UIManager.h 57-69 行整个方法）**，并删除 UIManager.h 中因此不再需要的 `GameMessageBox` 相关 include/前置声明（若仍被其他成员使用则保留）。

- [ ] **Step 3: 删除 `GameMessageBox.h` 中 `friend class MainMenuScene;` 与 `friend class GameScene;` 两行。**

- [ ] **Step 4: 全仓验证无残留调用**

Run: Grep `CreateMessageBox` 与 `->m_buttons` on `PlantVsZombies/`
Expected: 仅 GameMessageBox 自身成员使用，无外部访问。

- [ ] **Step 5: 构建 + 回归**

```powershell
cmake --build --preset clang-release        # 0 warning
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\pause_menu_shot.json -Seed 42; $LASTEXITCODE   # 0
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_gameplay.json  -Seed 42; $LASTEXITCODE   # 0
Pop-Location
```
主菜单选项面板无现成脚本：用 `click`/`key` 合成输入不易定位选项按钮坐标时，请主人开一次游戏肉眼确认四个复选框初始勾选态正确（尤其全屏/垂直同步来自真实状态）。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/Game/MainMenuScene.cpp PlantVsZombies/UI/UIManager.h PlantVsZombies/UI/GameMessageBox.h
git commit -m "refactor(ui): MainMenuScene 迁移 Builder；删除 UIManager::CreateMessageBox 与 friend 声明"
```
