# 设计：PvZ 代码跨平台化（第一阶段——代码可移植）

- 日期：2026-06-24
- 状态：已与主人确认，待转 writing-plans
- 目标平台（未来）：Android、Linux（桌面）；当前唯一构建/验证平台：x64 Windows

## 背景与动机

本项目目前是纯 x64 Windows（CMake + vcpkg `x64-windows-static`）。主人希望未来能在 Android、Linux 上运行，先让 **C++ 源码层** 去掉 Windows 专属硬假设，为后续移植打好地基。

勘察结论：**渲染 / 窗口 / 输入 / Vulkan 表面层其实已经跨平台**——入口是 `int main(int,char**)` 配 `#define SDL_MAIN_HANDLED`（非 `WinMain`），Vulkan 表面用 `SDL_Vulkan_CreateSurface` / `SDL_Vulkan_GetInstanceExtensions`，无任何 Win32 表面代码。真正阻碍非 Windows 运行的只有三类：

1. **CrashHandler**：纯 Win32（VEH、DbgHelp、shellapi），头文件 `#include <windows.h>`，被 `main.cpp` 与 `GameMonitor.h` include。
2. **文件读取**：资源用 `IMG_Load(路径)` / `TTF_OpenFont(路径)` / `Mix_LoadWAV/MUS(路径)` 与裸 `std::ifstream`——Android 上资源被打进 APK，不是真实文件，这些必然失败。
3. **存档路径**：`GameInfoSaver` 写死 `./saves/`，依赖当前工作目录；Android 上该目录不可写。

外加一处入口的 Windows 专属调用：`main.cpp` 的 `system("chcp 65001")` / `system("cls")`。

### 关键技术依据（决定设计走向）

- **读取换引擎**：`SDL_RWFromFile` 对**相对路径**自动从 Android APK assets 读、对**绝对路径**走真实文件系统；桌面则一律相对 CWD。因此读取侧只要全程改用 `SDL_RWFromFile`，桌面（行为不变）与 Android（读 APK）共用同一套代码，无需 `#ifdef`。SDL_RWops 本身就是 SDL 内置的虚拟文件系统，不必再自造一层 VFS。
- **写入只换地址**：`SDL_GetPrefPath()` 返回真实可写的绝对路径，普通 `std::ofstream` 即可写入。写入侧不需要 RWops，只需把写死的 `"./saves"` 换成一个按平台决定根目录的 `GetSaveRoot()`。

一句话：**读取换引擎（SDL_RWops），写入只换地址（GetSaveRoot）。**

## 范围 / 非目标

### 本次范围（第一阶段）
仅 C++ 源码层改造，使代码不再含 Windows 专属硬假设、能在 Android/Linux 上编译运行。三个组件 + 入口清理（见下）。

### 本次不做（留作后续独立项目）
- Android 的 Gradle/NDK 工程、SDL Android Activity 引导、把 `resources/`/`font/` 打进 APK assets。
- Linux/Android 的 CMake toolchain 与 triplet。
- CMakeLists 里 MSVC 专属旗标（`/utf-8 /W3 /sdl /EHsc`、`/clang:-O2` 等）与 Windows 系统库（`version setupapi imm32 winmm legacy_stdio_definitions`）的跨平台化——这些要等真正搭非 Windows 构建时再按编译器分支处理。
- **TestDriver（AutoTest 子系统）的文件 I/O 豁免**：它读测试脚本 / 写测试产物，是开发期桌面专用工具，永不在 Android 上运行，故其 `std::ifstream`/`std::ofstream` 本次不动。

### 验证手段
无 Linux/Android 构建环境，可移植性靠两条保证：
1. **源码不再有 Windows 专属硬假设**（本设计逐点消除）。
2. **桌面回归零行为变化**：Windows `clang-release` 构建 0 warning / 0 error；AutoTest 跑现有脚本（资源加载 + 存读档）并 Read 截图确认与改造前一致。

## 组件 1 — 入口与 CrashHandler 平台隔离

### 1a. CrashHandler 头文件可移植化
- `CrashHandler.h` **去掉 `#include <windows.h>`**；把所有 Win32 类型签名的私有成员/方法（`PVOID vehHandle`、`LONG64 lastUACTime`、所有以 `PEXCEPTION_POINTERS`/`DWORD`/`HMODULE`/`LONG WINAPI` 为签名的私有函数）从头文件移除，下沉为 `.cpp` 内部实现细节（文件内 `static` / 匿名命名空间）。
- 头文件对外只保留两个跨平台静态接口：`static void Initialize();` / `static void Cleanup();`。
- 结果：`main.cpp` 与 `GameMonitor.h` 的 `#include "CrashHandler.h"` 在任何平台都安全。

