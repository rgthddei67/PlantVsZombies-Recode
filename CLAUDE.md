# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

This is a Visual Studio 2026 C++ project (x64 Windows only).

- **Build:** Open `PlantsVsZombies.sln` in Visual Studio 2026, press F7 (or Build Solution)
- **Run:** F5 in Visual Studio, or run the compiled `x64\Debug\PlantsVsZombies.exe`
- **Debug mode:** Run with `-Debug` flag to show collision hitboxes

> **Important:** Do **not** use the MSVC-Debug-MCP server to build/compile/generate projects — that means **do not call** `build_solution`, `build_project`, or `clean_solution` unless I explicitly ask you to.  
> However, you **may freely use** the debugging, code navigation, and document editing tools described below.

The server exposes three families of tools:

- **Build (disabled by default):** `build_solution` / `build_project` / `clean_solution` — do not invoke these unless I give explicit permission.  
  *(Async note: `build_status` reflects the most recent VS build event and is shared between clean and build; a poll right after starting a build can return the previous `Done`. Cross-check with the output binary's timestamp before trusting completion.)*

- **Debug (allowed):** `debugger_launch` (F5), `debugger_add_breakpoint`, then poll `debugger_status` until `Mode == "Break"`. While paused, read state via `debugger_get_callstack`, `debugger_get_locals`, and `debugger_evaluate` (Immediate-window expressions). Drive with `debugger_continue` / `step_*`, and always `debugger_stop` when done (a lingering Break freezes the game window).  
  - In a **Release** build, function locals are often optimized away (shown as "variable optimized away"), but **object members reached through `this` remain readable** — prefer breakpoints where `this` is in scope and evaluate members (e.g. `mRunning`, `mHaveCards.size()`) rather than bare locals.

- **Operate MSVC (allowed):** open/read/write/save documents, edit (`editor_find` / `editor_replace` / `editor_goto_line`), navigate code (`goto_definition`, `find_references`, `symbol_workspace`), and inspect projects (`project_list`, `solution_info`, `startup_project_set`).

Dependencies: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, Vulkan 1.3, glm, nlohmann/json, plugxml

Toolchain: C++17, `/utf-8` source encoding (required for the Chinese UI strings), Unicode character set, vcpkg static linking. Crash dialogs are produced by `CrashHandler` via a Windows Vectored Exception Handler — not visible on stderr in headless runs.

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