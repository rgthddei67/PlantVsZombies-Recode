# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

This is a CMake + vcpkg(manifest) C++ project (x64 Windows only). 构建系统已于 2026-06-13 从 .sln/.vcxproj 统一迁移到 CMake（`CMakeLists.txt` + `CMakePresets.json` + `vcpkg.json`，triplet `x64-windows-static`）。

- **Build (Claude 可自主执行):** 需在 VS 开发者环境下执行。**关键顺序：先把 vswhere 所在的 Installer 目录加进 PATH，再导入 `VsDevCmd.bat`**——否则 VsDevCmd 内部调 vswhere 会吐 `'vswhere.exe' is not recognized`（构建仍能成功，但有噪音）。无噪音的一次性导入 + 构建：

  ```powershell
  # 1) 导入 VS 开发者环境（Installer 先进 PATH，消除 vswhere 噪音）
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

  # 2) 构建（preset 任选）
  cmake --preset msvc-release      # 或 msvc-debug / clang-release(-O3 -march=native -flto)
  cmake --build --preset msvc-release
  ```
  注：**编译警告归零的验证只能用 `clang-release`**——MSVC 默认配置不报 `-Wnonportable-include-path`/`-Wreorder-ctor`/`-Wunused-*`/`-Wswitch` 等诊断。

- **Run:** 产物在 `build\<preset>\PlantsVsZombies.exe`，CMake 已把 `font/resources/Shader` 全拷到该目录旁——**运行/AutoTest 的工作目录就用 `build\<preset>\` 本身**：`Push-Location build\msvc-release; .\PlantsVsZombies.exe -AutoTest <绝对路径>.json`。（⚠️ 根目录的 `x64\Release` 是 CMake 迁移前 vcxproj 的陈旧产物，只剩 .obj、无 Shader，**勿用**。）
- **VS 里开发:** Visual Studio「打开文件夹」指向项目根目录，自动识别 CMakePresets；F5 调试配置在根目录 `launch.vs.json`（已设好工作目录与 `-Debug` 变体）
- **Debug mode:** Run with `-Debug` flag to show collision hitboxes
- **源文件管理:** `GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集——新增 .cpp 无需改任何构建文件；不参与编译的文件加进 CMakeLists 的 `REMOVE_ITEM` 排除名单（现有：`Reanimation/AttachmentSystem.cpp`）

The MSVC-Debug-MCP server exposes three families of tools — Build、Debug、Operate。注意：`build_solution` / `build_project` / `clean_solution` 依赖 .sln，已随 CMake 统一弃用，构建一律走上文的 cmake 命令行；Debug/Operate 两族在 VS「打开文件夹」(CMake) 模式下继续可用：

- **Debug (allowed):** `debugger_launch` (F5), `debugger_add_breakpoint`, then poll `debugger_status` until `Mode == "Break"`. While paused, read state via `debugger_get_callstack`, `debugger_get_locals`, and `debugger_evaluate` (Immediate-window expressions). Drive with `debugger_continue` / `step_*`, and always `debugger_stop` when done (a lingering Break freezes the game window).  
  - In a **Release** build, function locals are often optimized away (shown as "variable optimized away"), but **object members reached through `this` remain readable** — prefer breakpoints where `this` is in scope and evaluate members (e.g. `mRunning`, `mHaveCards.size()`) rather than bare locals.

- **Operate MSVC (allowed):** open/read/write/save documents, edit (`editor_find` / `editor_replace` / `editor_goto_line`), navigate code (`goto_definition`, `find_references`, `symbol_workspace`), and inspect projects (`project_list`, `solution_info`, `startup_project_set`).

Dependencies: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, Vulkan 1.3, glm, nlohmann/json, plugxml

Toolchain: C++17, `/utf-8` source encoding (required for the Chinese UI strings), Unicode character set, vcpkg static linking. Crash dialogs are produced by `CrashHandler` via a Windows Vectored Exception Handler — not visible on stderr in headless runs.