### 1b. CrashHandler 实现平台隔离
- `CrashHandler.cpp` 全文用 `#if defined(_WIN32) … #else … #endif` 包裹：
  - Windows 分支 = 现有完整实现（含其内部的 `std::ofstream` 崩溃报告写入，一并被 `_WIN32` 守卫）。
  - 非 Windows 分支 = 空的 `Initialize()` / `Cleanup()`（无操作）。
- 效果：非 Windows 上**不编译任何 Win32 代码**，且 `main.cpp` 两处调用零改动。
- **决定（经主人认可）**：采用此"内部 `#ifdef` 编成空壳"方案，而非"CMake `REMOVE_ITEM` 排除文件 + 另写 stub"——前者调用处零改、CMakeLists 零改，更简洁，且同样达到"不编译 Win32 代码"。

### 1c. main.cpp 入口的 Windows 专属调用
- `main.cpp:14-15` 的 `system("chcp 65001")` 与 `system("cls")` 用 `#if defined(_WIN32) … #endif` 包裹。理由：`chcp`（设控制台 UTF-8 代码页，仅 Windows 需要）/`cls`（清屏）是 Windows 命令；Linux 终端默认 UTF-8、Android 无控制台。

## 组件 2 — 读取侧统一 SDL_RWops（资源可读 APK）

目标：所有**只读资源**的加载改走 `SDL_RWFromFile`，使其在 Android 上能从 APK assets 读取，同时桌面行为不变（仍相对 CWD）。资源路径保持相对（`./resources/…`、`Shader/…`、`font/…`），不引入路径根改动。

### 2a. FileManager 读取底层换 RWops
- `FileManager::LoadFileAsString` 与 `FileManager::LoadFileAsBinary`：底层从 `std::ifstream` 换成 `SDL_RWFromFile(path.c_str(), "rb")` + `SDL_RWsize` / `SDL_RWread` / `SDL_RWclose`；打开失败（返回 `nullptr`）走现有 `LogError` + 返回空值的路径。
- 连带受益（无需各自改）：`LoadXMLFile` / `LoadJsonFile` 经由 `LoadFileAsString` 自动获得 APK 读取能力——覆盖 reanim XML、粒子 XML、以及读存档 JSON。
- **写入函数不动**：`SaveFile` / `SaveBinaryFile` / `AppendToFile` / `SaveJsonFile` / `SaveXMLFile` 保持 `std::ofstream`（仅用于存档，见组件 3）。`std::filesystem` 工具函数（`CreateDirectory`/`IsDirectory`/`GetFilesInDirectory`/`DeleteFile`/`GetFileSize`）本次不动——仅在桌面存档路径下使用，Linux 的 `std::filesystem` 同样可用；Android 侧只在 prefPath（真实文件系统）下用到，亦可用。

### 2b. ResourceManager 改用 SDL `_RW` 变体
四类资源就地替换为 `_RW` 变体，并以 `SDL_RWFromFile(path,"rb")` 作源、第二参 `freesrc = 1`（SDL 用完自动关闭 RWops，避免泄漏）。打开/加载失败仍走各自现有 `LOG_ERROR` + 返回 `nullptr`：
- `IMG_Load(filepath.c_str())` → `IMG_Load_RW(SDL_RWFromFile(filepath.c_str(),"rb"), 1)`
  - 两处：`LoadTexture`（约 ResourceManager.cpp:42）、`LoadTiledTextureGL`（约 :109）。
- `TTF_OpenFont(fontPath.c_str(), size)` → `TTF_OpenFontRW(SDL_RWFromFile(fontPath.c_str(),"rb"), 1, size)`（约 :434）。
- `Mix_LoadWAV(path.c_str())` → `Mix_LoadWAV_RW(SDL_RWFromFile(path.c_str(),"rb"), 1)`（约 :511）。
- `Mix_LoadMUS(path.c_str())` → `Mix_LoadMUS_RW(SDL_RWFromFile(path.c_str(),"rb"), 1)`（约 :549）。
- 注意：`SDL_RWFromFile` 返回 `nullptr` 时不要把 `nullptr` 传给 `_RW`（应先判空并按失败处理），以保留清晰的错误日志。

### 2c. 收编绕过 FileManager 的零散读取
- `VulkanPipeline::LoadFile`（VulkanPipeline.cpp:22-37，读 `.spv` 的 `std::ifstream`）：改用 `FileManager::LoadFileAsBinary`（或直接 `SDL_RWFromFile`）。**必须保留** SPIR-V 校验：`size > 0` 且 `size % 4 == 0`，否则记错并返回 false。
- `PlantAlmanacScene.cpp:217` 与 `ZombieAlmanacScene.cpp:217` 的 `std::ifstream("./resources/info.txt")`：改用 `FileManager::LoadFileAsString`。

