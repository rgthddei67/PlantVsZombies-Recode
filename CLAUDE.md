# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

This is a Visual Studio 2022 C++ project (x64 Windows only).

- **Build:** Open `PlantsVsZombies.sln` in Visual Studio 2022, press F7 (or Build Solution)
- **Run:** F5 in Visual Studio, or run the compiled `x64\Debug\PlantsVsZombies.exe`
- **Debug mode:** Run with `-Debug` flag to show collision hitboxes
- **Speed cheat:** Press F3 during gameplay to toggle 5× game speed

Dependencies: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, OpenGL 3.3+, glad, glm, nlohmann/json

## Architecture Overview

### Object Hierarchy
```
GameObject (base: component system, render order, active state)
└── AnimatedObject (adds Animator for sprite animation)
    ├── Plant → Shooter → PeaShooter
    │          SunFlower, WallNut, CherryBomb
    ├── Zombie → ConeZombie, Polevaulter, ZombieCharred
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
| `SceneManager` | `Game/SceneManager` | Switch between `MainMenuScene` and `GameScene` |
| `ResourceManager` | `ResourceManager` | Asset loading/caching; keys in `ResourceKeys.h` |
| `Graphics` | `Graphics.cpp` | Custom OpenGL wrapper with transform stack and batch rendering |
| `Animator` | `Reanimation/Animator` | Named track system; `PlayTrack()`, `PlayTrackOnce()`, frame events |
| `ParticleSystem` | `ParticleSystem/` | XML-configured particle effects (`resources/particles/`) |

### Game Loop (GameApp::Run)
1. **Input** — SDL events → `InputHandler`
2. **Update** — `SceneManager` → `Board::Update()` + `GameObjectManager::Update()` (spawning, AI, collision)
3. **Render** — `PrepareForDraw()` per object (threaded), then `Draw()` in render-order, `Graphics::FlushBatch()`

### Board Grid
The board is a `vector<vector<shared_ptr<Cell>>>` grid. Plants are placed at `(row, column)`. Zombies travel by row. `Board` manages zombie waves and game state transitions (`CHOOSE_CARD → GAME → WIN/LOSE`).

### Save System
JSON serialization via nlohmann/json (`GameInfoSaver`). Plants/zombies implement `SaveExtraData(json&)` and `LoadExtraData(const json&)` virtual methods for custom state.

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