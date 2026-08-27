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

这是一个使用 CMake + vcpkg（manifest 模式）的 C++ 项目，仅支持 x64 Windows。构建系统已于 2026-06-13 统一迁移到 CMake，不再使用 `.sln/.vcxproj`（`CMakeLists.txt` + `CMakePresets.json` + `vcpkg.json`，triplet 为 `x64-windows-static`）；仓库内专用依赖通过 `cmake/vcpkg-ports` overlay port 提供。

- **构建（Codex 可自主运行）：** CMake 已加入系统 `PATH`，应直接调用 `cmake`，不再定位或硬编码 Visual Studio 自带的 `cmake.exe`。构建仍必须在 VS 开发者环境中运行，以便提供编译器、Windows SDK 和相关工具链。**关键顺序：先把 `vswhere` 所在的 Installer 目录加入 `PATH`，再导入 `VsDevCmd.bat`**；否则 VsDevCmd 内部调用 vswhere 时会输出 `'vswhere.exe' is not recognized`（构建仍能成功，但会产生噪声）。无噪声的一次性环境导入与构建命令：

  ```powershell
  # 1) 导入 VS 开发者环境（先把 Installer 加入 PATH，避免 vswhere 噪声）
  $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
  $vs = & vswhere -latest -property installationPath
  cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

  # 2) 所有任务的默认构建、F5 和 AutoTest 产物
  cmake --preset clang-release
  cmake --build --preset clang-release

  # 3) 只有明确需要 Debug CRT/Debug 语义或辅助定位 Release 问题时才切换
  cmake --preset clang-debug
  cmake --build --preset clang-debug

  # 4) Win7/旧环境出现 0xC000001D 时的同配置无 AVX2 发布版
  cmake --preset clang-release-noavx2
  cmake --build --preset clang-release-noavx2

  ```

  三个预设统一使用 `clang-cl + lld-link`。`clang-release` 是所有普通功能、逻辑、UI、资源、存档、性能和架构任务的唯一默认预设：直接用它迭代、F5、跑范围最小的 AutoTest 并交付。同一份当前源码已用 Release 产物完成相关验证时，不再必跑一轮 Debug 构建或重复 AutoTest。`clang-debug` 仅在主人明确要求 Debug CRT/Debug 语义，或 Release 问题确实需要辅助诊断时显式使用；`clang-release-noavx2` 仅用于 Win7/旧 CPU 兼容诊断。`clang-release`/`clang-release-noavx2` 用 `/Z7 -gline-tables-only -gcodeview-ghash` 与 `/DEBUG:GHASH`，只保留符号化函数栈和源码行所需的信息，不写变量/类型，并缩短 LLD 合并；EXE 只保留同目录 PDB 文件名的定位记录，不嵌入调试符号或本机构建绝对路径。三个 Clang 预设都会报告 `-Wnonportable-include-path`、`-Wreorder-ctor`、`-Wunused-*`、`-Wswitch` 等诊断，并应保持零警告。

- **Release 崩溃取证：** `clang-release` 的 Fatal Error / Access Violation 先保留 `crash_report_*.txt`、现场资源 WARN、触发脚本和崩溃阶段，不凭异常地址猜源码；同次构建的精简 PDB 可符号化函数栈、内联关系和源码行，但不能检查变量或类型。若 LTO 内联/合并使调用栈仍难以定位，优先给最小复现补充针对性的状态投影、日志或断言；同一路径能在 `clang-debug` 复现时可显式用其辅助诊断。新动画对象若在构造或首帧崩溃，先查 reanim 注册、动画类型映射和轨道资源；不要在 `Zombie`/`AnimatedObject` 基类添加宽泛空 Animator 早退，这通常只会把崩溃推迟到 `Start()`/`SetupZombie()` 并掩盖强制资源缺失。修复后用 `clang-release` 重建并重跑原失败脚本及父类回归。