### 2d. 一致性约束
实现完成后，除 TestDriver（豁免）与组件 3 的写入外，代码中**不应再有读取资源用的裸 `std::ifstream` / `IMG_Load(路径)` / `TTF_OpenFont(路径)` / `Mix_LoadWAV(路径)` / `Mix_LoadMUS(路径)`**。这是收口完整性的验收点。

## 组件 3 — 存档位置 GetSaveRoot()

### 3a. 新增 GetSaveRoot()
- 位置：`GameInfoSaver`（或 FileManager），返回 `std::string`（不含尾部 `/`，与现有 `"./saves"` 一致）。
- 行为：
  - 桌面（`#if !defined(__ANDROID__)`，覆盖 Windows 与 Linux）→ 返回 `"./saves"`（行为完全不变）。
  - Android（`#if defined(__ANDROID__)`）→ 首次调用 `SDL_GetPrefPath(org, "PlantsVsZombies")`，缓存其返回字符串（去尾斜杠后）于函数内 `static`，并 `SDL_free` 原指针；后续调用返回缓存。`org` 取一个稳定标识（如 `"PvZ"`，具体值在实现/计划阶段定，仅 Android 用到）。

### 3b. GameInfoSaver 路径改造
- 把所有写死的 `"./saves"` / `"./saves/..."` 改为基于 `GetSaveRoot()`：
  - `CreateDirectory("./saves")`（两处，约 :77、:124）→ `CreateDirectory(GetSaveRoot())`。
  - `"./saves/PlayerInfo.json"`（约 :91、:98）→ `GetSaveRoot() + "/PlayerInfo.json"`。
  - `"./saves/level" + N + "_data.json"`（约 :321/:322、:328、:582/:583）→ `GetSaveRoot() + "/level" + N + "_data.json"`。
- 写入仍用现有 `FileManager::SaveJsonFile`（`std::ofstream`）；prefPath 为真实可写绝对路径，无需 RWops。
- AutoTest 模式存档已短路（不读写 saves），不受影响。

## 数据流自洽校验

| 流向 | 路径来源 | 桌面 | Android |
|---|---|---|---|
| 读资源 | 相对 `./resources/…` | `SDL_RWFromFile` 相对 CWD ✓ | `SDL_RWFromFile` 相对 → APK assets ✓ |
| 读存档 | `GetSaveRoot()` | 相对 `./saves` → `SDL_RWFromFile` 相对 CWD ✓ | 绝对 prefPath → `SDL_RWFromFile` 真实 FS ✓ |
| 写存档 | `GetSaveRoot()` | 相对 `./saves` → `std::ofstream` ✓ | 绝对 prefPath → `std::ofstream` 私有目录可写 ✓ |

唯一需要平台分叉的只有两处：CrashHandler / main 入口（整块 Win32）与 `GetSaveRoot()`（一处三元）。其余全靠"换 API 不换路径"的 RWops 收口，桌面 / Android 共用一套读取码。

## 错误处理

- `SDL_RWFromFile` 返回 `nullptr` → 按各调用点现有失败路径（`LogError` / `LOG_ERROR` + 返回空/false），与原 `ifstream` 打开失败语义一致。
- `_RW` 加载一律 `freesrc = 1`，即便加载失败 SDL 也会关闭 RWops，无泄漏。
- 保留：nlohmann/json 解析 try-catch、SPIR-V `size % 4` 校验、GameInfoSaver 既有的 `*Impl` + try/catch 异常边界。

## 测试 / 验证（桌面回归）

1. **构建**：`clang-release` 0 warning / 0 error（沿用现有警告零基线，警告验证须用 clang）；`msvc-debug` 构建通过。
2. **AutoTest 回归**（工作目录 = `build/<preset>/`）：
   - `demo_peashooter.json`——验证图片 / 音频 / shader / reanim 资源照常加载（截图比对）。
   - 一个夜间蘑菇脚本（`goto_level` 10–19）——验证另一批资源路径。
   - 存读档：跑一个产生存档再读回的流程（或现有 smoke 脚本），确认桌面 `./saves` 路径与内容不变。
3. 退出码 0、`run.log` 无新增 ERROR。

通过即证明：读取侧改走 SDL_RWops、存档接 `GetSaveRoot()` 之后，桌面行为零变化；同时源码已不含组件 1–3 所列的 Windows 专属硬假设。

## 后续阶段（本设计之外，记录备忘）

- 第二阶段：Linux 实际构建（CMakeLists 按编译器分支 MSVC↔clang/gcc 旗标、系统库、x64-linux triplet），作为可移植性试金石。
- 第三阶段：Android Gradle/NDK 工程、SDL Android Activity、资源打包进 APK assets、`org` 取值与签名等。
