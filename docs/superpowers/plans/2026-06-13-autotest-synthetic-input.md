# AutoTest 合成输入（click / key）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 AutoTest 脚本新增 `click` / `key` 两条命令，通过 `SDL_PushEvent` 注入合成事件，在指定逻辑坐标模拟鼠标点击与键盘按键，驱动任意场景（图鉴等）的真实输入链路。

**Architecture:** TestDriver 在 `ExecuteCurrent()` 里构造 `SDL_Event` 并 `SDL_PushEvent` 推进 SDL 队列；事件在下一帧 poll 由 `InputHandler::ProcessEvent` 正常消费，路径与真实用户输入完全一致。点击/按键的"按下沿+松开沿"必须跨帧，用一个复用的逐命令状态字段 `mInputPhase` 实现小状态机。非 AutoTest 模式 `TestDriver::Update()` 首行即返回，零运行期影响。

**Tech Stack:** C++17、SDL2（`SDL_PushEvent`/`SDL_Event`）、nlohmann/json、CMake（clang-release / msvc-release preset）、AutoTest JSON 脚本作为集成测试。

---

## 背景速览（实现者必读）

每帧主循环顺序（`GameAPP::Run`，`PlantVsZombies/GameApp.cpp:328-371`）：

```
SDL_PollEvent → InputHandler::ProcessEvent   // 设 PRESSED/RELEASED
sceneManager.Update()                          // 场景读输入
TestDriver::Update()                           // ← 命令在此推进，调 SDL_PushEvent
InputHandler::Update()                         // PRESSED→DOWN, RELEASED→UP 翻页
```

`SDL_PushEvent` 推的事件进 SDL 内部队列，**下一帧** poll 才被消费——正好落在场景 Update 之前。因此跨帧时序是：

- TestDriver 帧 N 推 `MOUSEBUTTONDOWN` → 帧 N+1 poll 置 PRESSED → 帧 N+1 场景读到 `IsMouseButtonPressed`（按下沿）→ 帧 N+1 末翻成 DOWN。
- 同帧推 DOWN+UP 会在下一帧 poll 被刷成 RELEASED，场景**读不到按下沿**。故必须 down 帧、up 隔帧分开。

现有逐命令状态字段（`TestDriver.h:45-48`）：`mWaitAccum` / `mFramesLeft` / `mTimeoutAccum` / `mBreakFrame`，在 `Update()` 推进到下一条命令时统一清零（`TestDriver.cpp:154-156`）。`mFramesLeft`（`wait_frames`，`TestDriver.cpp:178-183`）是逐命令状态机的范本。

关键文件：
- `PlantVsZombies/Game/AutoTest/TestDriver.cpp` — 命令分发（顶部匿名 ns 有 `kPlantNames`/`kZombieNames` 名表；`ExecuteCurrent()` 是 if-链；末尾 `Fail("未知命令...")`）。
- `PlantVsZombies/Game/AutoTest/TestDriver.h` — 逐命令状态字段声明 + 命令集注释。
- `PlantVsZombies/UI/InputHandler.cpp:18-76` — `ProcessEvent` 只读 `motion.x/y`、`button.button`、`key.keysym.sym`。

格子中心像素（`PlantVsZombies/Game/Cell.h:12-15,60-61`）：`x = 242 + col*80 + 40`，`y = 88 + row*100 + 50`。例：row2/col2 ≈ (442, 338)。

构建与运行（详见 CLAUDE.md）：先导入 VS 开发者环境，再
```powershell
cmake --build --preset clang-release      # 警告归零验证用 clang-release
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本>.json -Seed 42
$LASTEXITCODE   # 0=成功 1=失败 100=解析失败
Pop-Location
```
产物在 `build\clang-release\autotest\out\<脚本名>\`（`run.log` 是权威记录，Release 下 Logger INFO 被裁，run.log 仍写）。

---

## File Structure

- **Modify** `PlantVsZombies/Game/AutoTest/TestDriver.h` — 新增 `int mInputPhase = -1;` 状态字段；命令集注释补 `click`/`key`。
- **Modify** `PlantVsZombies/Game/AutoTest/TestDriver.cpp` — 加 `#include <SDL2/SDL.h>`；顶部加 `kMouseButtonNames`/`kKeyNames` 两张表；`ExecuteCurrent()` 加 `click`/`key` 分支；`Update()` 清零块补 `mInputPhase = -1;`。
- **Create** `autotest/scripts/smoke_synthetic_input.json` — 坐标无关连通性冒烟（机检回归）。
- **Create** `autotest/scripts/almanac_click.json` — 图鉴点击 E2E（截图肉眼确认）。
- **Modify** `CLAUDE.md` — AutoTest 节命令集列表补 `click`/`key`。