- **运行：** 可执行文件位于 `build\<preset>\PlantsVsZombies.exe`。`build\clang-release\resources` 与同级 `font` 是唯一实体目录；`clang-release-noavx2`、`clang-debug` 在首次配置时只创建 NTFS 目录联接，不复制资源。Shader、存档与 AutoTest 输出仍由各预设独立持有。运行游戏或 AutoTest 时，**必须以 exe 所在的 `build\<preset>\` 本身作为工作目录**：`Push-Location build\clang-release; .\PlantsVsZombies.exe -AutoTest <absolute-path>.json`。（⚠️ 根目录的 `x64\Release` 是陈旧产物，**禁止使用**。）
- **在 VS 中开发：** 用 Visual Studio 的“打开文件夹”打开项目根目录，VS 会自动识别 CMakePresets。根目录 `launch.vs.json` 已包含 F5 调试配置、工作目录和 `-Debug` 变体。
- **调试模式：** 使用 `-Debug` 参数运行可显示碰撞框。
- **源文件管理：** `GLOB_RECURSE CONFIGURE_DEPENDS` 会自动收集源文件，新增 `.cpp` 无需修改构建文件；不参与编译的文件放入 `CMakeLists.txt` 的 `REMOVE_ITEM` 列表（当前为 `Reanimation/AttachmentSystem.cpp`）。

依赖：SDL2、SDL2_image、SDL2_ttf、SDL2_mixer、Vulkan 1.2、Volk、OpenGL 3.3 Core、glm、nlohmann/json、pugixml、YY-Thunks。Vulkan运行时入口由 SDL2 选定 loader 后交给 Volk动态加载；Vulkan SDK继续提供头文件、VMA 与 `glslc`，但 EXE 不直接链接 `vulkan-1.dll`。Vulkan 最低设备能力仍包含 `VK_KHR_swapchain`、Vulkan 1.2 bindless descriptor indexing 所需 feature，以及至少 8192 个 update-after-bind combined image sampler；OpenGL 兼容后端不降低 Vulkan 要求，也不使用扩展、SSBO、Bindless 或 GPU Instancing。默认 `clang-release` 要求 x64 + AVX2；`clang-release-noavx2` 的项目源码回到 x64 基线指令集，只用于排除 CPU/系统 XState 状态造成的 `0xC000001D`，不会降低 GPU 要求。

渲染器启动参数为 `-Renderer=auto|vulkan|opengl`，缺省等价于 `auto`：优先 Vulkan，初始化失败时销毁 Vulkan 对象和 Vulkan 窗口，再创建独立的 OpenGL 3.3 Core 窗口；强制 `vulkan` 不回退，强制 `opengl` 不触碰 SDL Vulkan loader。OpenGL 使用独立 GLSL 330、CPU 矩阵展开、单 sampler 动态 VBO/IBO Batch 和 CPU Reanimation 慢路径；为保证 Context 线程约束，它关闭并行 Draw record，但 `GameObjectManager` 的多线程 Update 不变。`-NoInstance` 在 OpenGL 下允许使用并记录为“CPU Batch 路径不变”。显式 AutoTest 故障注入仅使用 `-TestVulkanInitFailure`，不得接入正常玩家行为。

Windows 发布产物以 Windows 7 SP1 x64（PE subsystem 6.01）为最低系统。项目内 `yy-thunks` overlay port 固定官方 v1.2.2 的 Lib 与 Objs 资产：Clang/LLD 把 Win7 替代 import libraries 放在 WinSDK 前，MSVC `link.exe` 直接链接官方 `YY_Thunks_for_Win7.obj`；例如 `CopyFile2`、`CreateFile2`、`GetSystemTimePreciseAsFileTime` 等新 API 会运行时探测并在 Win7 走回退。每个 Windows 可执行目标链接后，`cmake/assert_win7_imports.ps1` 都会用 `llvm-readobj` 将直接 PE imports 与随包 Win7 x64 导出表逐项核对；新增依赖若引入 Win7 不存在且 YY-Thunks 未接管的入口，构建必须失败，禁止靠放宽白名单掩盖。此兼容层只解决系统 API 装载门槛；CPU 指令集由构建预设独立决定，GPU 的 Vulkan 1.2 与 bindless 设备能力要求不会因此降低。

Vulkan 运行时把 dynamic rendering 与 synchronization2 **分别**选路：Vulkan 1.3 优先核心入口；1.2 驱动若提供 `VK_KHR_dynamic_rendering` / `VK_KHR_synchronization2` 就使用对应 KHR 入口；缺少任一扩展时分别回退到传统 RenderPass / `vkCmdPipelineBarrier` + `vkQueueSubmit`，不会因为只缺其中一个而放弃另一个快路径。兼容矩阵开关为 `-Vulkan12`（把协商限制到 1.2）、`-VulkanLegacyRendering`、`-VulkanLegacySync`，`-Vulkan12Fallback` 等价于三者同时启用。它们只用于测试和故障诊断，正常启动保持 1.3 核心快路径。

工具链：C++17；源码使用 `/utf-8` 编码（中文 UI 字符串所必需）；Unicode 字符集；vcpkg 静态链接。无头运行时，`CrashHandler` 通过 Windows Vectored Exception Handler 生成的崩溃对话框不会出现在 stderr。

### TestDriver 的 Release 编译例外

`PlantVsZombies/Game/AutoTest/TestDriver.cpp` 只在 AutoTest 模式执行重逻辑；Clang Release 对该翻译单元保留正式 ABI、AVX2、`-flto` 和精简行表 PDB，但在源文件级最后追加 `/Od`，覆盖目标级 `/O2`。这不会切换 Debug CRT、定义 `_DEBUG` 或退出正式 LTO 链路；普通游戏每个逻辑步只多保留一次未优化的单例取址与 `mActive` 早退，现有名称映射仍在启动时构造，但不进入 JSON 命令重逻辑。2026-08-25 同机 Ninja 日志实测对象编译由 440.47 秒降至 4.12 秒，随后 LTO 链接及后处理为 43.07 秒，Win7 378 项导入审计与可见 `smoke_quit` 均通过。若调整此例外，必须分别比较对象与最终链接耗时，不能只用整次并行构建墙钟时间判断。

## 版本控制

- Codex 负责提交已完成且验证通过的工作，然后根据当前风险和仓库状态决定是否 push。
- 工作已完成并验证、改动范围与任务一致、目标上游明确，且可常规 fast-forward 时执行 push；否则保留本地提交并说明原因。
- 未经明确批准，不得 force-push、改写已发布历史，或发布无关/敏感改动。主人的明确指令始终优先。

## AutoTest 测试套件

启动参数 `-AutoTest <script.json>` 会通过 JSON 脚本自动驱动游戏（进入关卡、选卡、种植、生成僵尸、截图、导出状态，然后退出）。Codex 可以独立完成“修改代码 → 构建 → 运行脚本 → 读取截图验证”的完整闭环，无需主人手动提供游戏截图。

- **验证矩阵按改动面分流：** 新增或修改植物、僵尸、粒子、出怪池、数值、逻辑或普通资源时，默认只跑 `clang-release` 的当前桌面可见用例与实际影响范围内的状态/资源/截图断言，不因“是新内容”就机械加跑 `-NoInstance` 或强制 OpenGL。只有实际改动渲染后端、后端兼容路径或跨后端提交实现（如 Vulkan instance/batch、`-NoInstance` CPU 矩阵路径、OpenGL CPU batch/shader/texture 生命周期）时，才要在默认 Vulkan 之外加跑 `-NoInstance` 和强制 OpenGL 兼容回归。

- **资格拒绝断言：** `set_typhoon` 可传 `expectedSuccess=false`，用正式设置入口验证当前地图或天气拒绝台风；缺省仍要求设置成功，保持旧脚本语义。

- **脚本位置：** `autotest/scripts/*.json`（纯数据，不属于编译目标；修改脚本无需重新编译）。
- **运行方式（工作目录必须是 exe 所在的 `build\<preset>\`）：** Codex 默认必须让窗口显示在主人当前桌面。GUI 启动属于沙箱外桌面操作，调用 shell 时使用 `sandbox_permissions="require_escalated"`；仅写 `-WindowStyle Normal` 而不提升权限，进程仍可能落入隔离会话、主人看不到。推荐命令：

  ```powershell
  Push-Location build\clang-release   # 默认构建、迭代诊断与交付回归
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

- **产物：** 位于 `build\<preset>\autotest\out\<script-name>\`，包括 PNG、`status.json`、状态转储和 `run.log`。Release 会裁掉 Logger INFO，因此 `run.log` 是权威命令执行记录；首帧会记录 requested/selected Renderer、`-NoInstance`、SDL video driver，并按实际后端记录 Vulkan API/dynamic rendering/sync 路径，或 OpenGL Vendor/Renderer/Version/GLSL/framebuffer/VSync 及进程中是否已有 Vulkan loader。auto 回退另记录失败阶段和原始错误，防止测试只传参数却未命中目标分支。
- **离机归档：** `scripts/upload_autotest_artifacts.ps1 -TestName <script-name>` 可把单次输出打包上传到已配置的 SSH 备份节点；默认仍是 `clang-release`，其他预设必须显式传 `-Preset`。工具要求 `run.log` 与 `status.json`，记录提交、工作区脏状态、测试状态和逐文件 SHA-256；失败用例也允许归档，没有 PNG 时只警告。离机副本不改变当前桌面可见运行、退出码、日志、状态断言和截图均须现场检查的验收合同。部署与恢复见 `docs/operations/pvz-backup-node.md`。
- **西瓜投手夹具：** `set_melonpult_shoot_cycle` 按 `row/col` 固定当前活动西瓜家族植物的已累计时间与本轮间隔；紫卡升级同帧内会过滤已失活但尚未移除的基础株。`spawn_bullet` 名称表开放 `BULLET_MELON` 与 `BULLET_WINTERMELON`，可与抛物线参数组合覆盖溅射、落空、减速和对象池复用。
- **投手锁墙夹具：** `spawn_bullet` 的解析抛物线参数可加 `targetsIceWall=true`，显式锁定同行冰墙；状态 bullet 条目导出同名布尔值，供断言在途弹跳过墙后僵尸、落点复核墙体、快照往返与对象池复位。未给该字段时保持普通抛射语义。
- **BulletPool 压力夹具：** `spawn_bullet` 可用 `count=1..512` 批量创建同型弹丸，并用 `xStep/yStep` 给每发位置递增；缺省仍只创建一发。状态根节点导出 `bulletPoolStorageCount/ActiveCount/PeakCount/HitCount/MissCount/HitRateOn1000/ActiveSlotsValid`，其中 hit 只表示复用空闲对象，miss 表示必须新建。`stress_bullet_pool_active_slots.json` 以 256 发新建→全部回收→64 发复用锁定稠密活跃表、统计和阴影表现；性能取证加 `-Profile` 并读取 `5a.Draw_bulletShadows`，不能只凭结构变化声称帧率提升。
- **忧郁菇夹具：** `set_gloomshroom_shoot_cycle` 按 `row/col` 把已累计攻击周期固定为 `elapsed` 秒并清理未完成攻击；状态投影导出攻击内时间及下一云雾/伤害序号，供四段原版时间点和中途读档续播做确定性断言。
- **命令集：** `goto_level` / `choose_cards` / `wait_state` / `set_sun` / `set_weather` / `set_opening_typhoon_protection` / `set_roof_runoff` / `set_typhoon` / `roll_typhoon` / `reroll_typhoon_direction` / `trigger_typhoon_gust` / `set_weather_forecast` / `show_image_prompt` / `show_crazy_dave_dialog` / `advance_crazy_dave_dialog` / `skip_crazy_dave_dialog` / `roll_weather_forecast` / `advance_weather_phase` / `trigger_lightning` / `set_adventure_level` / `force_trophy` / `add_crater` / `force_survival_round` / `force_survival_round_clear` / `summon_next_wave` / `plant` / `assert_can_plant` / `set_plantern_gear` / `set_plantern_fuel` / `award_plantern_fuel` / `toggle_plantern_menu` / `assert_can_target` / `spawn_bullet` / `set_starfruit_shoot_cycle` / `set_cabbagepult_shoot_cycle` / `set_kernelpult_shoot_cycle` / `set_furnace_core_state` / `spawn_zombie` / `attempt_ice_execution` / `apply_zombie_control` / `make_gargantuar_smash_ready` / `set_jack_pop_countdown` / `set_elite_jack_throw_countdown` / `spawn_wave_zombie` / `set_zombie_mist_fuel_reward` / `kill_zombie` / `damage_plant` / `squish_plant` / `damage_zombie` / `add_perk` / `survival_perk_open` / `survival_perk_pick` / `survival_perk_refresh` / `show_plant_hp` / `show_zombie_hp` / `wait_seconds` / `wait_frames` / `set_timescale` / `reset_test_state` / `set_last_selected_cards` / `save_level_snapshot` / `reload_level_snapshot` / `charm_zombie` / `move_mouse` / `click` / `key` / `screenshot` / `dump_state` / `assert_state` / `quit`。等待类命令接受 `timeout`（默认 15 秒）。`set_opening_typhoon_protection` 只在进程内切换默认开启的前 5 波台风保护，不触碰真实 `PlayerInfo.json`；专项用它覆盖高难度玩家关闭保护后的原概率路径。`set_last_selected_cards` 只在进程内布置稳定植物枚举名数组，不触碰真实 `PlayerInfo.json`，供选卡恢复按钮和失效名称过滤专项使用。`plant` 对 `PLANT_BLOVER` 可选 `bloverDirection=HOUSE/FRONT`，用于固定实例方向；`assert_can_plant` 用 `type/row/col/expected` 直接断言正式 `Board::CanPlantAt`，适合覆盖睡莲承载层、水路禁种与弹坑等网格规则。`add_crater` 用 `row/col` 在当前棋盘直接创建弹坑，可选 `timeLeft` 固定剩余秒数，专用于验证不同格子地形和寿命阶段的绘制资源。`set_plantern_gear`、`set_plantern_fuel`、`award_plantern_fuel` 与 `toggle_plantern_menu` 固定路灯花玩法/UI 状态；`assert_can_target` 直接断言统一雾中索敌许可；`set_zombie_mist_fuel_reward` + `kill_zombie` 用确定性奖励走正式死亡发起入口，先断言 `pendingFuelTenths`、再等待飞行结束断言实际到账，避免用概率用例验证到账/丢弃边界。`set_roof_runoff` 对昼夜屋顶生效，用 `phase=IDLE/WARNING/FLOWING`、`charge`、活动阶段非空 `rows` 数组和可选 `remaining/retainedCharge` 固定径流状态；旧脚本的单个 `row` 仍兼容。`set_weather_forecast` 固定公开预报、真实天气和揭晓倒计时，只用于天气 UI/失败提示的确定性测试；当 `actual=HEAVY` 时可用 `typhoonStrength=NONE/TYPHOON/SEVERE/SUPER` 与 `promptVariant=0..2` 固定待生效台风和同级预警文案。`show_image_prompt` 用 `image=HUGE_WAVE/FINAL_WAVE` 显示既有图片提示，供多提示并存与绘制顺序测试。`show_crazy_dave_dialog` 按当前关卡显式打开戴夫闲聊（`force` 默认 true），`advance_crazy_dave_dialog` 与 `skip_crazy_dave_dialog` 分别推进或完成对话；状态根节点 `crazyDave` 导出触发、页码、台词、轨道和最终几何，`crazyDaveResources` 锁定 reanim 与全部部件资源。`roll_weather_forecast` 只在晴天用 1-based `weatherRoll` 走正式动态权重与弱天气保底，再发布必定准确的锁定预报，可用 `revealIn` 固定揭晓倒计时。`set_typhoon` 只在大雨中生效，用 `strength=NONE/TYPHOON/SEVERE/SUPER`、`direction=NONE/HOUSE/FRONT` 固定台风状态；可选 `gustIn`、`directionIn`、`gustsRemaining` 和 `decayIn` 固定阵风、转向、预算与衰减计时，`roll_typhoon` 用 1-based `chanceRoll`/`strengthRoll` 和固定方向走正式概率、连续落空保底与动态强度边界。`reroll_typhoon_direction` 用 `directionRoll=1..2` 走正式风向二选一重抽，确定性覆盖继续同向与切换方向。`trigger_typhoon_gust` 启动一次不消费自动预算的正式阵风，可用 `plantMoveIn` 固定阵风开始后多少游戏秒结算植物（默认 0 保持旧脚本的立即结算），活动期间仍会连续吹动僵尸。`force_survival_round` 直接定位测试轮次、重建出怪池并刷新轮次派生的天气速度；`force_survival_round_clear` 走正式轮清入口。`summon_next_wave` 直接走正式 `Board::SummonNextWave()`，可用 `count=1..100` 连续推进并验证波次派生状态；`spawn_zombie` 可加 `frozen=true` 让新目标立即走正式冻结入口；`set_jack_pop_countdown` 按 `row/index/value` 只覆盖 RUNNING 普通小丑的剩余开盒秒数；`set_elite_jack_throw_countdown` 按 `row/index/value` 选择精英小丑，可用 `targetRow/targetColumn` 固定下一只盒子的地图合法落点，供飞行、边界行、伤害与存档做确定性验证；`spawn_wave_zombie` 额外要求 `mutationRoll=1..100`，以正式天气变异解析器创建波次候选，用于确定性测试条件变异和每波上限；候选超过上限时命令成功但不创建回退类型，与正式挑选循环的 `continue` 一致。`spawn_bullet` 直接创建对象池子弹，可固定 `velocityX/velocityY/damage` 以及投掷物的 `lobTargetX/lobTargetY/lobDuration/lobApexHeight`，用于断言风力、伤害、解析抛物线与对象池复位；名称表同时开放豌豆系、孢子、尖刺、星弹、卷心菜、玉米粒和黄油。`set_starfruit_shoot_cycle`、`set_cabbagepult_shoot_cycle` 与 `set_kernelpult_shoot_cycle` 都按 `row/col` 固定植物已累计时间与本轮间隔，只布置正式射击周期，不直接触发动画或发弹；玉米投手命令另可用 `butter=true/false` 固定下一发。`damage_plant` 按 `row/col/index`、`damage_zombie` 按 `row/index` 选目标并走正式 `TakeDamage` 链；两者的 `source` 可取 `PLANT/ZOMBIE/OTHER`（默认 `OTHER`），后者另可选 `penetrateShield`，用于来源词条、护盾、断肢和死亡动画测试。`squish_plant` 按 `row/col/index` 调用植物基类正式压扁入口，供绕过巨人/冰车/投篮车攻击时序独立验证植物侧表现。`show_plant_hp` 与 `show_zombie_hp` 用可选 `on` 布置同层血量文字，供截图验证组合实体布局。`set_adventure_level` 与 `force_trophy` 仅用于冒险进度结算测试；`survival_perk_refresh` 消耗本轮共享的一次刷新额度并重抽当前全部词条候选。植物/僵尸类型直接使用枚举标识符（例如 `PLANT_PEASHOOTER`、`ZOMBIE_FASTPAPER`），新增类型需要在 `Game/AutoTest/TestDriver.cpp` 的名称表中添加一行。
- **炉芯花与冰封阻断夹具：** `set_furnace_core_state` 按 `row/col` 设置炉芯花的 `cores=0..2` 与 `progress=0..10`，只布置权威资源状态且不重播声光；`attempt_ice_execution` 按稳定 ID 选择指定行/序号的处刑者与目标格植物并调用正式封存入口。专项必须断言成功保护时目标从未进入冰封、来源能力进入 `SPENT`，并覆盖重叠提供者顺序、资源耗尽回退、自身排除和存档。
- **冰裂钻机夹具：** `set_ice_crack_drill_state` 按 `row/index` 布置 `MOVING/CHARGING/SPENT`、剩余蓄力秒数与能力消费态；`interrupt_zombie_special_action` 走品种通用的未提交动作中断入口；`apply_winter_corrosion_to_zombie` 只对目标仍存在的冰制装备层施加独立腐蚀，并可用 `expectedChanged` 断言是否实际结算。三者均不直接生成地裂或伪造音效，专项应继续从正式更新边沿验证提交。
- **天气预报夹具补充：** 上述命令集中的 `actual=HEAVY` 限制是旧口径；当前只要公开预报或真实下一天气涉及 `HEAVY`，`set_weather_forecast` 就可用 `typhoonStrength=NONE/TYPHOON/SEVERE/SUPER` 与 `promptVariant=0..2` 固定公开警报/新大雨待生效等级。当前已是大雨且预报同档续期时，`typhoonStrength` 只固定公开预报，当前 Board 台风是揭晓实况；两者不同也会激活失败提示。`weather.forecastDisplayText`、`weather.failedForecastTyphoonStrength/actualForecastTyphoonStrength` 导出公开文案及失败卡片两边的台风等级，`weather.panelHeight` 与 `weather.nightRoofCharge.executionLineVisible` 用于锁定动态详情行。
- **完整选卡夹具：** `set_all_owned_cards` 只在进程内按正式冒险奖励顺序布置当前全部已实装卡，供完整选卡面板专项使用，不改冒险进度或真实 `PlayerInfo.json`。选卡状态投影导出当前页、总页数、实际活动/隐藏植物列表及分页按钮的资源、角度和相对锚点；`click target=choose_card_page` 在执行时解析当前分页按钮中心并走真实输入路径。
- **巨人锤击测试夹具：** `make_gargantuar_smash_ready` 按 `row/index` 选择处于 `SMASHING` 且尚未结算命中的巨人，把正式 `anim_smash` 推进到既有第 93 帧事件前；后续等待逻辑帧仍走目标快照、植物分层反应和命中音画的正式路径。
- **巨人投掷测试夹具：** `make_gargantuar_throw_ready` 按 `row/index` 选择处于 `THROWING` 且尚未脱手的巨人，把正式 `anim_throw` 推进到既有第 131 帧事件前；后续等待逻辑帧仍走生成、阵营继承、视觉锚点对齐和音画的正式路径。
- **急救员测试夹具：** `set_difficulty` 用 `value=1..4` 设置当前进程测试难度；`make_healer_ready` 只把活动急救员的冷却与重试归零，仍走正式选疗、前摇和结算，传 `all=true` 时在同一命令边沿同步放开全部匹配行的急救员，专用于动作边沿性能压力测试。`damage_zombie` 可选 `type` 先筛僵尸品种，再按稳定实体 ID 应用 `index`，适合同场多种防具的确定性修复验证。
- **植物伤害测试参数：** `damage_plant` 按 `row/col` 选择目标，可选 `type` 先筛植物品种，再用 `index` 打破并列；用于同格多层植物的确定性外伤验证。
- **麻痹测试参数：** `spawn_zombie` 可用 `paralyzedFor` 设置通过正式入口施加的麻痹秒数；用于验证中立控制与品种状态机的并行计时，非法时长或免疫目标会让命令失败。
- **植物生命夹具：** `set_plant_health` 按 `row/col` 选取该格顶层植物，并将 `value` 设置为不超过最大生命的当前生命值，只用于稳定覆盖指定反噬致死边界。
- **黄油状态夹具：** `butter_zombie` 按 `row/index` 调用目标的正式 `ApplyButter()` 虚入口，并用 `expectedApplied` 断言施加或免疫结果，供品种抗连控与存档边界做确定性验证。
- **通用僵尸控制夹具：** `apply_zombie_control` 按稳定 ID 顺序用可选 `row/index` 选择目标，`effect=SLOW/FROZEN/BUTTER/PARALYSIS`，`duration` 控制减速/麻痹时长，`expectedApplied` 断言正式入口是否接受；状态投影导出四类临时免疫余时、合并永久免疫后的 `controlImmunityMask` 和三类硬控状态。冻结返回 false 仍可能按正式寒冰菇语义造成 20 点伤害并保留未免疫的减速尾巴，脚本须分开断言伤害与控制。
- **黑夜屋顶雷荷命令：** `set_night_roof_charge` 仅对 `NIGHT_ROOF` 生效，用 `phase=CHARGING/WARNING/DISCHARGING`、`charge`、活动阶段的 `row` 和可选 `remaining/overcharge` 固定雷荷状态；只有实际跨越 `WARNING -> DISCHARGING` 才会结算一次实体效果，直接恢复/设置 `DISCHARGING` 不重复命中。`overcharge` 只在活动阶段生效，封顶15并在放电结束兑现为下一轮主电荷。自然满电时普通行与有效引雷实体统一选路；`dump_state.weather.nightRoofCharge` 导出 `guided/guideID/guideCandidateCount`、路线是否使用蒙特卡洛、耗时微秒、rollout/候选/样本数、最佳分和既有积累/阶段字段；植物另导出 `shutdown/shutdownTimerMs`，僵尸导出 `paralyzed/paralysisTimerMs/canBeParalyzed/groundHazardEligible`，异品种测试靶可从 `zombiesByType.<枚举名>` 稳定读取。
- **地图覆盖夹具：** `goto_level` 可选 `background=GROUND_DAY/GROUND_NIGHT/WATER_POOL/NIGHT_WATER_POOL/ROOF/NIGHT_ROOF`，只在 AutoTest 场景创建时覆盖正式关卡背景；字段缺省时始终使用 `GameAPP::GetBackgroundID`。当前 5-9 正式使用白天 `ROOF`，6-1～6-9（内部 46～54）正式使用 `NIGHT_ROOF`；显式背景覆盖只用于隔离测试，禁止接入正常玩家入口。
- **扶梯测试夹具：** `add_ladder` 用 `row/col` 经正式 `Board::AddLadder` 在当前棋盘创建唯一扶梯，供攀爬、植物死亡、磁吸、台风换格与存档边界做确定性验证；`set_elite_ladder_scan_countdown` 用 `row/index/value=0..5` 只缩短目标精英扶梯的一次性行扫描倒计时，不直接结算能力；`ladders[]` 同时导出样式/贴图键、宿主存在性、二维换格视觉偏移及附件—宿主偏移误差的千分整数投影。
- **同步截图与测试状态：** `screenshot` 是等待型屏障：渲染器只有在 `IMG_SavePNG` 成功后才发布 ticket 成功，TestDriver 随后校验 PNG 存在且非空并写 `done`；脚本无需在截图后人为等待即可直接切场景。`reset_test_state` 显式恢复 `timeScale=1`、`devNoCooldown=false`、`devFreePlant=false`、`devSpawnPaused=false`、`monteCarloAIEnabled=true`、`advancedPauseEnabled=false`；`goto_level` 可传 `"resetTestState":true` 在切场景前复用同一复位函数，字段缺省时保持旧语义。状态 JSON 用 `timeScaleOn1000` 做倍速整数断言。
- **轻量蒙特卡洛 AI 测试：** `set_monte_carlo_ai` 用 `value=true/false` 切换 `GameAPP::mEnableMonteCarloAI`；`reset_test_state` 和带复位的 `goto_level` 都恢复为默认开启。状态 JSON 的 `monteCarloAIEnabled` 锁定总开关；蹦极与精英小丑导出各自的 targeting mode 和 rollout/候选/僵尸/卡槽计数，蹦极另导出详细植物数与压缩支撑数，急救员导出 `healerDecisionMode`、`healerDecisionAction`、`healerStrategicWaitMs` 及同组推演统计，黑夜屋顶满雷路线另导出 `route*` 统计。四类决策共用每次 rollout 最多 16 只僵尸的硬上限；详细植物上限 128，普通花盆/睡莲由 `simulation.supportOnly` 进入独立 64 格支撑数组。`smoke_zombie_monte_carlo_cap.json` 用 16 只实体锁定运行时入口，`smoke_monte_carlo_support_compression.json` 锁定支撑层不占详细容量且避雷花盆仍保留完整画像，纯数值 `plant-defense-monte-carlo` 测试锁定延迟候选、劫持者处决效用和同格阻挡层级。`stress_healer_monte_carlo.json` 用 `make_healer_ready all=true` 同步放开 10 名急救员并等待 ID 顺序的三固定逻辑步间隔预算全部完成；性能取证需加 `-Profile`，读取 `MC.Healer.Snapshot/Rollouts/Total` 的调用次数、单次平均/最大耗时，并结合 `GOM_Update` 与 `SceneUpdate_total` 最大值判断动作边沿尖峰，不能只看 60 帧平均 FPS。关闭开关后调用者必须保留原随机或确定性回退。`MainMenuScene` 也允许 `assert_state` / `dump_state` 读取这些 GameAPP 级根字段，供主菜单控制台走真实 CheckBox 点击路径验证。
- **高级暂停测试：** `set_advanced_pause` 用 `value=true/false` 切换 `GameAPP::mAdvancedPauseEnabled`；默认及 `reset_test_state` 均为关闭。状态 JSON 的 `advancedPauseEnabled` 锁定设置，`pauseGameplayInputBlocked` 锁定当前普通空格暂停是否屏蔽卡槽/落种。控制台测试必须走主菜单真实 CheckBox；Esc 完整菜单不承载此设置。
- **蹦极僵尸测试命令：** `set_bungee_altitude` 按 `row/index/value` 强制目标蹦极进入当前下降/上升阶段的指定离地高度；`set_bungee_bottom_countdown` 按 `row/index/value` 固定落地等待秒数，用于不新增动画帧事件地确定性覆盖落地、抓取、快速上升与离场。
- **迷雾测试命令：** `set_fog_weather` 用 `intensity=DEFAULT/SMALL/NORMAL/DENSE` 与 `duration` 固定当前合格迷雾关卡的基础雾、小雾、普通迷雾或大雾；`set_fog_forecast` 用同一组四档 `forecast/actual` 和 `revealIn` 固定独立雾势预报；`set_fog_dispersal` 用 `value=0..1` 固定台风驱散进度。`dump_state.fog` 导出档位、渲染层数、基础/有效左边界、逐列最大 alpha、可见雾格数、驱散百分比、有符号风向位移、预报、贴图分片加载数与动态大雾概率。通用雾场仍由 `NIGHT_WATER_POOL` 背景提供；固定复用完整迷雾的冒险关集中登记在 `AdventureProgression::HasLevelSpecificFogMechanics()`，当前仅 6-9 的 `NIGHT_ROOF`。迷雾脚本必须同时覆盖非资格关卡 no-op、6-8/6-9 边界、四档覆盖距离与层数、双向风偏移、完全吹散、停风回流、预报和关卡快照往返。
- **最终绘制坐标取证：** AutoTest 模式会采集 Animator 默认实例化与 `-NoInstance` 慢路径实际提交的世界四边形；所有 `AnimatedObject`（植物、僵尸、动画子弹与动画特效）按 tag 导出到 `animatedObjectsByTag`，包含 `renderProbeReady`、`renderPath`、`worldBounds`、相对视觉原点投影以及最近植物/僵尸 collider 关系。粒子按效果名导出到 `particleEffectsByName`，包含裁剪前实际粒子包围盒、相对发射原点投影、`clipRightXInt` 与最近实体关系。新增内容只把 C# 800×600 坐标当行为语义参考；稳定断言使用当前项目的格子/collider/最终几何相对量整数投影，并配合同步截图。修改 Animator 世界变换时还须让默认与 `-NoInstance` 同一静止用例的整数 `worldBounds` 一致。
- **僵尸分层受击观测：** `zombies.N.hitFlashMask` 与 `renderedHitGlowMask` 均以 bit0 表示本体/头盔/飞行额外生命、bit1 表示二类护盾；前者证明伤害层计时器，后者证明 Animator 实际轨道高亮。普通正面子弹命中持盾目标应为 `2`，大喷穿透同时伤盾与后层应为 `3`，等待白光结束后回到 `0`。
- **隔离关卡快照：** `save_level_snapshot` / `reload_level_snapshot` 的 `name` 只允许 ASCII 字母、数字、`_`、`-`，文件固定在当前脚本的 `autotest/out/<script>/snapshots/<name>.json`。保存复用正式序列化；重载先销毁旧 `GameScene`，再让同关卡的新场景在正常加载阶段用正式反序列化读取一次性路径。bullet 状态额外导出只读 `fromPool` / `poolType`，用于确认动画变种读档后仍归属原对象池槽位。
- **长时序隔离：** `set_spawn_paused` 以 `value=true/false` 暂停或恢复自然出波，不影响 `spawn_zombie`、`summon_next_wave` 等显式命令，适合成长、恢复和长计时测试；脚本离开隔离段前应显式恢复 `false`。
- **静止测试靶：** `spawn_zombie` 可加 `stationary=true` 把该实例的基础 Animator 速度设为 0，从而停止 `_ground` 位移；它不伪造冻结/减速状态，适合长时间射击成长与承伤验证。
- **合成输入（所有场景共用真实 click/key 路径）：** 现有 `plant`、`spawn_zombie` 等操作直接调用游戏逻辑，只覆盖 GameScene。驱动图鉴等非 GameScene UI 时使用 `click` / `key`；它们通过 `SDL_PushEvent` 注入合成事件，走与真实输入相同的路径（在下一帧 poll 时消费，并使用同一套 letterbox 坐标逆变换）。正常游戏没有运行时开销，因为非 AutoTest 模式下 `TestDriver::Update` 第一行就会返回。
  - `move_mouse`：`{ "op":"move_mouse", "x":440, "y":298 }`，只注入鼠标移动事件而不按键，适合截取悬停预览和 hover 状态。
  - `click`：`{ "op":"click", "x":570, "y":490 }`，可选 `"button"`（`left`，默认 / `right` / `middle`）和 `"hold_frames"`（默认 1，即按下到释放之间保持的帧数）。`x,y` 是**逻辑坐标**，与 UI 布局和 `dump_state` 的 x/y 使用同一坐标系；`target=trophy`、`target=restore_last_cards`、`target=choose_card_page`、`target=zombie_almanac_previous_page` 与 `target=zombie_almanac_next_page` 可分别在执行时解析奖杯、选卡按钮或僵尸图鉴翻页按钮中心。关卡选择页另提供 `target=game_select_previous_page`、`target=game_select_next_page`，以及带内部关卡号 `level` 的 `target=game_select_level`；后者只允许点击当前页实际创建且可用的入口，未解锁或其他页关卡会令脚本失败。冒险入口默认定位当前可挑战关所在页；生存入口只有在对应大关全部通关后才创建。状态投影的 `gameSelectSurvivalUnlockAreas` 锁定集中定义表中的解锁大关顺序。一次 click 会跨帧完成（按下沿 → 保持 → 释放沿）；若下一条命令要观察释放回调产生的状态，至少再等待两个 `wait_frames` 计数，让释放事件经过一个真实输入处理帧。
  - `assert_state`：`{ "op":"assert_state", "path":"perks.stacks.PLANT_DAMAGE_UP", "equals":3 }`，断言对象与 `dump_state` 写出的状态 JSON 相同。`path` 使用点分段；纯数字段用于索引数组（如 `zombies.0.type`）。数值字段也可用 `atLeast` / `atMost`（可同时给出）做闭区间断言。不匹配或路径缺失会失败并返回 exit 1。不要对浮点字段（如 `zombieHealthMult`）使用 `equals`，因为它执行 JSON 精确比较；优先断言整数投影字段（如 `plantDamageOn100`）。
    `ZombieAlmanacScene` 也支持这两条命令，导出 `scene`、`adventureLevel`、`encounteredEliteDancer`、完整的 `zombieAlmanacEntries` / `zombieAlmanacEntryCount`、当前页 `zombieAlmanacPageIndex` / `Number` / `Count`、`zombieAlmanacVisibleEntries` / `EntryCount`、前后翻页按钮状态，以及 `zombieAlmanacSelected` 和右侧详情 `zombieAlmanacPreview`。这些字段用于断言图鉴随冒险进度解锁、当前关不提前泄露、分页和详情动画状态；`encounteredEliteDancer` 是 PlayerInfo 的永久遭遇标记，但 AutoTest 只验证同一进程内的内存状态，不会读写真实玩家存档。
  - `key`：`{ "op":"key", "name":"space" }`，可选 `"action"`（`press`，默认，完整点击 / `down`，仅按下沿 / `up`，仅释放沿）。`name` 是键名字符串：`a`–`z`、`0`–`9`、`space` / `enter` / `escape` / `tab` / `backspace`、方向键 `up` / `down` / `left` / `right`、`f1`–`f12` 等；新增键名需要在 `TestDriver.cpp` 的 `kKeyNames` 中添加一行。
- **隔离性：** AutoTest 模式默认短路所有玩家存档读写（不读取或写入 `saves/`）；每次进入关卡都是确定性的全新关卡；`-Seed N` 固定随机种子。只有必须复现真实关卡存档问题时才显式加 `-AutoTestLoadSave`：此参数允许 `LoadLevelData` 从当前构建目录的 `./saves/` 读取关卡存档，但玩家设置仍用 AutoTest 默认值，且保存和删除入口继续短路，因此是严格只读模式。脚本快照是唯一显式写入例外，只能写当前脚本输出目录；禁止临时关闭 `GameAPP::mAutoTestMode` 绕过保护，否则可能触发真实存档写入、删除或迁移。
- **植物状态观测：** `dump_state` 根节点提供 `plantCount`、`bulletCount`、`repeatingShootingHeadCount`；Shooter 植物条目另含 `headTrack`、`headAnimPlaying`、`headAnimPlayState`，可配合 `assert_state` 验证附加头部 Animator。`scaredyShroomsByCell.<row_col>.fearState` 按格稳定导出胆小菇四态，不受同格南瓜成为 `topPlantsByCell` 的影响。
- **示例：** `autotest/scripts/demo_peashooter.json`（验收脚本）以及各子系统的 `smoke_*.json`。

- **冬季地面冲击夹具：** `resolve_winter_ground_impact` 按 `row/col` 选择当前战斗顶层植物，以 `kind=COLLISION/GROUND_CRACK` 调用植物通用冬季冲击语义；`expectedIntercepted`、`expectedContainsScatter` 与 `expectedDownstreamMultiplierOn1000` 直接断言原子响应。该命令只替代尚未实现威胁的动作提交，不施加伤害或伪造雪橇落点。
- **寒潮预报依赖夹具：** `disrupt_cold_wave_forecast` 只调用 Board 正式干扰入口；`set_melt_snow_pult_shoot_cycle` 与 `set_melt_snow_pult_salt_state` 分别固定融雪投手本次出手和库存/蓄力状态。`meltSnowPultsByCell` 导出库存、蓄力与观测预报状态，bullet 条目的 `winterCorrosionDamage` 只投影盐晶携带的独立目标层腐蚀值；脚本仍须断言普通目标只承受基础 20 点本体伤害，并覆盖干扰前已离手盐晶的存档往返。

## 架构概览

### 对象层次

```text
GameObject（基类：显式可选 Transform/Collider/Shadow/Clickable、渲染顺序、激活状态）
└── AnimatedObject（增加 Animator 精灵动画）
    ├── Plant → Shooter → PeaShooter
    │          SunFlower、WallNut、CherryBomb……
    ├── Zombie → ConeZombie、Polevaulter……
    └── Coin（可收集物）
Bullet（独立类型；通过 BulletPool 使用对象池）
```

`Bullet` 是单一 `final` 具体类型，弹型差异由稳定的 `BulletType`、分型对象池槽位和窄行为函数表达。豌豆、寒冰豌豆、孢子、火球与尖刺不得仅为名称建立无覆写、无独立状态的空派生类；火炬树桩运行时换型仍须区分当前 `mBulletType` 与固定 `mPoolType`，并由 `Reset` 完整恢复槽位状态。`BulletPool` 与 GOM 共同以 `shared_ptr` 持有池对象；`Bullet` 不反向持有池，不构成循环。每颗弹丸只保存一个不入档、复用时不重置的稳定池槽位下标，Release 以边界和指针一致性校验后直接定位，禁止恢复逐弹丸指针哈希或在逐帧阴影路径做 `weak_ptr::lock()`。池另维护稠密活跃槽位下标：Acquire 记录槽位在活跃表的位置，Release 用末项交换 O(1) 移除并拒绝重复回收，活跃计数直接由该表派生；hit 只统计空闲对象复用，miss 只统计新建。GOM 仍保留全部池对象以维持排序与附件生命周期，但 Update/Draw 是否进入并行路径只按总候选数扣除休眠池弹丸后的数量判断，不能让历史池高水位把小场景误判为大场景。若未来弹型出现无法由同一生命周期和复位合同安全表达的独立行为，再以实质覆写和专属状态为依据重新评估继承，而不是预先建立标记子类。

### 架构决策：继承式玩法对象

自 2026-08-16 起，本项目正式采用**继承式玩法对象**作为植物、僵尸及其他有独立生命周期玩法实体的主模型：公共状态与流程收敛在稳定基类，品种差异通过派生类、窄虚接口和注册式工厂表达。不得仅为追求形式统一，把植物或僵尸能力拆成通用 ECS 组件、复制一套平行状态，或让组件组合取代现有 `Plant` / `Zombie` 生命周期、动画与存档契约。

早期通用 `Component` 容器已于 2026-08-22 完整删除，不是项目未来的玩法对象模型。新增代码不得恢复 `Component` 基类、按 `type_index` 索引的类型表、`Add/Get/RemoveComponent<T>` 服务定位或通用 Start/Update/Draw 生命周期；跨多个无继承关系宿主复用且确实可选的横切能力，应由宿主用命名明确的值或小对象显式拥有。最终迁移契约见 `docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md` 与 `docs/superpowers/plans/2026-08-16-component-system-contraction.md`。

`EntityRegistry` 与上述组件容器相互独立：它是 Board 范围内的稳定实体 ID 注册表和查询索引，服务跨对象引用、存档恢复与热路径检索，不是 ECS，组件收缩期间不得删除或把其职责重新塞回对象指针。

### 显式附件

`GameObject` 通过 `std::optional<Transform>` 直接保存非多态空间值；只有空间对象才调用 `CreateTransform()`，调用方用 `GetTransform()` 读取唯一权威的位置、旋转和缩放。Collider 也已脱离通用组件表，由宿主用 `unique_ptr` 可选独占；只能通过 `CreateCollider()` / `GetCollider()` / `RemoveCollider()` 创建、访问或移除，入口原子维护 owner、CollisionSystem 注册、ID 与缓存，场景销毁和运行时移除都不得直接重置字段。`ColliderComponent` 仅保留过渡名称，不再继承 `Component`，Debug 绘制由 `GameObject::Draw()` 显式提交。

Shadow 同样由 `GameObject` 用 `unique_ptr<ShadowComponent>` 显式可选独占；`ShadowComponent` 仅保留过渡名称、不再继承 `Component`。创建、访问和移除统一走 `CreateShadow()` / `GetShadow()` / `RemoveShadow()`；介质/出土等生命周期显隐用 `SetVisible()`，跳跃/投掷等动作阶段门控用 `SetEnabled()`，两者独立并取 AND，禁止互相覆盖。普通对象由 `GameObject::Draw()` 在本体前固定提交；Bullet 不调用该阶段，仍由 `BulletPool::DrawShadows()` 在植物层前跨对象提交，并只遍历稠密活跃槽位，不得让历史池高水位放大阴影阶段扫描量。默认实例路径必须使用 `DrawTextureInstanced()` 与 reanim 保序，`-NoInstance` 继续走普通批次兜底。

Clickable 也由 `GameObject` 用 `unique_ptr<ClickableComponent>` 显式可选独占；创建、访问和移除统一走 `CreateClickable()` / `GetClickable()` / `RemoveClickable()`。`CreateClickable()` 会先保证 Collider 完整就绪再注册，`RemoveCollider()` 会同步注销 Clickable；仅替换 Collider 时保持 Clickable 注册有效。Clickable 继续使用主线程稀疏自注册表，输入处理保持渲染顺序降序、`ConsumeEvent`、悬停光标计数以及 UI/世界坐标选择，禁止退回每帧扫描全部 GameObject。

当前运行源码已不存在通用 `Component` 基类、派生类、类型表、待初始化/更新/绘制视图或模板访问接口。`ColliderComponent`、`ShadowComponent`、`ClickableComponent` 的 `Component` 后缀仅是兼容性的过渡命名：它们是 `GameObject` 通过具名 API 显式独占的附件，不构成组件系统。不得把 Transform、Collider、Shadow、Clickable 或新玩法状态重新抽回通用容器。

`Card` 已直接拥有单卡的冷却、选中、三叶草方向、可用性和显示缓存，并在 `Card::Start/Update/Draw` 中显式管理点击回调、玩法更新与卡面绘制。不要重新引入 `CardComponent` / `CardDisplayComponent`，也不要通过组件容器查询单卡状态。场景级多卡仲裁由 `GameScene` 通过 `unique_ptr<CardSlotManager>` 明确拥有；实战 `Card` 由该控制器直接绑定非拥有指针，禁止恢复匿名 `CardUI` 宿主或每帧扫描组件表定位 manager。

### 关键系统类

| 类 | 文件 | 职责 |
|---|---|---|
| `Board` | `Game/Board.cpp` | 关卡玩法权威：天气、僵尸波次、阳光生成、胜负逻辑 |
| `BoardPresentation` | `Game/BoardPresentation.h` | `Board` 到宿主场景的窄展示端口：提示、进度条及 UI 瞬态存取 |
| `GameObjectManager` | `Game/GameObjectManager` | 创建/销毁对象、渲染顺序、线程池 |
| `CollisionSystem` | `Game/CollisionSystem` | 每帧碰撞检测与回调 |
| `EntityRegistry` | `Game/EntityRegistry` | 按 ID 跟踪实体（存档系统使用） |
| `SceneManager` | `Game/SceneManager` | 持有唯一活动场景并在各场景间切换；场景在 `GameApp.cpp` 注册 |
| `ResourceManager` | `ResourceManager` | 加载/缓存资源；资源键定义在 `ResourceKeys.h` |
| `Graphics` | `Graphics.cpp` | 游戏公共绘制入口；Vulkan 保留 bindless/InstanceRecord/worker 快路，OpenGL 3.3 使用 CPU 展开和动态 VBO/IBO Batch；资源通过后端无关 `RenderTexture`/`TextureBackend` 生命周期接入 |
| `Animator` | `Reanimation/Animator` | 命名轨道动画系统；提供 `PlayTrack()`、`PlayTrackOnce()` 和帧事件 |
| `ParticleSystem` | `ParticleSystem/` | 由 `resources/particles/` 下 XML 配置驱动的粒子系统 |
| `AudioSystem` | `Game/AudioSystem.h` | SDL2_mixer 音效与音乐封装，管理声道 |
| `InputHandler` | `UI/InputHandler.{h,cpp}` | 将 SDL 事件转换成 Update 阶段查询的鼠标/键盘状态 |

### 游戏循环（`GameApp::Run`）

1. **输入：** SDL 事件 → `InputHandler`。
2. **更新：** `SceneManager` → `Board::Update()` + `GameObjectManager::Update()`，处理生成、AI 和碰撞。
3. **渲染：** `Draw()` 按渲染顺序遍历对象；Vulkan 可由 worker 并行 record/replay 并提交 InstanceRecord/Batch，OpenGL 强制在 Context 主线程按相同顺序串行 CPU 展开并调用 `Graphics::FlushBatch()`。两者都不得跨提交序列重排。

### 所有权与场景边界

- `SceneManager` 只持有一个 `unique_ptr<Scene>`。`SwitchTo()` 会先执行当前场景的 `OnExit()` 再销毁；本项目的 `Scene::OnExit()` 会清理全局 `GameObjectManager`，因此不支持把旧场景压栈后恢复。需要覆盖式 UI 时应留在当前场景内：`UIManager` 直接拥有 `Button`、`Slider` 和 `GameMessageBox`，模态框关闭请求在控件遍历结束后统一清理，不得再借用 `GameObjectManager` 的玩法对象生命周期。普通控件先于活动 `GameMessageBox` 绘制；调用方通过禁用背景控件屏蔽命中，不能通过停止绘制背景控件来伪造模态效果。
- `GameScene` 用 `unique_ptr` 独占 `Board`；多数运行对象由 `GameObjectManager` 的 `shared_ptr` 持有。`Board` 中的 `Cell*`、预览僵尸指针以及场景缓存指针均为非拥有索引，奖杯、弹坑等可失效引用优先使用 `weak_ptr`。
- `Board` 不依赖具体 `GameScene`，只保存非拥有的 `BoardPresentation*`。天气、波次和生存模式玩法状态只能由 `Board` 持有；场景只实现提示、闪屏、进度条和 UI 计时快照。新增展示请求应扩充这个窄端口，不能重新加入 `Board::mGameScene` 或在场景复制玩法状态。

### Board 网格

棋盘是 `vector<vector<Cell*>>` 非拥有寻址网格，`Cell` 的实际所有权在 `GameObjectManager`。植物放置在 `(row, column)`，僵尸按行移动。`Board` 管理僵尸波次以及 `BoardState` 状态转换：`CHOOSE_CARD → GAME → WIN` 或 `LOSE_GAME`；`NONE` 表示尚未初始化。

`Board` 拥有当前关卡的行数、首行 Y 与行高：普通草地为 5×100px，泳池背景为 6×85px（水路是 0-based 第 2/3 行）。位置、弹坑、子弹影子与小推车必须优先调 `GetCellCenterPosition` / `GetCellHeight`，不要再硬编 `CELL_INITALIZE_POS_Y + row*100`。`Cell` 当前分 `under/normal/pumpkin/overlay` 四层植物槽；战斗顶层优先级仍为 `pumpkin > normal > under`，短时飞行覆盖层不参与啃食 top。正式放置入口是 `Board::CanPlantAt`，僵尸啃咬只选 `GetTopPlantAt`。明确作用于整格植物组合的通用效果走 `Board::ForEachActivePlantInCell`：按 `overlay/pumpkin/normal/under` 快照实体 ID，并在每次动作前重新解析活动实体；需要聚合拦截或传播时序的效果另设窄专用入口。跳跃阻拦另走 `GetJumpBlockingPlantAt` 按层询问能力，非阻拦南瓜不会遮蔽内层高坚果。铲子单独按格内可见区域选层：南瓜中空中心选 `normal`，外圈选 `pumpkin`，空壳整格仍选南瓜；命中区域必须按当前 Cell 宽高派生，悬停高亮与最终铲除结果共用同一目标。需要南瓜拦截的僵尸范围扣血统一走 `ApplyPumpkinProtectedZombieAreaDamage`：先按技能原几何找命中植物，再为每个命中层选择自身逻辑九宫格内最近的活动南瓜，稳定打破并列并按保护者 ID 归并为一次默认 5 倍外壳伤害；爆破工头显式使用 4 倍重载，无保护者仍逐层结算，南瓜之间不连锁，普通小丑直接清除不走此入口。新增层必须同步创建/读档创建、释放/清理、render order、台风整组搬运、外部范围伤害与 AutoTest 投影。

战场主体绘制按行交错：`row N 植物 → row N 僵尸/扶梯 → row N+1 植物`，所以同排僵尸仍盖住植物，而下一行植物会正确遮住上一行越界伸下来的身体。`LAYER_GAME_PLANT` / `LAYER_GAME_ZOMBIE` 继续表示对象语义层，`GameObjectManager` 只在二者到 `LAYER_GAME_BULLET` 之间编排实际绘制号；小推车的 `LAYER_GAME_OBJECT` 和子弹层不变。任何运行期 `mRow` 变化必须通过排序键刷新入口重新分配绘制号，不能只改字段。

### 存档系统

使用 nlohmann/json 进行 JSON 序列化（`GameInfoSaver`）。植物和僵尸通过 `SaveExtraData(json&)`、`LoadExtraData(const json&)` 保存和恢复自定义状态。`PlayerInfo.json` 保存全局状态，`level{N}_data.json` 保存各关卡状态。Windows 通过 `FOLDERID_SavedGames` 写入系统“保存的游戏”目录（默认 `%USERPROFILE%\Saved Games\PlantsVsZombies\saves`）；Android 仍使用 `SDL_GetPrefPath`，Linux 暂沿用 `./saves/`。

两类 JSON 根节点都写入独立的 `schemaVersion`，并在任何运行状态被修改前由纯逻辑 `SaveSchema` 事务式升级。缺版本的历史档视为 v0；高于当前程序的未来版本、非对象根节点或非法版本字段一律拒绝加载，失败时输入文档和游戏状态均不应被部分修改。新增持久化结构变化时，应在 `SaveSchema` 增加逐版本迁移并同步 `SaveSchemaTests`，不要把一次性兼容分支继续散落到对象恢复过程。

玩家 schema v5 在 v4 `lastSelectedCards` 之外增加 `crazyDaveTutorialsSeen`，按稳定冒险关卡号保存已经完整看完或主动跳过的戴夫闲聊；v4 及更早旧档迁移为空数组。恢复时闲聊记录只接收当前冒险流程内的整数并排序去重；上次选卡仍只从当前选卡面板已有卡中按名解析、去重并遵守 11 张上限，未知、未注册或未拥有的卡会跳过；按钮恢复必须复用 `Card::SetTargetPosition` 的既有飞行动画，不能直接改卡片坐标。

Windows 首次发生真实存档访问时，会把当前工作目录旧 `./saves/` 中的普通文件复制到中央目录、逐字节校验后再删除源文件，跨磁盘同样安全。目标已有相同文件时只清理重复源文件；同名但内容不同则中央档优先、旧档原地保留且记录警告；迁移失败的缺失文件仍可逐文件回退旧目录读取。AutoTest 不触发迁移，`-AutoTestLoadSave` 始终只读构建目录下的 `./saves/`。

### 资源与资产

- 资源键是在 `ResourceKeys.h` 中手写的字符串常量，命名为 `PREFIX_UPPERCASE`（如 `IMAGE_PEASHOOTER`、`SOUND_CHERRYBOMB`、`MUSIC_DAY`、`PARTICLE_EXPLOSIONCLOUD`）。它们是 `ResourceManager::GenerateStandardKey` 根据文件名生成的实际键的防拼写错误镜像：去掉目录和扩展名 → 转大写 → 非字母数字转 `_` → 添加前缀。键值与常量名相同时，用 `RKEY(X)` 宏（展开为 `inline const std::string X = "X"`），避免重复书写；键值与名称不同时（例如 `IMAGE_HUGE_WAVE_APPROACHING = "IMAGE_APPROACHING"`、`SOUND_SHOOTER_SHOOT = "SOUND_THROW"`、值为 CamelCase 的 `REANIM_*`、字体路径），必须显式声明。
- 资产根目录：图片在 `./resources/image/`，粒子 XML 在 `./resources/particles/config/`，reanim 文件在 `./resources/reanim/`，字体在 `./font/`。
- **资源加载闭环：** `manifest.txt` 是构建期文件清单，也是 `image/reanim/` 等目录在 Android/桌面的枚举来源，但它不代替各资源类型自己的注册与键规则：

  | 资源类型 | 进入加载器的条件 | 实际运行时键 | 最小断言 |
  |---|---|---|---|
  | `.reanim` | 文件进入 manifest，且 `resources.xml/<Reanimations>` 有同名 `<Reanimation name>` | `REANIM_*` 常量的值必须等于 `name` | `HasReanimation(key)` |
  | `image/reanim/*.png` 运行时换图 | 文件进入 manifest；启动时 `LoadAllImagesFromPath` 全量加载 | `IMAGE_` + 大写文件名 stem；只有 reanim `<i>` 实际引用的图另有 `IMAGE_REANIM_*` 别名 | `GetTexture(key, false) != nullptr` |
  | 游戏图片 | `resources.xml/<GameImages>` 明确列出 | `IMAGE_` 标准键 | `GetTexture(key, false) != nullptr` |
  | 粒子专用 PNG | `resources.xml/<ParticleTextures>` 明确列出 | `PARTICLE_` 标准键 | `GetTexture(key, false) != nullptr` |
  | 音效 | `resources.xml/<Sounds>` 明确列出 | `SOUND_` + 大写文件名 stem | `HasSound(key)` |
  | 粒子配置 | XML 位于 `particles/config/` 并进入启动扫描 | 第一个 `<Emitter>` 的 `<Name>` | 发射前计数 0、发射后计数 1 + 同步截图 |

  原版部分 reanim 素材用 `Name.jpg` 保存颜色、用同尺寸 `Name_.png` 保存灰度透明遮罩；两者同时存在时 `ResourceManager` 在解码 JPG 后自动把遮罩亮度写入 alpha。reanim 按 `.png`、`.jpg` 候选路径加载时，前一个合法候选缺失不是资源错误，只有全部候选失败才记录一次 ERROR。不得把带黑底的 JPG 单独当作最终纹理，也不得把 `_` PNG 白色轮廓误当作彩色替代图。

  文件存在、manifest 存在、效果肉眼偶尔可见都不是单独的充分证据；运行时换图和掉落物应把加载状态导出到 AutoTest。Release 的资源 WARN 不保证进入 `run.log`，因此不能只 grep 日志。强制 reanim/纹理缺失必须修正注册或键来源，不能用通用 null guard 将坏对象留在场上。
- **Reanimation：** `Reanimation/` 是自定义骨骼动画系统，不是精灵表播放器。它加载 `.reanim` XML 文件，通过 `Animator::PlayTrack()` 播放命名轨道（如 `anim_walk`）。帧事件可在指定帧注册一次性回调。
- **粒子特效：** 粒子效果通过 `./resources/particles/config/` 下 XML 配置；一个文件可并列多个顶层 `<Emitter>` 片段（包含 `<Image>`、`<LaunchSpeed>`、`<Field>` 等），`ParticleXMLLoader` 以首个 Emitter 的 Name 缓存整组效果。

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
- `mRow`、`mColumn` 表示游戏网格单元；像素位置存放在宿主唯一的 `Transform`，视觉偏移继续单独使用 `mVisualOffset`。
- 代码文件统一使用 UTF-8（无 BOM），由根目录 `.editorconfig` 约束；中文 UI 字符串使用 UTF-8。
- **头文件保护（每个 `.h`）：** 每个头文件必须以 `#pragma once` 开头。旧有的 `#pragma once` + `#ifndef _NAME_H` 双重形式也可接受。运行 `cmake --preset` 时会安装 `.githooks/pre-commit` hook（`git config core.hooksPath .githooks`），拒绝暂存区中缺少保护的头文件；同一配置步骤也会用 WARNING 列出仓库里已有的无保护头文件。检查支持 BOM：在前 512 字节内匹配 token，而不是锚定 `^`，因此 UTF-8 BOM 不会造成误报。原因是迁移掉 `.sln` 后，VS 的“添加新项”模板不再自动插入保护。

## 沟通风格

回复时始终称呼用户为 **主人**，不要使用泛化的“用户”或“你”。例如使用“主人需要构建项目”。本规则适用于本仓库上下文中的所有解释、建议和对话。
