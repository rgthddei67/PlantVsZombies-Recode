# PlantVsZombies 项目指南

本文件是 Codex 在本仓库中工作的详细按需参考。根目录 `AGENTS.md` 只保留始终生效的规则和任务路由；两者如有冲突，以根目录 `AGENTS.md` 为准。

## 项目记忆

从 Claude Code 迁移而来的项目记忆保存在 `docs/agent-memory/`，现作为面向 Codex 的项目文档维护。

- 使用 `docs/agent-memory/MEMORY.md` 作为路由索引。诊断或修改现有子系统前，先按主题搜索索引，只读取相关的记忆文件。
- 记忆属于历史工程上下文，并非当前仓库状态的证明。依赖其中带日期的分支、提交、push、行号、构建和测试结论前，必须根据 Git 与当前代码重新核实。
- 明确的任务指令、根目录 `AGENTS.md`、当前源码以及当前测试/构建证据优先于冲突的记忆记录。
- 对已记录的子系统做出实质修改后，更新对应主题文件和 `MEMORY.md` 中的一行摘要；没有合适主题时，新建范围集中的主题文件。
- 不要依赖旧的 `~/.claude` 副本。仓库内副本是后续 Codex 工作的权威项目记忆。

## 构建与运行

这是一个使用 CMake + vcpkg（manifest 模式）的 C++ 项目，仅支持 x64 Windows。构建系统已于 2026-06-13 统一迁移到 CMake，不再使用 `.sln/.vcxproj`（`CMakeLists.txt` + `CMakePresets.json` + `vcpkg.json`，triplet 为 `x64-windows-static`）。

- **构建（Codex 可自主运行）：** CMake 已加入系统 `PATH`，应直接调用 `cmake`，不再定位或硬编码 Visual Studio 自带的 `cmake.exe`。构建仍必须在 VS 开发者环境中运行，以便提供编译器、Windows SDK 和相关工具链。**关键顺序：先把 `vswhere` 所在的 Installer 目录加入 `PATH`，再导入 `VsDevCmd.bat`**；否则 VsDevCmd 内部调用 vswhere 时会输出 `'vswhere.exe' is not recognized`（构建仍能成功，但会产生噪声）。无噪声的一次性环境导入与构建命令：

  ```powershell
  # 1) 导入 VS 开发者环境（先把 Installer 加入 PATH，避免 vswhere 噪声）
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

  # 2) 日常构建：Release 级运行性能 + PDB，但不做 LTO
  cmake --preset clang-playtest
  cmake --build --preset clang-playtest

  # 正式发布验证：相同优化再加 LTO，不生成 PDB
  cmake --preset clang-release
  cmake --build --preset clang-release
  ```

  三个预设各司其职：`clang-playtest` 用于日常编译和流畅试玩（`/O2`、AVX2、fast-math、PDB、无 LTO）；`clang-release` 只用于正式发布（额外 `-flto`、无 PDB）；`msvc-debug` 只在确实需要 Debug CRT/断点语义时使用。两个 Clang 预设都会报告 `-Wnonportable-include-path`、`-Wreorder-ctor`、`-Wunused-*`、`-Wswitch` 等诊断；日常零警告检查可用 playtest，只有正式发布、明确要求的发布验证或构建系统发布配置验收才运行 clang-release。

