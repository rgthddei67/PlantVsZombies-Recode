# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

This is a Visual Studio 2026 C++ project (x64 Windows only).

- **Build:** Open `PlantsVsZombies.sln` in Visual Studio 2026, press F7 (or Build Solution)
- **Run:** F5 in Visual Studio, or run the compiled `x64\Debug\PlantsVsZombies.exe`
- **Debug mode:** Run with `-Debug` flag to show collision hitboxes

> **Important:** After making code changes, do NOT attempt to build the project yourself (e.g., via MSVC or any build command). The user will handle all builds manually in Visual Studio.

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
| `GameObjectManager` | `Game/GameObjectManager` | Create/destroy objects, render order, thread pool for PrepareForDraw |
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

## Adding a New Plant
1. Subclass `Plant` (or `Shooter` for shooting plants) in `Game/Plant/`
2. Add entry to `PlantType` enum (`Game/Plant/PlantType.h`)
3. Register data (cost, cooldown, etc.) in `GameDataManager`
4. Load textures/animations via `ResourceManager` using a key in `ResourceKeys.h`
5. Add a corresponding `Card` entry

## Adding a New Zombie
1. Subclass `Zombie` in `Game/Zombie/`
2. Add entry to `ZombieType` enum (`Game/Zombie/ZombieType.h`)
3. Override virtual methods: `ZombieUpdate()`, `TakeDamage()`, `SetupZombie()`,`HelmDrop()` / `ShieldDrop()`(etc.) as needed
4. Register in `Board` wave-spawning logic

## Coding Conventions
- All game objects are managed as `shared_ptr`; use `weak_ptr` inside components (`mGameObjectWeak`) to avoid circular references
- Visual offsets use `mVisualOffset` (separate from the logical grid position)
- Row/column position (`mRow`, `mColumn`) is the gameplay grid cell; pixel position is in `TransformComponent`
- Chinese (UTF-8) strings are used throughout the codebase for UI text