---

## Task 1: 名表 + 状态字段（编译通过，无行为）

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（顶部 include + 匿名 ns 名表）
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.h`（新增 `mInputPhase` 字段）

- [ ] **Step 1: 加 SDL include**

在 `TestDriver.cpp` 现有 include 块末尾（`#include <unordered_map>` 之后，第 18 行附近）加：

```cpp
#include <SDL2/SDL.h>
```

- [ ] **Step 2: 加鼠标按钮名表与按键名表**

在 `TestDriver.cpp` 顶部匿名命名空间内，紧接 `kBoardStateNames` 定义之后（`TestDriver.cpp:56` 之后）插入：

```cpp
	const std::unordered_map<std::string, Uint8> kMouseButtonNames = {
		{ "left", SDL_BUTTON_LEFT }, { "right", SDL_BUTTON_RIGHT },
		{ "middle", SDL_BUTTON_MIDDLE },
	};

	// 按键名 → SDL_Keycode。初始覆盖常用集，按需加行（与植物/僵尸名表同惯例）。
	const std::unordered_map<std::string, SDL_Keycode> kKeyNames = {
		{ "a", SDLK_a }, { "b", SDLK_b }, { "c", SDLK_c }, { "d", SDLK_d },
		{ "e", SDLK_e }, { "f", SDLK_f }, { "g", SDLK_g }, { "h", SDLK_h },
		{ "i", SDLK_i }, { "j", SDLK_j }, { "k", SDLK_k }, { "l", SDLK_l },
		{ "m", SDLK_m }, { "n", SDLK_n }, { "o", SDLK_o }, { "p", SDLK_p },
		{ "q", SDLK_q }, { "r", SDLK_r }, { "s", SDLK_s }, { "t", SDLK_t },
		{ "u", SDLK_u }, { "v", SDLK_v }, { "w", SDLK_w }, { "x", SDLK_x },
		{ "y", SDLK_y }, { "z", SDLK_z },
		{ "0", SDLK_0 }, { "1", SDLK_1 }, { "2", SDLK_2 }, { "3", SDLK_3 },
		{ "4", SDLK_4 }, { "5", SDLK_5 }, { "6", SDLK_6 }, { "7", SDLK_7 },
		{ "8", SDLK_8 }, { "9", SDLK_9 },
		{ "space", SDLK_SPACE }, { "enter", SDLK_RETURN }, { "return", SDLK_RETURN },
		{ "escape", SDLK_ESCAPE }, { "esc", SDLK_ESCAPE }, { "tab", SDLK_TAB },
		{ "backspace", SDLK_BACKSPACE },
		{ "up", SDLK_UP }, { "down", SDLK_DOWN }, { "left", SDLK_LEFT }, { "right", SDLK_RIGHT },
		{ "f1", SDLK_F1 }, { "f2", SDLK_F2 }, { "f3", SDLK_F3 }, { "f4", SDLK_F4 },
		{ "f5", SDLK_F5 }, { "f6", SDLK_F6 }, { "f7", SDLK_F7 }, { "f8", SDLK_F8 },
		{ "f9", SDLK_F9 }, { "f10", SDLK_F10 }, { "f11", SDLK_F11 }, { "f12", SDLK_F12 },
	};
```

- [ ] **Step 3: 加 mInputPhase 状态字段**

在 `TestDriver.h` 等待型状态字段块（第 48 行 `mBreakFrame` 之后）加：

```cpp
	int   mInputPhase = -1;      // click/key(press) 跨帧状态机阶段（-1 = 未初始化）
```

- [ ] **Step 4: 清零块补 mInputPhase**

在 `TestDriver.cpp` 的 `Update()` 推进下一条命令清零块（`TestDriver.cpp:154-156`，`mTimeoutAccum = 0.0f;` 之后）加：

