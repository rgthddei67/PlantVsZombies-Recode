# AutoTest 合成输入（点击 / 键盘）设计

日期：2026-06-13
关联：`docs/superpowers/specs/2026-06-12-autotest-suite-design.md`（AutoTest 套件原始设计）

## 背景与动机

现有 AutoTest 命令（`plant` / `spawn_zombie` / `choose_cards` / `set_sun` …）全部**绕过输入层、直接调用游戏逻辑**（`Board::CreatePlant`、`ChooseCardUI::ToggleCardSelection` 等），因此只能驱动 `GameScene`。正在迭代的 `PlantAlmanacScene` / `ZombieAlmanacScene` 等非 GameScene 场景，其交互全走 `ClickableComponent` + `InputHandler`，现有命令**完全覆盖不到**。

本设计新增两条命令 `click` / `key`，通过**注入合成 SDL 事件**模拟在任意逻辑坐标的鼠标点击与键盘按键，使 AutoTest 能驱动任意场景的真实输入链路。

## 目标 / 非目标

**目标：**
- 在指定逻辑坐标 `(x, y)` 模拟一次鼠标点击（左/右/中键），走与真实点击**完全相同**的输入路径。
- 模拟一次键盘按键（按下沿 / 松开沿 / 完整按一下），按键用可读名字符串指定。
- 零侵入：对非 AutoTest 的真实游戏运行**无任何性能或行为影响**。

**非目标（YAGNI）：**
- 不做拖拽 / 按住-移动-松开的低级 `mouse_move`/`mouse_down`/`mouse_up` 拆分命令。`PlantAlmanacScene` 仅用 `ClickableComponent`（纯点击，无拖拽/滚动条）。若将来出现拖拽交互再补。
- 不做文本输入（IME / `SDL_TEXTINPUT`）。
- 不做鼠标滚轮。

## 关键约束：时序正确性

主循环每帧顺序（`GameAPP::Run`，GameApp.cpp:328-371）：

```
SDL_PollEvent → InputHandler::ProcessEvent   // 设置 PRESSED / RELEASED
sceneManager.Update()                          // 场景在此读输入（IsMouseButtonPressed 等）
TestDriver::Update()                           // ← 我们的命令在此推进
InputHandler::Update()                         // PRESSED→DOWN, RELEASED→UP 翻页
```

`TestDriver::Update()` 跑在场景 Update **之后**、翻页**之前**。这排除了"直接改 InputHandler 状态"的方案（按下沿会被当帧翻页吃掉，场景永远读不到 `IsMouseButtonPressed`）。

**选定方案：`SDL_PushEvent` 注入合成事件。** 事件进入 SDL 内部队列，在**下一帧**的 poll 循环由 `ProcessEvent` 正常消费——落点正好在场景 Update 之前，与真实用户输入路径完全一致（同样经 `Graphics::ScreenToLogical` letterbox 逆变换、同样的 PRESSED→DOWN 翻页时序）。

### 点击的跨帧状态机

一次真实点击 = 按下沿 + 松开沿，必须跨帧，否则同帧 DOWN+UP 会在下一帧 poll 里被刷成 RELEASED，场景读不到按下沿：

| 阶段 | TestDriver 本帧推送 | 下一帧场景读到 |
|---|---|---|
| 0 | `SDL_MOUSEMOTION(x,y)` + `SDL_MOUSEBUTTONDOWN` | 鼠标到位 + `IsMouseButtonPressed==true`（按下沿）|
| 1 | （等 `hold_frames` 帧，默认 1）| `IsMouseButtonDown==true`（按住）|
| 2 | `SDL_MOUSEBUTTONUP` | `IsMouseButtonReleased==true`（松开沿）|

`click` 命令实现为逐命令小状态机，复用 TestDriver 现有的逐命令状态字段惯例（参考 `mFramesLeft` for `wait_frames`），推进到下一条命令时清零。

## 命令规格

### `click`

```jsonc
{ "op": "click", "x": 400, "y": 300 }                                  // 左键单击
{ "op": "click", "x": 400, "y": 300, "button": "right", "hold_frames": 3 }
```

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `x`, `y` | number | 必填 | **逻辑坐标**（与游戏内布局/UI 判定、`dump_state` 的 x/y 同一坐标系）|
| `button` | string | `"left"` | `left` / `right` / `middle` |
| `hold_frames` | int | `1` | 按下到松开之间保持的帧数（≥1）|