- **运行：** 可执行文件位于 `build\<preset>\PlantsVsZombies.exe`。`build\clang-release\resources` 与同级 `font` 是唯一实体目录；`clang-playtest`、`msvc-debug` 在首次配置时只创建 NTFS 目录联接，不复制资源。Shader、存档与 AutoTest 输出仍由各预设独立持有。运行游戏或 AutoTest 时，**必须以 exe 所在的 `build\<preset>\` 本身作为工作目录**：`Push-Location build\clang-playtest; .\PlantsVsZombies.exe -AutoTest <absolute-path>.json`。（⚠️ 根目录的 `x64\Release` 是陈旧产物，**禁止使用**。）
- **在 VS 中开发：** 用 Visual Studio 的“打开文件夹”打开项目根目录，VS 会自动识别 CMakePresets。根目录 `launch.vs.json` 已包含 F5 调试配置、工作目录和 `-Debug` 变体。
- **调试模式：** 使用 `-Debug` 参数运行可显示碰撞框。
- **源文件管理：** `GLOB_RECURSE CONFIGURE_DEPENDS` 会自动收集源文件，新增 `.cpp` 无需修改构建文件；不参与编译的文件放入 `CMakeLists.txt` 的 `REMOVE_ITEM` 列表（当前为 `Reanimation/AttachmentSystem.cpp`）。

依赖：SDL2、SDL2_image、SDL2_ttf、SDL2_mixer、Vulkan 1.3、glm、nlohmann/json、plugxml。

工具链：C++17；源码使用 `/utf-8` 编码（中文 UI 字符串所必需）；Unicode 字符集；vcpkg 静态链接。无头运行时，`CrashHandler` 通过 Windows Vectored Exception Handler 生成的崩溃对话框不会出现在 stderr。

## 版本控制

- Codex 负责提交已完成且验证通过的工作，然后根据当前风险和仓库状态决定是否 push。
- 工作已完成并验证、改动范围与任务一致、目标上游明确，且可常规 fast-forward 时执行 push；否则保留本地提交并说明原因。
- 未经明确批准，不得 force-push、改写已发布历史，或发布无关/敏感改动。主人的明确指令始终优先。

## AutoTest 测试套件

启动参数 `-AutoTest <script.json>` 会通过 JSON 脚本自动驱动游戏（进入关卡、选卡、种植、生成僵尸、截图、导出状态，然后退出）。Codex 可以独立完成“修改代码 → 构建 → 运行脚本 → 读取截图验证”的完整闭环，无需主人手动提供游戏截图。

- **脚本位置：** `autotest/scripts/*.json`（纯数据，不属于编译目标；修改脚本无需重新编译）。
- **运行方式（工作目录必须是 exe 所在的 `build\<preset>\`）：** Codex 默认必须让窗口显示在主人当前桌面。GUI 启动属于沙箱外桌面操作，调用 shell 时使用 `sandbox_permissions="require_escalated"`；仅写 `-WindowStyle Normal` 而不提升权限，进程仍可能落入隔离会话、主人看不到。推荐命令：

  ```powershell
  Push-Location build\clang-playtest   # 发布验证可用 clang-release；必须作为 WorkingDirectory
  $exe = (Resolve-Path '.\PlantsVsZombies.exe').Path
  $script = (Resolve-Path '..\..\autotest\scripts\demo_peashooter.json').Path
  $process = Start-Process -FilePath $exe `
    -ArgumentList @('-AutoTest', "`"$script`"", '-Seed', '42') `
    -WorkingDirectory (Get-Location).Path -WindowStyle Normal -PassThru
  $process.WaitForExit()
  $exitCode = $process.ExitCode   # 0=成功；1=命令失败/超时；100=脚本解析失败
  Pop-Location
  exit $exitCode
  ```

  启动后可用桌面窗口枚举确认出现 `PlantsVsZombies.exe` / “植物大战僵尸中文版”；主人报告未显示时，先核对提升权限和工作目录，不要重复普通 shell 启动。AutoTest 自动退出；需要主人亲自操作时改为同方案启动不带 `-AutoTest` 的普通游戏，或在测试脚本中加入明确观察停留时间。

- **产物：** 位于 `build\<preset>\autotest\out\<script-name>\`，包括 PNG、`state.json` 和 `run.log`。Release 会裁掉 Logger INFO，因此 `run.log` 是权威命令执行记录。
- **命令集：** `goto_level` / `choose_cards` / `wait_state` / `set_sun` / `set_weather` / `set_typhoon` / `roll_typhoon` / `reroll_typhoon_direction` / `trigger_typhoon_gust` / `set_weather_forecast` / `show_image_prompt` / `roll_weather_forecast` / `advance_weather_phase` / `trigger_lightning` / `set_adventure_level` / `force_trophy` / `force_survival_round` / `force_survival_round_clear` / `summon_next_wave` / `plant` / `assert_can_plant` / `spawn_bullet` / `spawn_zombie` / `spawn_wave_zombie` / `damage_plant` / `damage_zombie` / `add_perk` / `survival_perk_open` / `survival_perk_pick` / `survival_perk_refresh` / `wait_seconds` / `wait_frames` / `set_timescale` / `charm_zombie` / `move_mouse` / `click` / `key` / `screenshot` / `dump_state` / `assert_state` / `quit`。等待类命令接受 `timeout`（默认 15 秒）。`assert_can_plant` 用 `type/row/col/expected` 直接断言正式 `Board::CanPlantAt`，适合覆盖睡莲承载层、水路禁种与弹坑等网格规则。`set_weather_forecast` 固定公开预报、真实天气和揭晓倒计时，只用于天气 UI/失败提示的确定性测试；当 `actual=HEAVY` 时可用 `typhoonStrength=NONE/TYPHOON/SEVERE/SUPER` 与 `promptVariant=0..2` 固定待生效台风和同级预警文案。`show_image_prompt` 用 `image=HUGE_WAVE/FINAL_WAVE` 显示既有图片提示，供多提示并存与绘制顺序测试。`roll_weather_forecast` 只在晴天用 1-based `weatherRoll` 走正式动态权重与弱天气保底，再发布必定准确的锁定预报，可用 `revealIn` 固定揭晓倒计时。`set_typhoon` 只在大雨中生效，用 `strength=NONE/TYPHOON/SEVERE/SUPER`、`direction=NONE/HOUSE/FRONT` 固定台风状态；可选 `gustIn`、`directionIn`、`gustsRemaining` 和 `decayIn` 固定阵风、转向、预算与衰减计时，`roll_typhoon` 用 1-based `chanceRoll`/`strengthRoll` 和固定方向走正式概率、连续落空保底与动态强度边界。`reroll_typhoon_direction` 用 `directionRoll=1..2` 走正式风向二选一重抽，确定性覆盖继续同向与切换方向。`trigger_typhoon_gust` 启动一次不消费自动预算的正式阵风，可用 `plantMoveIn` 固定阵风开始后多少游戏秒结算植物（默认 0 保持旧脚本的立即结算），活动期间仍会连续吹动僵尸。`force_survival_round` 直接定位测试轮次、重建出怪池并刷新轮次派生的天气速度；`force_survival_round_clear` 走正式轮清入口。`summon_next_wave` 直接走正式 `Board::SummonNextWave()`，可用 `count=1..100` 连续推进并验证波次派生状态；`spawn_zombie` 可加 `frozen=true` 让新目标立即走正式冻结入口；`spawn_wave_zombie` 额外要求 `mutationRoll=1..100`，以正式天气变异解析器创建波次候选，用于确定性测试条件变异和每波上限；候选超过上限时命令成功但不创建回退类型，与正式挑选循环的 `continue` 一致。`spawn_bullet` 直接创建对象池子弹，可固定 `velocityX/velocityY/damage`，用于断言风力派生速度与伤害；目前只开放豌豆、寒冰豌豆和孢子。`damage_plant` 按 `row/col/index`、`damage_zombie` 按 `row/index` 选目标并走正式 `TakeDamage` 链；两者的 `source` 可取 `PLANT/ZOMBIE/OTHER`（默认 `OTHER`），后者另可选 `penetrateShield`，用于来源词条、护盾、断肢和死亡动画测试。`set_adventure_level` 与 `force_trophy` 仅用于冒险进度结算测试；`survival_perk_refresh` 消耗本轮共享的一次刷新额度并重抽当前全部词条候选。植物/僵尸类型直接使用枚举标识符（例如 `PLANT_PEASHOOTER`、`ZOMBIE_FASTPAPER`），新增类型需要在 `Game/AutoTest/TestDriver.cpp` 的名称表中添加一行。
- **长时序隔离：** `set_spawn_paused` 以 `value=true/false` 暂停或恢复自然出波，不影响 `spawn_zombie`、`summon_next_wave` 等显式命令，适合成长、恢复和长计时测试；脚本离开隔离段前应显式恢复 `false`。
- **静止测试靶：** `spawn_zombie` 可加 `stationary=true` 把该实例的基础 Animator 速度设为 0，从而停止 `_ground` 位移；它不伪造冻结/减速状态，适合长时间射击成长与承伤验证。
- **合成输入（所有场景共用真实 click/key 路径）：** 现有 `plant`、`spawn_zombie` 等操作直接调用游戏逻辑，只覆盖 GameScene。驱动图鉴等非 GameScene UI 时使用 `click` / `key`；它们通过 `SDL_PushEvent` 注入合成事件，走与真实输入相同的路径（在下一帧 poll 时消费，并使用同一套 letterbox 坐标逆变换）。正常游戏没有运行时开销，因为非 AutoTest 模式下 `TestDriver::Update` 第一行就会返回。
  - `move_mouse`：`{ "op":"move_mouse", "x":440, "y":298 }`，只注入鼠标移动事件而不按键，适合截取悬停预览和 hover 状态。
  - `click`：`{ "op":"click", "x":570, "y":490 }`，可选 `"button"`（`left`，默认 / `right` / `middle`）和 `"hold_frames"`（默认 1，即按下到释放之间保持的帧数）。`x,y` 是**逻辑坐标**，与 UI 布局和 `dump_state` 的 x/y 使用同一坐标系。一次 click 会跨帧完成（按下沿 → 保持 → 释放沿），脚本无需手动等待。
  - `assert_state`：`{ "op":"assert_state", "path":"perks.stacks.PLANT_DAMAGE_UP", "equals":3 }`，断言对象与 `dump_state` 写出的状态 JSON 相同。`path` 使用点分段；纯数字段用于索引数组（如 `zombies.0.type`）。数值字段也可用 `atLeast` / `atMost`（可同时给出）做闭区间断言。不匹配或路径缺失会失败并返回 exit 1。不要对浮点字段（如 `zombieHealthMult`）使用 `equals`，因为它执行 JSON 精确比较；优先断言整数投影字段（如 `plantDamageOn100`）。
  - `key`：`{ "op":"key", "name":"space" }`，可选 `"action"`（`press`，默认，完整点击 / `down`，仅按下沿 / `up`，仅释放沿）。`name` 是键名字符串：`a`–`z`、`0`–`9`、`space` / `enter` / `escape` / `tab` / `backspace`、方向键 `up` / `down` / `left` / `right`、`f1`–`f12` 等；新增键名需要在 `TestDriver.cpp` 的 `kKeyNames` 中添加一行。
- **隔离性：** AutoTest 模式默认短路所有存档读写（不读取或写入 `saves/`）；每次进入关卡都是确定性的全新关卡；`-Seed N` 固定随机种子。只有必须复现真实关卡存档问题时才显式加 `-AutoTestLoadSave`：此参数允许 `LoadLevelData` 从当前构建目录的 `./saves/` 读取关卡存档，但玩家设置仍用 AutoTest 默认值，且保存和删除入口继续短路，因此是严格只读模式。
- **射手存档观测：** `dump_state` 根节点提供 `plantCount`、`bulletCount`、`repeatingShootingHeadCount`；Shooter 植物条目另含 `headTrack`、`headAnimPlaying`、`headAnimPlayState`，可配合 `assert_state` 验证附加头部 Animator。
- **示例：** `autotest/scripts/demo_peashooter.json`（验收脚本）以及各子系统的 `smoke_*.json`。

## 架构概览

### 对象层次

```text
GameObject（基类：组件系统、渲染顺序、激活状态）
└── AnimatedObject（增加 Animator 精灵动画）
    ├── Plant → Shooter → PeaShooter
    │          SunFlower、WallNut、CherryBomb……
    ├── Zombie → ConeZombie、Polevaulter……
    └── Coin（可收集物）
Bullet（独立类型；通过 BulletPool 使用对象池）
```

### 组件系统

`GameObject` 使用 `std::unordered_map<std::type_index, shared_ptr<Component>>` 保存组件。标准组件包括：

- `TransformComponent`：位置（x、y）、旋转、缩放。
- `ColliderComponent`：碰撞框，以及 `onTriggerEnter/Stay/Exit`、`onCollisionEnter/Exit` 回调。
- `ClickableComponent`：鼠标交互。
- `ShadowComponent`：阴影渲染。

通过 `AddComponent<T>(args...)` 添加组件，通过 `GetComponent<T>()` 获取组件。

### 关键系统类

| 类 | 文件 | 职责 |
|---|---|---|
| `Board` | `Game/Board.cpp` | 关卡管理：僵尸波次、阳光生成、胜负逻辑 |
| `GameObjectManager` | `Game/GameObjectManager` | 创建/销毁对象、渲染顺序、线程池 |
| `CollisionSystem` | `Game/CollisionSystem` | 每帧碰撞检测与回调 |
| `EntityManager` | `Game/EntityManager` | 按 ID 跟踪实体（存档系统使用） |
| `SceneManager` | `Game/SceneManager` | 在 `MainMenuScene`、`AlmanacScene`、`GameScene`、`PlantAlmanacScene`、`ZombieAlmanacScene` 间切换；场景在 `GameApp.cpp` 注册 |
| `ResourceManager` | `ResourceManager` | 加载/缓存资源；资源键定义在 `ResourceKeys.h` |
| `Graphics` | `Graphics.cpp` | 自定义 Vulkan 封装，含变换栈与批渲染 |
| `Animator` | `Reanimation/Animator` | 命名轨道动画系统；提供 `PlayTrack()`、`PlayTrackOnce()` 和帧事件 |
| `ParticleSystem` | `ParticleSystem/` | 由 `resources/particles/` 下 XML 配置驱动的粒子系统 |
| `AudioSystem` | `Game/AudioSystem.h` | SDL2_mixer 音效与音乐封装，管理声道 |
| `InputHandler` | `UI/InputHandler.{h,cpp}` | 将 SDL 事件转换成 Update 阶段查询的鼠标/键盘状态 |

### 游戏循环（`GameApp::Run`）

1. **输入：** SDL 事件 → `InputHandler`。
2. **更新：** `SceneManager` → `Board::Update()` + `GameObjectManager::Update()`，处理生成、AI 和碰撞。
3. **渲染：** `Draw()` 按渲染顺序遍历对象；内部通过线程池对每个对象调用 `PrepareForDraw()` 并行准备批数据，然后录制/提交 Vulkan 命令并调用 `Graphics::FlushBatch()`。

### Board 网格

棋盘是 `vector<vector<shared_ptr<Cell>>>` 网格。植物放置在 `(row, column)`，僵尸按行移动。`Board` 管理僵尸波次以及 `BoardState` 状态转换：`CHOOSE_CARD → GAME → WIN` 或 `LOSE_GAME`；`NONE` 表示尚未初始化。

`Board` 拥有当前关卡的行数、首行 Y 与行高：普通草地为 5×100px，泳池背景为 6×85px（水路是 0-based 第 2/3 行）。位置、弹坑、子弹影子与小推车必须优先调 `GetCellCenterPosition` / `GetCellHeight`，不要再硬编 `CELL_INITALIZE_POS_Y + row*100`。`Cell` 分 `under/normal` 两层植物槽；正式放置入口是 `Board::CanPlantAt`，铲子与僵尸啃咬只选 `GetTopPlantAt`。

### 存档系统

使用 nlohmann/json 进行 JSON 序列化（`GameInfoSaver`）。植物和僵尸通过 `SaveExtraData(json&)`、`LoadExtraData(const json&)` 保存和恢复自定义状态。`PlayerInfo.json` 保存全局状态，`level{N}_data.json` 保存各关卡状态。Windows 通过 `FOLDERID_SavedGames` 写入系统“保存的游戏”目录（默认 `%USERPROFILE%\Saved Games\PlantsVsZombies\saves`）；Android 仍使用 `SDL_GetPrefPath`，Linux 暂沿用 `./saves/`。

Windows 首次发生真实存档访问时，会把当前工作目录旧 `./saves/` 中的普通文件复制到中央目录、逐字节校验后再删除源文件，跨磁盘同样安全。目标已有相同文件时只清理重复源文件；同名但内容不同则中央档优先、旧档原地保留且记录警告；迁移失败的缺失文件仍可逐文件回退旧目录读取。AutoTest 不触发迁移，`-AutoTestLoadSave` 始终只读构建目录下的 `./saves/`。

### 资源与资产

- 资源键是在 `ResourceKeys.h` 中手写的字符串常量，命名为 `PREFIX_UPPERCASE`（如 `IMAGE_PEASHOOTER`、`SOUND_CHERRYBOMB`、`MUSIC_DAY`、`PARTICLE_EXPLOSIONCLOUD`）。它们是 `ResourceManager::GenerateStandardKey` 根据文件名生成的实际键的防拼写错误镜像：去掉目录和扩展名 → 转大写 → 非字母数字转 `_` → 添加前缀。键值与常量名相同时，用 `RKEY(X)` 宏（展开为 `inline const std::string X = "X"`），避免重复书写；键值与名称不同时（例如 `IMAGE_HUGE_WAVE_APPROACHING = "IMAGE_APPROACHING"`、`SOUND_SHOOTER_SHOOT = "SOUND_THROW"`、值为 CamelCase 的 `REANIM_*`、字体路径），必须显式声明。
- 资产根目录：图片在 `./resources/image/`，粒子 XML 在 `./resources/particles/config/`，reanim 文件在 `./resources/reanim/`，字体在 `./font/`。
- **Reanimation：** `Reanimation/` 是自定义骨骼动画系统，不是精灵表播放器。它加载 `.reanim` XML 文件，通过 `Animator::PlayTrack()` 播放命名轨道（如 `anim_walk`）。帧事件可在指定帧注册一次性回调。
- **粒子特效：** 粒子效果通过 `./resources/particles/config/` 下 XML 配置（根标签为 `<Emitter>`，包含 `<Image>`、`<LaunchSpeed>`、`<Field>` 等）；`ParticleXMLLoader` 按名称缓存。

## 参考与实现指引

新增**经典植物、僵尸或子弹（projectile）**时，建议通过以下方式核对原版行为与数值：

1. **搜索网络**
   查阅 *Plants vs. Zombies* 社区 Wiki、Mod 文档或开源复刻，确认经典单位的攻击方式、生命值、速度和特殊能力。

2. **查阅 C# 参考代码（强烈建议）**
   本项目最初参考 Lawn 引擎的 C# 实现，完整代码位于：
   `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`

   目录包括：

   - `Plant/`：全部植物逻辑（射击、产阳光、防御等）。
   - `Zombie/`：僵尸行为（移动、攻击、头盔掉落等）。
   - `Projectile/`：子弹属性与碰撞逻辑（豌豆、寒冰豌豆、火球等）。

   实现新的 `Plant`、`Zombie` 或 `Bullet` 子类前，**先查阅对应 C# 实现**，确保数值和行为与原版一致。

3. **动画故障排查**
   新植物或僵尸无法播放某段动画时，先检查 `./resources/reanim/` 中对应 `.reanim` 文件的轨道名称和帧数据，再对照 C# 参考代码确认预期动画序列与时序（动画文件基本一致）。

4. **帧事件要求**
   新植物或僵尸需要帧事件时，必须先询问主人。

## 新增植物

使用 `adding-plant` 技能（`.agents/skills/adding-plant/SKILL.md`）；它取代了原先放在本文件中的检查清单。

## 新增僵尸

使用 `adding-zombie` 技能（`.agents/skills/adding-zombie/SKILL.md`）；它取代了原先放在本文件中的检查清单。该技能已在舞王僵尸 + 伴舞僵尸上验证，覆盖断肢断头、召唤编队、出土裁剪、魅惑交互、帧事件陷阱和调参量交付。

### 生成僵尸的两条不同路径

- **游戏逻辑路径（绑定网格）：** `Board::CreateZombie(type, row, x, ...)` / `CreateZombieWithID(...)`。可以传任意像素 `x`，但 **`y` 始终通过 `GetZombieSpawnY(row)` 由 `row` 推导**，有意不提供 `y` 参数。真实僵尸、波次生成和存档恢复都使用此路径；存档只持久化 `row + x`。
- **自由放置路径（仅展示）：** `GameAPP::InstantiateZombieFree(type, board, x, y)` 用于必须放在任意 `y`、不能吸附到行的预览/UI 僵尸（选卡预览散布、`AlmanacScene` / `ZombieAlmanacScene`）。它封装 `InstantiateZombie(..., row = -1, isPreview = true)`。当 `board != nullptr` 时，这类僵尸会计入 `mBoard->mZombieNumber`，并在 `Zombie::Die` 中递减，因此必须保持增减平衡。

## 编码约定

- 视觉偏移使用 `mVisualOffset`，与逻辑网格位置分离。
- `mRow`、`mColumn` 表示游戏网格单元；像素位置存放在 `TransformComponent`。
- 代码库中的中文 UI 字符串使用 UTF-8。
- **头文件保护（每个 `.h`）：** 每个头文件必须以 `#pragma once` 开头。旧有的 `#pragma once` + `#ifndef _NAME_H` 双重形式也可接受。运行 `cmake --preset` 时会安装 `.githooks/pre-commit` hook（`git config core.hooksPath .githooks`），拒绝暂存区中缺少保护的头文件；同一配置步骤也会用 WARNING 列出仓库里已有的无保护头文件。检查支持 BOM：在前 512 字节内匹配 token，而不是锚定 `^`，因此 UTF-8 BOM 不会造成误报。原因是迁移掉 `.sln` 后，VS 的“添加新项”模板不再自动插入保护。

## 沟通风格

回复时始终称呼用户为 **主人**，不要使用泛化的“用户”或“你”。例如使用“主人需要构建项目”。本规则适用于本仓库上下文中的所有解释、建议和对话。