```cpp
			mInputPhase = -1;
```

- [ ] **Step 5: 构建验证编译通过**

Run（先导入 VS 环境，见背景速览）：`cmake --build --preset clang-release`
Expected: 构建成功，0 error 0 warning（本任务只加未使用的常量表与字段，clang 不应报 unused —— 命名空间常量与成员变量均不触发 `-Wunused`）。

- [ ] **Step 6: Commit**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp PlantVsZombies/Game/AutoTest/TestDriver.h
git commit -m "feat(autotest): click/key 名表与跨帧状态字段（无行为）"
```

---

## Task 2: 实现 `key` 命令

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（`ExecuteCurrent()` 加 `key` 分支）
- Test: `autotest/scripts/smoke_synthetic_input.json`（Task 4 建；本任务先手验单条）

- [ ] **Step 1: 加 `key` 分支**

在 `ExecuteCurrent()` 末尾 `Fail("未知命令 op=...")`（`TestDriver.cpp:318`）**之前**插入：

```cpp
	if (op == "key") {
		const std::string name = cmd.value("name", "");
		auto it = kKeyNames.find(name);
		if (it == kKeyNames.end()) { Fail("未知按键名: " + name); return false; }
		const SDL_Keycode key = it->second;
		const std::string action = cmd.value("action", "press");

		auto pushKey = [&](Uint32 type) {
			SDL_Event ev{};
			ev.type = type;
			ev.key.keysym.sym = key;
			SDL_PushEvent(&ev);
		};

		if (action == "down") { pushKey(SDL_KEYDOWN); return true; }
		if (action == "up")   { pushKey(SDL_KEYUP);   return true; }
		if (action == "press") {
			// 跨帧：down 帧 → 下一帧 up，使场景读得到按下沿（理由见文件头时序说明）
			if (mInputPhase < 0) { pushKey(SDL_KEYDOWN); mInputPhase = 1; return false; }
			if (mInputPhase > 0) { --mInputPhase; return false; }
			pushKey(SDL_KEYUP);
			return true;
		}
		Fail("未知 key action: " + action);
		return false;
	}
```

- [ ] **Step 2: 构建**

Run: `cmake --build --preset clang-release`
Expected: 成功，0 error 0 warning。

- [ ] **Step 3: 手验单条 key 不崩、退出码 0**

临时建 `autotest/scripts/_tmp_key.json`：

```json
{
  "commands": [
    { "op": "wait_frames", "value": 5 },
    { "op": "key", "name": "escape" },
    { "op": "key", "name": "space", "action": "down" },
    { "op": "key", "name": "space", "action": "up" },
    { "op": "wait_frames", "value": 3 },
    { "op": "quit" }
  ]
}
```

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\_tmp_key.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: `$LASTEXITCODE` = 0；`build\clang-release\autotest\out\_tmp_key\run.log` 含 `done cmd#... (key)` 各行、无 `FAIL`。

- [ ] **Step 4: 删临时脚本**

```bash
rm autotest/scripts/_tmp_key.json
```
（注：`build\...\autotest\out\_tmp_key\` 是产物目录，不入 git，无需清理。）

- [ ] **Step 5: Commit**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m "feat(autotest): key 命令（press/down/up 合成键盘事件）"
```

---

## Task 3: 实现 `click` 命令

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（`ExecuteCurrent()` 加 `click` 分支）

- [ ] **Step 1: 加 `click` 分支**

在 `ExecuteCurrent()` 的 `key` 分支之后、`Fail("未知命令...")` 之前插入：

