# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

This is a CMake + vcpkg(manifest) C++ project (x64 Windows only). The build system was unified onto CMake on 2026-06-13, migrating away from .sln/.vcxproj (`CMakeLists.txt` + `CMakePresets.json` + `vcpkg.json`, triplet `x64-windows-static`).

- **Build (Claude may run this autonomously):** Must run inside the VS developer environment. **Critical ordering: first add the Installer directory (where vswhere lives) to PATH, then import `VsDevCmd.bat`** — otherwise VsDevCmd's internal vswhere call emits `'vswhere.exe' is not recognized` (build still succeeds, but with noise). Noise-free one-shot import + build:

  ```powershell
  # 1) Import the VS dev environment (Installer onto PATH first to silence vswhere noise)
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

  # 2) Build (pick a preset: clang-release for shipping/perf, msvc-debug for F5 + hitbox debugging)
  cmake --preset clang-release      # -O3 -march=native -flto; or msvc-debug
  cmake --build --preset clang-release
  ```
  Note: the MSVC Release preset was removed on 2026-06-13 — `clang-release` is now the sole release build (better optimization, and the only config that reports `-Wnonportable-include-path`/`-Wreorder-ctor`/`-Wunused-*`/`-Wswitch` and similar diagnostics, so warning-zero verification lives here).

- **Run:** The binary is at `build\<preset>\PlantsVsZombies.exe`; CMake copies `font/resources/Shader` next to it — **use `build\<preset>\` itself as the working directory for Run/AutoTest**: `Push-Location build\clang-release; .\PlantsVsZombies.exe -AutoTest <absolute-path>.json`. (⚠️ The root `x64\Release` is a stale artifact from the pre-CMake vcxproj — only .obj files, no Shader, **do not use**.)
- **Develop in VS:** Visual Studio "Open Folder" pointed at the project root auto-detects CMakePresets; the F5 debug config is in `launch.vs.json` at the root (working directory and `-Debug` variant already set).
- **Debug mode:** Run with `-Debug` flag to show collision hitboxes
- **Source file management:** `GLOB_RECURSE CONFIGURE_DEPENDS` auto-collects sources — adding a new .cpp needs no build-file edits; files excluded from compilation go in the `REMOVE_ITEM` list in CMakeLists (current: `Reanimation/AttachmentSystem.cpp`).

Dependencies: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, Vulkan 1.3, glm, nlohmann/json, plugxml

Toolchain: C++17, `/utf-8` source encoding (required for the Chinese UI strings), Unicode character set, vcpkg static linking. Crash dialogs are produced by `CrashHandler` via a Windows Vectored Exception Handler — not visible on stderr in headless runs.

## Version Control (commit policy)

**Division of labor: `git commit` is Claude's job (commit once the task is done and verified); `git push` is the master's — Claude does not `push` by default unless the master explicitly says to push in that conversation.**

## AutoTest Suite

The `-AutoTest <script.json>` launch flag drives the game automatically from a JSON script (enter level / choose cards / plant / spawn zombies / screenshot / dump state, then exit). Claude can complete the full "edit code → build → run script → Read screenshot to verify" loop independently, with no manual in-game screenshots.

- **Script location:** `autotest/scripts/*.json` (pure data, not in vcxproj; editing scripts needs no recompile)
- **Run (working dir = `build\<preset>\` where the exe lives):**
  ```powershell
  Push-Location build\clang-release   # or build\msvc-debug
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
  $LASTEXITCODE   # 0=success; 1=command failed/timeout; 100=script parse failure
  Pop-Location
  ```
- **Artifacts:** under `build\<preset>\autotest\out\<script-name>\` — PNGs (view directly with the Read tool), `state.json`, `run.log` (per-command execution trace; in Release, Logger INFO is stripped, so run.log is the authoritative record)
- **Command set:** `goto_level` / `choose_cards` / `wait_state` / `set_sun` / `plant` / `spawn_zombie` / `wait_seconds` / `wait_frames` / `set_timescale` / `click` / `key` / `screenshot` / `dump_state` / `quit`. Wait-type commands accept `timeout` (default 15s). Plant/zombie types use the enum identifier verbatim (e.g. `PLANT_PEASHOOTER`, `ZOMBIE_FASTPAPER`); new types need a line added to the name table in `Game/AutoTest/TestDriver.cpp`.
- **Synthetic input (real click/key path for any scene):** existing `plant`/`spawn_zombie` etc. call game logic directly and only cover GameScene; to drive UI in non-GameScene scenes (almanac, etc.) use `click`/`key` — they inject synthetic events via `SDL_PushEvent`, taking the same path as real user input (consumed at the next frame's poll, same letterbox coordinate inverse-transform), with zero runtime impact on the real game (`TestDriver::Update` returns on its first line when not in AutoTest).
  - `click`: `{ "op":"click", "x":570, "y":490 }`, optional `"button"` (`left`(default)/`right`/`middle`), `"hold_frames"` (default 1, frames held between press and release). `x,y` are **logical coordinates** (same coordinate system as UI layout and `dump_state` x/y). A click completes across frames (press edge → hold → release edge); the script needs no manual waiting.
  - `key`: `{ "op":"key", "name":"space" }`, optional `"action"` (`press`(default, a full tap)/`down`(press edge only)/`up`(release edge only)). `name` is a key-name string (`a`–`z`, `0`–`9`, `space`/`enter`/`escape`/`tab`/`backspace`/arrows `up`/`down`/`left`/`right`/`f1`–`f12`…); new keys need a line added to `kKeyNames` in `TestDriver.cpp`.
- **Isolation:** in AutoTest mode all save reads/writes are short-circuited (no read/write of `saves/`); every level entry is a deterministic fresh level; `-Seed N` fixes the random seed.
- **Examples:** `autotest/scripts/demo_peashooter.json` (acceptance script), `smoke_*.json` (per-subsystem smoke tests)

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
- Visual offsets use `mVisualOffset` (separate from the logical grid position)
- Row/column position (`mRow`, `mColumn`) is the gameplay grid cell; pixel position is in `TransformComponent`
- Chinese (UTF-8) strings are used throughout the codebase for UI 

## Communication Style
When responding to the user, always address them as **主人** (master) instead of using generic terms like "user" or "you". For example: "主人需要构建项目" rather than "你需要构建项目". This applies to all explanations, suggestions, and conversations within this repository context.