## 版本控制（提交策略）

**提交分工：`git commit` 由 Claude 负责（任务完成并验证通过后即可提交）；`git push` 由主人自己来——Claude 默认不 `push`，除非主人在该次对话中明确指示要推送。**

## AutoTest 自动化测试套件

`-AutoTest <script.json>` 启动参数让游戏按 JSON 脚本自动执行（进关/选卡/种植/出怪/截图/状态导出后退出），Claude 可独立完成「改代码→构建→跑脚本→Read 截图验证」闭环，无需人工游戏内截图。

- **脚本位置:** `autotest/scripts/*.json`（纯数据，不进 vcxproj；改脚本无需重编译）
- **运行（工作目录 = exe 所在的 `build\<preset>\`）:**
  ```powershell
  Push-Location build\msvc-release   # 或 build\clang-release
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
  $LASTEXITCODE   # 0=成功；1=命令失败/超时；100=脚本解析失败
  Pop-Location
  ```
- **产物:** `build\<preset>\autotest\out\<脚本名>\` 下的 PNG（用 Read 工具直接看）、`state.json`、`run.log`（每条命令的执行轨迹；Release 下 Logger INFO 被裁掉，run.log 是权威记录）
- **命令集:** `goto_level` / `choose_cards` / `wait_state` / `set_sun` / `plant` / `spawn_zombie` / `wait_seconds` / `wait_frames` / `set_timescale` / `click` / `key` / `screenshot` / `dump_state` / `quit`。等待型命令支持 `timeout`（默认 15s）。植物/僵尸类型用枚举标识符原文（如 `PLANT_PEASHOOTER`、`ZOMBIE_FASTPAPER`），新类型须在 `Game/AutoTest/TestDriver.cpp` 的名表加一行。
- **合成输入（任意场景的真实点击/按键链路）:** 现有 `plant`/`spawn_zombie` 等是直调游戏逻辑、只覆盖 GameScene；要驱动图鉴等非 GameScene 场景的 UI，用 `click`/`key`——它们经 `SDL_PushEvent` 注入合成事件，与真实用户输入同路径（下一帧 poll 消费、同样的 letterbox 坐标逆变换），对真实游戏零运行期影响（`TestDriver::Update` 非 AutoTest 即首行返回）。
  - `click`：`{ "op":"click", "x":570, "y":490 }`，可选 `"button"`（`left`(默认)/`right`/`middle`）、`"hold_frames"`（默认 1，按下到松开保持的帧数）。`x,y` 是**逻辑坐标**（与 UI 布局/`dump_state` 的 x/y 同一坐标系）。点击跨帧完成（按下沿→保持→松开沿），脚本无需手动等待。
  - `key`：`{ "op":"key", "name":"space" }`，可选 `"action"`（`press`(默认，按一下)/`down`(仅按下沿)/`up`(仅松开沿)）。`name` 是按键名串（`a`–`z`、`0`–`9`、`space`/`enter`/`escape`/`tab`/`backspace`/方向键 `up`/`down`/`left`/`right`/`f1`–`f12`…），新键在 `TestDriver.cpp` 的 `kKeyNames` 加一行。
- **隔离性:** AutoTest 模式下存档读写全部短路（不读不写 `saves/`），每次进关都是确定性全新关卡；`-Seed N` 固定随机种子。
- **范例:** `autotest/scripts/demo_peashooter.json`（验收脚本），`smoke_*.json`（各子系统冒烟）

## Architecture Overview

### Object Hierarchy
```
GameObject (base: component system, render order, active state)
└── AnimatedObject (adds Animator for sprite animation)
    ├── Plant → Shooter → PeaShooter
    │          SunFlower, WallNut, CherryBomb...
    ├── Zombie → ConeZombie, Polevaulter...
    └── Coin (collectibles)