```cpp
	if (op == "click") {
		if (!cmd.contains("x") || !cmd.contains("y")) { Fail("click 缺 x 或 y 字段"); return false; }
		const float x = cmd.value("x", 0.0f);
		const float y = cmd.value("y", 0.0f);
		const std::string btnName = cmd.value("button", "left");
		auto bit = kMouseButtonNames.find(btnName);
		if (bit == kMouseButtonNames.end()) { Fail("未知鼠标按钮: " + btnName); return false; }
		const Uint8 button = bit->second;
		const int holdFrames = std::max(1, cmd.value("hold_frames", 1));

		// 跨帧状态机：mInputPhase<0 推 移动+按下并置剩余保持帧；>0 递减保持；==0 推松开完成。
		if (mInputPhase < 0) {
			SDL_Event mv{};
			mv.type = SDL_MOUSEMOTION;
			mv.motion.x = static_cast<Sint32>(x);   // AutoTest 窗口 scale=1，逻辑坐标即屏幕像素
			mv.motion.y = static_cast<Sint32>(y);
			SDL_PushEvent(&mv);

			SDL_Event down{};
			down.type = SDL_MOUSEBUTTONDOWN;
			down.button.button = button;
			down.button.x = static_cast<Sint32>(x);
			down.button.y = static_cast<Sint32>(y);
			SDL_PushEvent(&down);

			mInputPhase = holdFrames;
			return false;
		}
		if (mInputPhase > 0) { --mInputPhase; return false; }

		SDL_Event up{};
		up.type = SDL_MOUSEBUTTONUP;
		up.button.button = button;
		up.button.x = static_cast<Sint32>(x);
		up.button.y = static_cast<Sint32>(y);
		SDL_PushEvent(&up);
		return true;
	}
```

> 注：`ExecuteCurrent` 顶部已 `#include <algorithm>`（`TestDriver.cpp:17`），`std::max` 可用。

- [ ] **Step 2: 构建**

Run: `cmake --build --preset clang-release`
Expected: 成功，0 error 0 warning。

- [ ] **Step 3: 手验单条 click 不崩、退出码 0**

临时建 `autotest/scripts/_tmp_click.json`：

```json
{
  "commands": [
    { "op": "wait_frames", "value": 5 },
    { "op": "click", "x": 400, "y": 300 },
    { "op": "click", "x": 200, "y": 200, "button": "right", "hold_frames": 3 },
    { "op": "wait_frames", "value": 3 },
    { "op": "quit" }
  ]
}
```

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\_tmp_click.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: `$LASTEXITCODE` = 0；`out\_tmp_click\run.log` 含两行 `done cmd#... (click)`、无 `FAIL`。

- [ ] **Step 4: 删临时脚本**

```bash
rm autotest/scripts/_tmp_click.json
```

- [ ] **Step 5: Commit**

```bash
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m "feat(autotest): click 命令（跨帧按下/保持/松开合成鼠标事件）"
```

---

## Task 4: 连通性冒烟脚本（机检回归）

**Files:**
- Create: `autotest/scripts/smoke_synthetic_input.json`

- [ ] **Step 1: 写冒烟脚本（坐标无关，验证 parse+名表+事件推送+状态机全程不崩）**

```json
{
  "commands": [
    { "op": "wait_frames", "value": 10 },
    { "op": "click", "x": 400, "y": 300 },
    { "op": "click", "x": 250, "y": 150, "button": "right" },
    { "op": "click", "x": 300, "y": 400, "button": "middle", "hold_frames": 4 },
    { "op": "key", "name": "escape" },
    { "op": "key", "name": "enter", "action": "down" },
    { "op": "key", "name": "enter", "action": "up" },
    { "op": "wait_frames", "value": 5 },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 运行冒烟，验证退出码 0 且无 FAIL**

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_synthetic_input.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: `$LASTEXITCODE` = 0；`out\smoke_synthetic_input\run.log` 末行 `script finished OK`，含每条 `click`/`key` 的 `done cmd#...`，无 `FAIL`/`timeout`。

- [ ] **Step 3: 用 Read 工具看 run.log 确认**

Read `build/clang-release/autotest/out/smoke_synthetic_input/run.log`，逐行确认 6 条输入命令都有 `done`，无异常。

- [ ] **Step 4: Commit**

```bash
git add autotest/scripts/smoke_synthetic_input.json
git commit -m "test(autotest): 合成输入连通性冒烟脚本"
```

---

## Task 5: 图鉴点击 E2E（截图肉眼确认，dogfood 真实 UI 链路）

**Files:**
- Create: `autotest/scripts/almanac_click.json`

> 本任务证明合成点击**真的命中** `ClickableComponent` 并触发场景跳转——只有 `IsMouseButtonPressed` 按下沿真正生效时，MainMenu→图鉴 的导航才会发生。起始场景是 `MainMenuScene`（`GameApp.cpp:315`）。主菜单上图鉴入口按钮、以及图鉴里植物卡的精确像素坐标不是常量，**执行时通过截图 Read 现场读取**。