坐标语义说明：合成的 `SDL_MOUSEMOTION` 的 `motion.x/y` 会经 `ScreenToLogical` 逆变换。AutoTest 窗口通常 scale=1/offset=0，此变换为恒等，故脚本里写的就是逻辑坐标值。

### `key`

```jsonc
{ "op": "key", "name": "space" }                      // 完整按一下（press = down 然后 up）
{ "op": "key", "name": "escape", "action": "down" }   // 仅按下沿
{ "op": "key", "name": "enter",  "action": "up" }     // 仅松开沿
```

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `name` | string | 必填 | 按键名，见名表 |
| `action` | string | `"press"` | `press`（down+up，跨帧同 click）/ `down`（仅按下沿）/ `up`（仅松开沿）|

`press` 同样需跨帧（down 帧 → 下一帧 up），理由同点击。`down`/`up` 单帧推一个事件即完成。

### 错误处理

- `x`/`y`/`name` 缺失 → `Fail(...)`（与现有命令一致，退出码 1）。
- `button` / `action` / `name` 取值非法（不在表中）→ `Fail("未知 ...: <值>")`。
- 不校验坐标是否命中任何控件——点击空白处是合法行为（测试可借此验证"点空白不触发")。

## 名表

放在 `TestDriver.cpp` 顶部匿名命名空间，与 `kPlantNames`/`kZombieNames` 并排，照抄"用到再加一行"的惯例。

```cpp
// 鼠标按钮名 → SDL 按钮宏
{ "left", SDL_BUTTON_LEFT }, { "right", SDL_BUTTON_RIGHT }, { "middle", SDL_BUTTON_MIDDLE }

// 按键名 → SDL_Keycode（初始覆盖常用集，按需扩展）
"a".."z"            → SDLK_a .. SDLK_z
"0".."9"            → SDLK_0 .. SDLK_9
"space"             → SDLK_SPACE
"enter" / "return"  → SDLK_RETURN
"escape" / "esc"    → SDLK_ESCAPE
"tab"               → SDLK_TAB
"backspace"         → SDLK_BACKSPACE
"up"/"down"/"left"/"right" → SDLK_UP / DOWN / LEFT / RIGHT
"f1".."f12"         → SDLK_F1 .. SDLK_F12
```

合成 `SDL_KEYDOWN`/`SDL_KEYUP` 时填 `event.key.keysym.sym = keycode`（`ProcessEvent` 只读 `.sym`，无需填 scancode）。

## 实现落点

单文件改动：`Game/AutoTest/TestDriver.cpp`（外加 `TestDriver.h` 若需新增逐命令状态字段）。

1. `TestDriver.cpp` 顶部加 `kMouseButtonNames`、`kKeyNames` 两张表 + `#include <SDL2/SDL.h>`。
2. `ExecuteCurrent()` 增加 `op == "click"` 与 `op == "key"` 两个分支：
   - 构造 `SDL_Event`，填字段，`SDL_PushEvent(&ev)`。
   - `click` / `key(press)` 用逐命令阶段状态（新增 `mClickPhase` 之类字段，推进下条时在 `Update()` 的清零块里复位）跨帧完成；返回 `false` 表示"等待中，下帧重试"，全部阶段走完返回 `true`。
3. 在 `TestDriver::Update()` 推进到下一条命令的清零块（TestDriver.cpp:154-156）补上新状态字段复位。
4. `TestDriver.h` 命令集注释 / `CLAUDE.md` AutoTest 节命令集列表补 `click` / `key`。

## 性能影响

无。`TestDriver::Update()` 首行 `if (!mActive) return;`——非 AutoTest 模式下新代码一行不执行，是编译进去但运行期永不触发的死路径。AutoTest 模式下每个点击命令仅多 2~3 次 `SDL_PushEvent`（内部队列入队，开销可忽略），与现有 `plant`/`spawn_zombie` 同级。主人既有的性能优化数字不受影响。

## 验收

新增冒烟脚本 `autotest/scripts/smoke_synthetic_input.json`：进图鉴场景 → `click` 某卡片坐标 → `dump_state` / `screenshot` 验证交互生效。Read 截图人眼确认。同时保留一条"点击空白处不触发"的负向用例。