Bullet (separate; uses object pooling via BulletPool)
```

### Component System
`GameObject` stores components in `std::unordered_map<std::type_index, shared_ptr<Component>>`. Standard components:
- `TransformComponent` — position (x, y), rotation, scale
- `ColliderComponent` — hitbox with `onTriggerEnter/Stay/Exit` and `onCollisionEnter/Exit` callbacks
- `ClickableComponent` — mouse interaction
- `ShadowComponent` — drop shadow rendering

Add/get components via `AddComponent<T>(args...)` and `GetComponent<T>()`.

### Key System Classes
| Class | File | Role |
|---|---|---|
| `Board` | `Game/Board.cpp` | Level manager: zombie waves, sun spawning, win/lose logic |
| `GameObjectManager` | `Game/GameObjectManager` | Create/destroy objects, render order, thread pool |
| `CollisionSystem` | `Game/CollisionSystem` | Per-frame collision checks, callbacks |
| `EntityManager` | `Game/EntityManager` | ID-based entity tracking (used by save system) |
| `SceneManager` | `Game/SceneManager` | Switches between scenes (`MainMenuScene`, `AlmanacScene`, `GameScene`, `PlantAlmanacScene`, `ZombieAlmanacScene`) — registered in `GameApp.cpp` |
| `ResourceManager` | `ResourceManager` | Asset loading/caching; keys in `ResourceKeys.h` |
| `Graphics` | `Graphics.cpp` | Custom Vulkan wrapper with transform stack and batch rendering |
| `Animator` | `Reanimation/Animator` | Named track system; `PlayTrack()`, `PlayTrackOnce()`, frame events |
| `ParticleSystem` | `ParticleSystem/` | XML-configured particle effects (`resources/particles/`) |
| `AudioSystem` | `Game/AudioSystem.h` | SDL2_mixer wrapper for SFX + music; channel management |
| `InputHandler` | `UI/InputHandler.{h,cpp}` | SDL event → mouse/keyboard state queried during Update |

### Game Loop (GameApp::Run)
1. **Input** — SDL events → `InputHandler`
2. **Update** — `SceneManager` → `Board::Update()` + `GameObjectManager::Update()` (spawning, AI, collision)
3. **Render** — `Draw()` walks objects in render order; internally calls `PrepareForDraw()` per object via the thread pool to parallel-prepare batch data, then records/submits Vulkan commands and `Graphics::FlushBatch()`

### Board Grid
The board is a `vector<vector<shared_ptr<Cell>>>` grid. Plants are placed at `(row, column)`. Zombies travel by row. `Board` manages zombie waves and game state transitions via the `BoardState` enum: `CHOOSE_CARD → GAME → WIN` or `LOSE_GAME` (`NONE` is the uninitialized state).

### Save System
JSON serialization via nlohmann/json (`GameInfoSaver`). Plants/zombies implement `SaveExtraData(json&)` and `LoadExtraData(const json&)` virtual methods for custom state. Save files live in `./saves/` (`PlayerInfo.json` for global state, `level{N}_data.json` per level); the directory is created on demand.

### Resources & Assets
- Asset keys are string constants in `ResourceKeys.h`, named `PREFIX_UPPERCASE` (e.g., `IMAGE_PEASHOOTER`, `SOUND_CHERRYBOMB`, `MUSIC_DAY`, `PARTICLE_EXPLOSIONCLOUD`). Helper class `ResourceKeyGenerator` derives keys from filenames.
- Asset roots: images in `./resources/image/`, particle XML configs in `./resources/particles/config/`, reanim files in `./resources/reanim/`, fonts in `./font/`.
- **Reanimation** (`Reanimation/`) is a custom skeletal-animation system, not a sprite-sheet player. It loads `.reanim` XML files and exposes named tracks (e.g. `anim_walk`) via `Animator::PlayTrack()`. Frame events register one-shot callbacks at specific frames.
- **Particle effects** are XML-configured under `./resources/particles/config/` (`<Emitter>` root with `<Image>`, `<LaunchSpeed>`, `<Field>`, etc.). `ParticleXMLLoader` caches by name.

## Reference & Implementation Guidance

When you need to **add a new classic plant, zombie, or bullet (projectile)**, it is recommended to refer to the original game's behavior and stats using the following approaches:

1. **Search the web**  
   Consult the *Plants vs. Zombies* community wiki, modding documentation, or open-source recreations to understand the attack patterns, health, speed, and special abilities of classic units (Pea Shooter, Wall-Nut, Basic Zombie, etc.).

2. **Consult the C# reference code (strongly recommended)**  
   This project was originally inspired by the Lawn engine's C# implementation. The complete code is located at:  
   `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`  
   The directory contains:
   - `Plant/` – logic for all plants (shooting, sun production, defense, etc.)
   - `Zombie/` – behavior for all zombies (movement, attack, helmet drop, etc.)
   - `Projectile/` – properties and collision logic for bullets (pea, ice pea, fireball, etc.)

   When implementing a new `Plant`, `Zombie` or `Bullet` subclass, **first consult the corresponding C# implementation** to ensure consistent stats and behavior with the original game.

3. **Animation troubleshooting** 
    If you encounter animation-related problems (e.g., a zombie fails to play a certain animation), first check the `./resources/reanim/` directory for the corresponding `.reanim` file – read its track names and frame data. Cross-reference with the C# reference code (`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`) to understand the expected animation sequence and timing. (the animation file is almost the same.)

4. **Frame event note:** 
    If your new plant or zombie needs frame events, please ask me first.

## Adding a New Plant
1. Subclass `Plant` (or `Shooter` for shooting plants) in `Game/Plant/`
2. Add entry to `PlantType` enum (`Game/Plant/PlantType.h`)
3. Register data (cost, cooldown, etc.) in `GameDataManager`
4. Load textures/animations via `ResourceManager` using a key in `ResourceKeys.h`
5. Add a corresponding `Card` entry
6. **(Reference)** For classic plants, consult the C# code or web resources mentioned in the "Reference & Implementation Guidance" section above.

## Adding a New Zombie
1. Subclass `Zombie` in `Game/Zombie/`
2. Add entry to `ZombieType` enum (`Game/Zombie/ZombieType.h`)
3. Override virtual methods: `ZombieUpdate()`, `TakeDamage()`, `SetupZombie()`,`HelmDrop()` / `ShieldDrop()`(etc.) as needed
4. **(Reference)** Check the C# implementation under `Zombie/` for attack intervals, health, special abilities, etc.

### Spawning zombies: two distinct paths
- **Gameplay (grid-bound):** `Board::CreateZombie(type, row, x, ...)` / `CreateZombieWithID(...)`. Pass an arbitrary pixel `x`, but **`y` is always derived from `row`** via `GetZombieSpawnY(row)` — there is intentionally no `y` parameter. Use this for real zombies, wave spawns, and savegame restore (saves persist only `row + x`).
- **Free placement (display only):** `GameAPP::InstantiateZombieFree(type, board, x, y)` for preview/UI zombies that must sit at an arbitrary `y` not snapped to a row (card-selection preview scatter, `AlmanacScene` / `ZombieAlmanacScene`). It wraps `InstantiateZombie(..., row = -1, isPreview = true)`. When `board != nullptr`, such zombies count toward `mBoard->mZombieNumber` and are decremented in `Zombie::Die`, so keep that increment/decrement balanced.

## Coding Conventions
- All game objects are managed as `shared_ptr`; use `weak_ptr` inside components (`mGameObjectWeak`) to avoid circular references
- Visual offsets use `mVisualOffset` (separate from the logical grid position)
- Row/column position (`mRow`, `mColumn`) is the gameplay grid cell; pixel position is in `TransformComponent`
- Chinese (UTF-8) strings are used throughout the codebase for UI 

## Communication Style
When responding to the user, always address them as **主人** (master) instead of using generic terms like "user" or "you". For example: "主人需要构建项目" rather than "你需要构建项目". This applies to all explanations, suggestions, and conversations within this repository context.