- [ ] **Step 1: 先截主菜单，拿到图鉴按钮坐标**

临时脚本 `autotest/scripts/_probe_menu.json`：

```json
{
  "commands": [
    { "op": "wait_frames", "value": 30 },
    { "op": "screenshot", "name": "menu.png" },
    { "op": "quit" }
  ]
}
```

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\_probe_menu.json -Seed 42
Pop-Location
```
然后用 Read 工具看 `build/clang-release/autotest/out/_probe_menu/menu.png`，**目测图鉴（Almanac / 图鉴）入口按钮的像素中心坐标**，记为 `(bx, by)`。

- [ ] **Step 2: 写 E2E 脚本（用上一步坐标）**

把 `(bx, by)` 填入 `autotest/scripts/almanac_click.json`（下例占位 480/520 必须替换为实测值）：

```json
{
  "commands": [
    { "op": "wait_frames", "value": 30 },
    { "op": "screenshot", "name": "01_menu.png" },
    { "op": "click", "x": 480, "y": 520 },
    { "op": "wait_frames", "value": 30 },
    { "op": "screenshot", "name": "02_after_click.png" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 3: 运行 E2E**

Run:
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\almanac_click.json -Seed 42
$LASTEXITCODE
Pop-Location
```
Expected: `$LASTEXITCODE` = 0。

- [ ] **Step 4: Read 两张截图，确认点击生效**

Read `out/almanac_click/01_menu.png` 与 `02_after_click.png`。
Expected: `01` 是主菜单；`02` 画面**已切换**（进入图鉴/下一界面），证明合成点击命中了按钮并触发了 `ClickableComponent`。若 `02` 仍停在主菜单，说明坐标没点中 → 回 Step 1 重新取坐标。

- [ ] **Step 5: 删探针脚本，提交 E2E**

```bash
rm autotest/scripts/_probe_menu.json
git add autotest/scripts/almanac_click.json
git commit -m "test(autotest): 图鉴点击 E2E（合成点击触发场景跳转）"
```

---

## Task 6: 文档（CLAUDE.md 命令集 + TestDriver.h 注释）

**Files:**
- Modify: `CLAUDE.md`（AutoTest 节命令集列表）
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.h`（命令集注释，若有）

- [ ] **Step 1: CLAUDE.md 命令集补两条**

在 `CLAUDE.md` 「AutoTest 自动化测试套件 → 命令集」一行，把 `set_timescale` / `screenshot` 之间或末尾的命令清单补上 `click` / `key`，并加一句说明：

```
... `set_timescale` / `click`（在逻辑坐标 x,y 模拟鼠标点击，button=left|right|middle，hold_frames 默认1）/ `key`（按键名 name，action=press|down|up）/ `screenshot` / ...
```

- [ ] **Step 2: 提交文档**

```bash
git add CLAUDE.md PlantVsZombies/Game/AutoTest/TestDriver.h
git commit -m "docs(autotest): 命令集补充 click/key 用法"
```

---

## Self-Review 记录

- **Spec 覆盖：** `click`（Task 3）/`key`（Task 2）/ 逻辑坐标（Task 3 motion.x/y 直传 + 注释）/ 按键名表（Task 1）/ 跨帧时序（Task 2、3 状态机）/ 零侵入（`mActive` 首行返回，未改）/ 验收双脚本（Task 4 机检 + Task 5 截图）—— 全部有对应任务。非目标（拖拽/文本/滚轮）未实现，符合 YAGNI。
- **占位符扫描：** Task 5 的 `(bx,by)` 480/520 是显式标注"必须替换为实测值"的现场探测步骤，方法具体（截图+Read），非 TODO 占位。其余无。
- **类型一致：** `mInputPhase`（Task 1 声明 / Task 2、3 使用 / Task 1 Step4 清零）三处一致；名表 `kMouseButtonNames`/`kKeyNames`（Task 1 定义 / Task 2、3 引用）一致；`SDL_PushEvent` 字段与 `InputHandler::ProcessEvent` 读取字段（`motion.x/y`、`button.button`、`key.keysym.sym`）一致。
