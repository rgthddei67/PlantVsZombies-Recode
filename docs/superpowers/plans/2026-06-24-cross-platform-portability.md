# 跨平台化第一阶段（代码可移植）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 去掉 PvZ C++ 源码里的 Windows 专属硬假设（CrashHandler、入口 `system()`、文件读取、存档路径），使代码能在 Android/Linux 上编译运行，同时桌面行为零变化。

**Architecture:** 三组件 + 入口清理。读取侧统一 `SDL_RWFromFile`（相对路径→Android APK / 桌面 CWD，绝对路径→真实文件系统），桌面/Android 共用一套读取码；写入侧只把写死的 `"./saves"` 换成按平台决定根目录的 `GetSaveRoot()`；CrashHandler 与入口 `system()` 用 `#if defined(_WIN32)` 隔离。唯一平台分叉只有 CrashHandler/入口（整块 Win32）与 `GetSaveRoot()`（一处三元）。

**Tech Stack:** C++17、SDL2 / SDL2_image / SDL2_ttf / SDL2_mixer（RWops 体系）、nlohmann/json、pugixml、Vulkan、CMake + vcpkg（`x64-windows-static`）。

**Spec:** `docs/superpowers/specs/2026-06-24-cross-platform-portability-design.md`

---

## 执行环境与通用命令

> 源码根在嵌套子目录 `PlantVsZombies/PlantVsZombies/`（如 `PlantVsZombies/CrashHandler.cpp`）。项目根（git 根、含 `docs/`、`autotest/`、`CMakeLists.txt`）是 `D:\PVZ\PlantsVsZombies\PlantVsZombies`。本计划所有源码路径以**项目根**为基准。
>
> **若在 git worktree 执行**：AutoTest 需要资源——先从主工作区 `build/<preset>/` 把 `resources/` 与 `font/` 拷到 worktree 的 `build/<preset>/`，否则 ResourceManager 初始化失败 exit-6（见项目记忆 `reference_pvz_assets_worktree_autotest_gotchas`）。顺序在主工作区执行可免此步。

### 【命令A：导入 VS 开发环境】（每个新 shell 跑一次）
```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
```

### 【命令B：构建 msvc-debug】（快速迭代，功能验证）
```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```
预期：`Build succeeded`，退出码 0。

### 【命令C：构建 clang-release】（警告零基线——只有 clang 报 -Wunused/-Wreorder 等）
```powershell
cmake --preset clang-release
cmake --build --preset clang-release
```
预期：`Build succeeded`，**0 warning / 0 error**。

### 【命令D：跑某 AutoTest 脚本】（工作目录 = 含 exe 的 build 目录）
```powershell
Push-Location build\msvc-debug    # 或 build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本名>.json -Seed 42
$LASTEXITCODE                      # 0=成功；非0=失败
Pop-Location
```
产物在 `build\<preset>\autotest\out\<脚本名>\`（PNG 用 Read 工具看，`run.log` 看执行轨迹）。

---

## File Structure（本计划触及的文件）

| 文件（项目根相对） | 改动 | 责任 |
|---|---|---|
| `PlantVsZombies/CrashHandler.h` | 修改 | 头文件去 windows.h 依赖；`#if defined(_WIN32)` 守卫 Win32 私有成员；对外仅 `Initialize/Cleanup` |
| `PlantVsZombies/CrashHandler.cpp` | 修改 | 全文 `#if defined(_WIN32)` 包裹；非 Windows 空实现 |
| `PlantVsZombies/main.cpp` | 修改 | `system("chcp")`/`system("cls")` 加 `_WIN32` 守卫 |
| `PlantVsZombies/FileManager.cpp` | 修改 | `LoadFileAsBinary`/`LoadFileAsString` 底层换 `SDL_RWFromFile` |
| `PlantVsZombies/ResourceManager.cpp` | 修改 | 图/字/音 4 类资源换 `IMG_Load_RW`/`TTF_OpenFontRW`/`Mix_LoadWAV_RW`/`Mix_LoadMUS_RW` |
| `PlantVsZombies/Renderer/VulkanPipeline.cpp` | 修改 | `LoadFile` 改用 `FileManager::LoadFileAsBinary`（保留 SPIR-V 校验） |
| `PlantVsZombies/Game/PlantAlmanacScene.cpp` | 修改 | `info.txt` 读取改 `LoadFileAsString` + `istringstream` |
| `PlantVsZombies/Game/ZombieAlmanacScene.cpp` | 修改 | 同上 |
| `PlantVsZombies/GameInfoSaver.cpp` | 修改 | 新增匿名命名空间 `GetSaveRoot()`；所有 `./saves` 路径改用之 |

`GameInfoSaver.h` 不改（`GetSaveRoot()` 为文件内 static，不暴露接口）。

---

## Task 1：组件 1 — 入口与 CrashHandler 平台隔离

**Files:**
- Modify: `PlantVsZombies/CrashHandler.h`（整文件重写头部结构）
- Modify: `PlantVsZombies/CrashHandler.cpp:1-17`（include 段 + 全文包裹）、`:467` 末尾
- Modify: `PlantVsZombies/main.cpp:14-15`

- [ ] **Step 1：重写 `CrashHandler.h` —— 头文件可移植化**

把整个文件改成下面内容（关键：`#include <windows.h>` 与所有 Win32 私有成员都进 `#if defined(_WIN32)`；对外只暴露两个跨平台静态函数）：

```cpp
#pragma once
#ifndef _CRASH_HANDLER_H
#define _CRASH_HANDLER_H

#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

class CrashHandler {
public:
    // 初始化崩溃处理器(VEH)。非 Windows 平台为空操作。
    static void Initialize();

    // 清理崩溃处理器。非 Windows 平台为空操作。
    static void Cleanup();

#if defined(_WIN32)
private:
    static PVOID vehHandle;
    static LONG64 lastUACTime;

    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo);
    static void HandleCrash(PEXCEPTION_POINTERS exceptionInfo);
    static void HandleStackOverflowMinimal(PEXCEPTION_POINTERS exceptionInfo);
    static bool IsCrashException(DWORD exceptionCode);
    static std::string GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo);
    static void ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath);
    static std::string GetExceptionCodeString(DWORD exceptionCode);
    static std::string GetStackTrace(PEXCEPTION_POINTERS exceptionInfo);
    static std::string GetModuleName(HMODULE module);
    static bool IsUACByTimeWindow(PEXCEPTION_POINTERS exceptionInfo);
#endif
};

#endif
```

- [ ] **Step 2：包裹 `CrashHandler.cpp` 全文**

在第 1 行 `#include "CrashHandler.h"` **之后**插入 `#if defined(_WIN32)`，并把原文件第 2 行到第 467 行（即 `#include "Logger.h"` 起、直到文件末尾 `IsUACByTimeWindow` 的 `}`）全部留在该分支内；在文件**最末尾**追加非 Windows 空实现。即顶部变为：

```cpp
#include "CrashHandler.h"
#if defined(_WIN32)
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <DbgHelp.h>
#include <shellapi.h>
#include <psapi.h>
#include <TlHelp32.h>
#include <algorithm>

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "Shell32.lib")

PVOID CrashHandler::vehHandle = nullptr;
LONG64 CrashHandler::lastUACTime = 0;

// ……（第 19 行起的所有现有实现原样保留，不改一字）……
```

文件**末尾**（原第 467 行 `}` 之后）追加：

```cpp
#else   // !_WIN32

void CrashHandler::Initialize() {}
void CrashHandler::Cleanup() {}

#endif  // _WIN32
```

> 注：现有 Windows 实现一字不改，只是被 `#if defined(_WIN32)` 包住。这与 spec 组件 1b 等价（头文件可移植 + 非 Windows 不编译任何 Win32 代码），但改动面更小——无需把成员函数改写为自由函数。

- [ ] **Step 3：守卫 `main.cpp` 的 `system()` 调用**

`main.cpp:14-15`：

```cpp
    system("chcp 65001");
    system("cls");
```

改为：

```cpp
#if defined(_WIN32)
    system("chcp 65001");   // 设控制台 UTF-8 代码页：仅 Windows 需要
    system("cls");
#endif
```

（其余 `main.cpp` 不变；`CrashHandler::Initialize()`/`Cleanup()` 调用零改动，因两函数在所有平台都存在。）

- [ ] **Step 4：构建验证（Windows 上 `_WIN32` 为真，行为应完全不变）**

跑【命令A】后跑【命令C：构建 clang-release】。
预期：`Build succeeded`，**0 warning / 0 error**。

- [ ] **Step 5：启动冒烟（确认崩溃处理初始化路径未破坏）**

跑【命令D】用脚本 `smoke_quit`（preset 用 `clang-release`）。
预期：`$LASTEXITCODE` = 0；`build\clang-release\autotest\out\smoke_quit\run.log` 无新增 ERROR。

- [ ] **Step 6：提交**

```powershell
git add PlantVsZombies/CrashHandler.h PlantVsZombies/CrashHandler.cpp PlantVsZombies/main.cpp
git commit -m @'
feat(xplat): CrashHandler 与入口 system() 平台隔离

CrashHandler.h 去 windows.h 依赖(_WIN32 守卫私有成员)，对外仅 Initialize/Cleanup；
CrashHandler.cpp 全文 _WIN32 包裹、非 Windows 空实现；main.cpp 的 chcp/cls 加守卫。
Windows 行为不变。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 2：组件 2a — FileManager 读取底层换 SDL_RWops

**Files:**
- Modify: `PlantVsZombies/FileManager.cpp:1-4`（include）、`:11-40`（两个读取函数）

- [ ] **Step 1：加 SDL include**

`FileManager.cpp` 顶部 `#include "FileManager.h"` 之后加：

```cpp
#include <SDL2/SDL.h>
```
（`<filesystem>`、`<sstream>` 保留——`CreateDirectory`/`SaveXMLFile` 仍用。）

- [ ] **Step 2：`LoadFileAsBinary` 换 RWops**

把 `FileManager.cpp:23-40` 的 `LoadFileAsBinary` 整体替换为：

```cpp
std::vector<char> FileManager::LoadFileAsBinary(const std::string& path) {
    // SDL_RWFromFile：相对路径在 Android 自动读 APK assets / 桌面读 CWD；绝对路径走真实文件系统。
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) {
        LogError("Failed to open binary file: " + path);
        return {};
    }

    Sint64 size = SDL_RWsize(rw);
    if (size < 0) {
        LogError("Failed to get size of binary file: " + path);
        SDL_RWclose(rw);
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(size));
    Sint64 readTotal = 0;
    if (size > 0) {
        readTotal = SDL_RWread(rw, buffer.data(), 1, static_cast<size_t>(size));
    }
    SDL_RWclose(rw);

    if (readTotal != size) {
        LogError("Failed to read binary file: " + path);
        return {};
    }
    return buffer;
}
```

- [ ] **Step 3：`LoadFileAsString` 复用 `LoadFileAsBinary`（DRY）**

把 `FileManager.cpp:11-21` 的 `LoadFileAsString` 整体替换为：

```cpp
std::string FileManager::LoadFileAsString(const std::string& path) {
    // 复用二进制读取（同样走 SDL_RWops）；空文件与打开失败都返回 ""，与原 ifstream 行为一致。
    std::vector<char> bytes = LoadFileAsBinary(path);
    return std::string(bytes.begin(), bytes.end());
}
```

> 行为说明：打开失败时由 `LoadFileAsBinary` 记 "Failed to open binary file"（措辞较原 "Failed to open file" 略变，无碍）。`LoadXMLFile`/`LoadJsonFile` 经 `LoadFileAsString` 自动获得 APK 读取能力，无需各自改。

- [ ] **Step 4：构建验证**

跑【命令C：构建 clang-release】。
预期：`Build succeeded`，**0 warning / 0 error**（注意：原 `<fstream>` 若 `FileManager.cpp` 已不再用则会触发 clang 的 unused-include？不会——unused include 不报警；但若有 unused 变量残留会报。确认 0 warning）。

- [ ] **Step 5：AutoTest 回归（reanim/粒子 XML 经 FileManager 读取）**

跑【命令D】用 `demo_peashooter`（preset `clang-release`），Read 截图 `build\clang-release\autotest\out\demo_peashooter\` 下的 PNG。
预期：`$LASTEXITCODE` = 0；豌豆射手动画与场景资源显示正常（与改造前一致）。

- [ ] **Step 6：提交**

```powershell
git add PlantVsZombies/FileManager.cpp
git commit -m @'
feat(xplat): FileManager 读取底层换 SDL_RWops

LoadFileAsBinary/LoadFileAsString 从 std::ifstream 改为 SDL_RWFromFile，
使凡走 FileManager 的读取(reanim/粒子 XML、存档 JSON)在 Android 能读 APK assets。
写入函数与 std::filesystem 工具不变。桌面行为不变。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 3：组件 2b — ResourceManager 四类资源换 _RW 变体

**Files:**
- Modify: `PlantVsZombies/ResourceManager.cpp:42`、`:109`、`:434`、`:511`、`:549`

每处统一模式：先 `SDL_RWFromFile(path,"rb")` 取源并判空（保留清晰错误日志），再调 `_RW` 变体且 `freesrc=1`（SDL 用完自动关闭 RWops，无泄漏）。

- [ ] **Step 1：`LoadTexture`（`:42`）**

把 `SDL_Surface* surface = IMG_Load(filepath.c_str());` 替换为：

```cpp
    SDL_RWops* rw = SDL_RWFromFile(filepath.c_str(), "rb");
    if (!rw) {
        LOG_ERROR("ResourceManager") << "LoadTexture 无法打开图片: " << filepath;
        return nullptr;
    }
    SDL_Surface* surface = IMG_Load_RW(rw, 1);   // freesrc=1
```

- [ ] **Step 2：`LoadTiledTextureGL`（`:109`）**

把 `SDL_Surface* loadedSurface = IMG_Load(info.path.c_str());` 替换为：

```cpp
    SDL_RWops* rw = SDL_RWFromFile(info.path.c_str(), "rb");
    if (!rw) {
        LOG_ERROR("ResourceManager") << "LoadTiledTexture 无法打开图片: " << info.path;
        return false;
    }
    SDL_Surface* loadedSurface = IMG_Load_RW(rw, 1);
```

- [ ] **Step 3：字体（`:434`）**

把 `TTF_Font* font = TTF_OpenFont(fontPath.c_str(), size);` 替换为：

```cpp
    SDL_RWops* fontRw = SDL_RWFromFile(fontPath.c_str(), "rb");
    if (!fontRw) {
        LOG_ERROR("ResourceManager") << "GetFont 无法打开字体: " << fontPath << " size: " << size;
        return nullptr;
    }
    TTF_Font* font = TTF_OpenFontRW(fontRw, 1, size);   // freesrc=1
```

- [ ] **Step 4：音效（`:511`）**

把 `Mix_Chunk* sound = Mix_LoadWAV(path.c_str());` 替换为：

```cpp
    SDL_RWops* sndRw = SDL_RWFromFile(path.c_str(), "rb");
    if (!sndRw) {
        LOG_ERROR("ResourceManager") << "LoadSound 无法打开音效: " << path;
        return nullptr;
    }
    Mix_Chunk* sound = Mix_LoadWAV_RW(sndRw, 1);   // freesrc=1
```

- [ ] **Step 5：音乐（`:549`）**

把 `Mix_Music* mus = Mix_LoadMUS(path.c_str());` 替换为：

```cpp
    SDL_RWops* musRw = SDL_RWFromFile(path.c_str(), "rb");
    if (!musRw) {
        LOG_ERROR("ResourceManager") << "LoadMusic 无法打开音乐: " << path;
        return nullptr;
    }
    Mix_Music* mus = Mix_LoadMUS_RW(musRw, 1);   // freesrc=1
```

- [ ] **Step 6：构建验证**

跑【命令C：构建 clang-release】。预期：`Build succeeded`，**0 warning / 0 error**。

- [ ] **Step 7：AutoTest 回归（图/字/音全路径）**

跑【命令D】用 `demo_peashooter` 与 `smoke_small_sun`（夜间蘑菇，另一批资源），preset `clang-release`。Read 两者截图。
预期：两者 `$LASTEXITCODE` = 0；图片、字体文字、（音频无截图但 `run.log` 无加载失败 ERROR）均正常。

- [ ] **Step 8：提交**

```powershell
git add PlantVsZombies/ResourceManager.cpp
git commit -m @'
feat(xplat): ResourceManager 图/字/音改用 SDL _RW 变体

IMG_Load/TTF_OpenFont/Mix_LoadWAV/Mix_LoadMUS 改为 *_RW(SDL_RWFromFile,...,1)，
使资源在 Android 能从 APK assets 加载。freesrc=1 防泄漏。桌面行为不变。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 4：组件 2c — 收编 shader 与年鉴 info.txt

**Files:**
- Modify: `PlantVsZombies/Renderer/VulkanPipeline.cpp:5`（include）、`:22-37`（LoadFile）
- Modify: `PlantVsZombies/Game/PlantAlmanacScene.cpp:171-172`（含 include）
- Modify: `PlantVsZombies/Game/ZombieAlmanacScene.cpp:217-218`（含 include）

- [ ] **Step 1：`VulkanPipeline::LoadFile` 收编（保留 SPIR-V 校验）**

`VulkanPipeline.cpp` 顶部 `#include <fstream>`（第 5 行）替换为：

```cpp
#include "../FileManager.h"
```

把 `:22-37` 的 `LoadFile` 整体替换为：

```cpp
    bool VulkanPipeline::LoadFile(const char* path, std::vector<char>& out) {
        out = FileManager::LoadFileAsBinary(path);   // 底层走 SDL_RWops
        if (out.empty()) {
            LOG_ERROR("VulkanPipeline") << "Failed to open shader file: " << path;
            return false;
        }
        if ((out.size() % 4) != 0) {
            LOG_ERROR("VulkanPipeline") << "Bad SPIR-V file size (" << out.size() << ") for " << path;
            return false;
        }
        return true;
    }
```

> `size <= 0` 的旧校验由 `out.empty()` 覆盖（空文件/打开失败均为空）；`% 4` 校验保留。

- [ ] **Step 2：`PlantAlmanacScene::LoadInfoFile` 收编**

确保 `PlantAlmanacScene.cpp` 顶部 include 了 `"../FileManager.h"`（相对该文件位置；若已 include 则跳过）与 `<sstream>`。把 `:171-172`：

```cpp
    std::ifstream file("./resources/info.txt");
    if (!file.is_open()) return;
```

替换为：

```cpp
    std::string content = FileManager::LoadFileAsString("./resources/info.txt");
    if (content.empty()) return;
    std::istringstream file(content);
```

（其后 `std::getline(file, line)` 解析循环**原样不变**——`istringstream` 与 `ifstream` 同为 `std::istream`。）
若该文件除此处外无其它 `ifstream` 用法，删除 `#include <fstream>`。

- [ ] **Step 3：`ZombieAlmanacScene::LoadInfoFile` 收编**

同 Step 2，对 `ZombieAlmanacScene.cpp:217-218` 作完全相同的替换（确保 include `"../FileManager.h"` 与 `<sstream>`）：

```cpp
    std::string content = FileManager::LoadFileAsString("./resources/info.txt");
    if (content.empty()) return;
    std::istringstream file(content);
```

若该文件除此处外无其它 `ifstream` 用法，删除 `#include <fstream>`。

- [ ] **Step 4：构建验证**

跑【命令C：构建 clang-release】。预期：`Build succeeded`，**0 warning / 0 error**。

- [ ] **Step 5：AutoTest 回归（shader 启动必经 + 年鉴 info.txt）**

跑【命令D】用 `smoke_quit`（shader 加载是启动必经——能起来即证明 `.spv` 读取 OK）与 `almanac_click`（年鉴渲染 info.txt 文本），preset `clang-release`。Read `almanac_click` 截图。
预期：两者 `$LASTEXITCODE` = 0；年鉴里植物/僵尸名称与描述文本正常显示（证明 info.txt 解析未变）。

- [ ] **Step 6：提交**

```powershell
git add PlantVsZombies/Renderer/VulkanPipeline.cpp PlantVsZombies/Game/PlantAlmanacScene.cpp PlantVsZombies/Game/ZombieAlmanacScene.cpp
git commit -m @'
feat(xplat): 收编 shader 与年鉴 info.txt 读取到 FileManager

VulkanPipeline::LoadFile 改用 FileManager::LoadFileAsBinary(保留 SPIR-V %4 校验)；
两个 Almanac 的 info.txt 改用 LoadFileAsString + istringstream(解析逻辑不变)。
至此读取侧无裸 ifstream(TestDriver 除外)。桌面行为不变。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 5：组件 3 — GetSaveRoot() + GameInfoSaver 路径改造

**Files:**
- Modify: `PlantVsZombies/GameInfoSaver.cpp`（顶部加 Android SDL include + 匿名命名空间 `GetSaveRoot`；`:77`、`:91`、`:98`、`:124`、`:321-322`、`:328`、`:582-583` 路径拼接）

- [ ] **Step 1：加 Android 专属 SDL include + `GetSaveRoot()`**

在 `GameInfoSaver.cpp` 顶部 include 段之后（第一个函数定义之前）加入：

```cpp
#if defined(__ANDROID__)
#include <SDL2/SDL.h>
#endif

namespace {
// 存档根目录（不含尾斜杠）。
// 桌面(Win/Linux)：沿用 "./saves"(相对 CWD，行为不变)。
// Android：CWD 不可写，改用 SDL 提供的应用私有可写目录。
const std::string& GetSaveRoot() {
#if defined(__ANDROID__)
    static const std::string root = []() -> std::string {
        char* p = SDL_GetPrefPath("PvZ", "PlantsVsZombies");
        if (!p) return "saves";                 // 极端兜底
        std::string s(p);
        SDL_free(p);
        if (!s.empty() && (s.back() == '/' || s.back() == '\\'))
            s.pop_back();                       // 去尾斜杠，拼接统一加 "/"
        return s;
    }();
    return root;
#else
    static const std::string root = "./saves";
    return root;
#endif
}
} // namespace
```

> `SDL_GetPrefPath` 第一参 org 取 `"PvZ"`（仅 Android 用到；第三阶段搭 Android 工程时可按正式包名调整）。

- [ ] **Step 2：改 PlayerInfo 路径与建目录（`:77`、`:91`、`:98`、`:124`）**

- `:77` 与 `:124` 的 `FileManager::CreateDirectory("./saves");` → `FileManager::CreateDirectory(GetSaveRoot());`
- `:91` 的 `return FileManager::SaveJsonFile("./saves/PlayerInfo.json", j);` → `return FileManager::SaveJsonFile(GetSaveRoot() + "/PlayerInfo.json", j);`
- `:98` 的 `if (!FileManager::LoadJsonFile("./saves/PlayerInfo.json", j))` → `if (!FileManager::LoadJsonFile(GetSaveRoot() + "/PlayerInfo.json", j))`

- [ ] **Step 3：改 level 数据路径（`:321-322`、`:328`、`:582-583`）**

把三处 `std::string filename = "./saves/level" + std::to_string(board->mLevel) + "_data.json";` 统一替换为：

```cpp
    std::string filename = GetSaveRoot() + "/level" + std::to_string(board->mLevel) + "_data.json";
```

（`:322` 的 `SaveJsonFile(filename, j)`、`:330` 的 `LoadJsonFile(filename, j)`、`:583` 的 `DeleteFile(filename)` 调用本身不变。）

- [ ] **Step 4：桌面等价性核对（这是组件 3 的主要验收——AutoTest 存档被短路，测不了）**

人工/grep 核对：桌面分支 `GetSaveRoot() == "./saves"`，故拼接结果逐字等于改造前——
- `"./saves" + "/PlayerInfo.json"` == `"./saves/PlayerInfo.json"` ✓
- `"./saves" + "/level" + N + "_data.json"` == `"./saves/levelN_data.json"` ✓
- `CreateDirectory("./saves")` == 原 ✓

确认 `GameInfoSaver.cpp` 内**不再有**字面量 `"./saves`（用 Grep 搜 `"\./saves`，应 0 命中）。

- [ ] **Step 5：构建验证**

跑【命令C：构建 clang-release】。预期：`Build succeeded`，**0 warning / 0 error**。

- [ ] **Step 6：冒烟（确认未破坏启动/游戏循环；存档逻辑在 AutoTest 下短路，此步只验不崩）**

跑【命令D】用 `smoke_gameplay`，preset `clang-release`。
预期：`$LASTEXITCODE` = 0；`run.log` 无新增 ERROR。

> 真机/真存档行为（桌面非 AutoTest 实际产生并读回存档）建议主人在桌面手动跑一次确认；本计划无法自动覆盖（AutoTest 存档短路）。

- [ ] **Step 7：提交**

```powershell
git add PlantVsZombies/GameInfoSaver.cpp
git commit -m @'
feat(xplat): 存档位置经 GetSaveRoot()(桌面 ./saves / Android prefPath)

GameInfoSaver 写死的 ./saves 全改为基于匿名命名空间 GetSaveRoot()：
桌面返回 "./saves"(拼接后逐字等价，行为不变)，Android 返回 SDL_GetPrefPath 私有可写目录。
写入仍用 std::ofstream(prefPath 为真实可写绝对路径)。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 6：收口完整性核验 + 全量回归

**Files:** 无代码改动（除非核验发现遗漏）。

- [ ] **Step 1：读取侧收口完整性 grep（组件 2d 验收点）**

用 Grep 在 `PlantVsZombies/` 下搜以下模式，确认**除 `Game/AutoTest/`（TestDriver 豁免）与 `CrashHandler.cpp`（_WIN32 内崩溃报告写入）外**无残留：
- `IMG_Load\(` （应只剩 `IMG_Load_RW`）
- `TTF_OpenFont\(` （应只剩 `TTF_OpenFontRW`）
- `Mix_LoadWAV\(` / `Mix_LoadMUS\(` （应只剩 `_RW` 版）
- 读取资源用的 `std::ifstream`（应只剩 TestDriver；FileManager 写入用 ofstream 不算）

若发现遗漏的资源读取点，按 Task 2-4 同法收编并补提交。

- [ ] **Step 2：权威构建（警告零基线）**

跑【命令A】+【命令C：构建 clang-release】全量。预期：**0 warning / 0 error**。
再跑【命令B：构建 msvc-debug】确认 debug 配置也通过。

- [ ] **Step 3：全量 AutoTest 回归**

跑【命令D】依次执行下列脚本（preset `clang-release`），逐一确认 `$LASTEXITCODE` = 0、Read 截图与改造前一致、`run.log` 无新增资源加载 ERROR：
- `demo_peashooter`（图/音/reanim）
- `smoke_small_sun`（夜间蘑菇资源）
- `almanac_click`（info.txt 文本）
- `smoke_gameplay`（综合渲染）
- `smoke_quit`（shader 启动）

- [ ] **Step 4：（无新代码则跳过提交）**

若 Step 1 有补漏改动，已在 Step 1 提交；本 Task 通常无独立提交。最终在响应中向主人汇报：构建 0 warning、各脚本退出码与截图核验结果。

---

## Self-Review（写计划后对照 spec 自检）

**1. Spec 覆盖：**
- 组件 1a/1b（CrashHandler）→ Task 1 ✓
- 组件 1c（main `system()`）→ Task 1 Step 3 ✓
- 组件 2a（FileManager RWops）→ Task 2 ✓
- 组件 2b（ResourceManager _RW）→ Task 3 ✓
- 组件 2c（shader + Almanac 收编）→ Task 4 ✓
- 组件 2d（收口完整性核验）→ Task 6 Step 1 ✓
- 组件 3（GetSaveRoot + 路径）→ Task 5 ✓
- 验证（clang-release 0 warning + AutoTest）→ 各 Task 构建/回归步骤 + Task 6 ✓
- TestDriver 豁免 → Task 6 Step 1 明确排除 ✓
无遗漏。

**2. Placeholder 扫描：** 无 TBD/TODO；所有代码步骤含完整代码；`org="PvZ"` 已给具体值（并注明第三阶段可调）。✓

**3. 类型/签名一致性：** `FileManager::LoadFileAsBinary` 返回 `std::vector<char>`（Task 2 定义，Task 4 `VulkanPipeline::LoadFile` 中 `out = LoadFileAsBinary(...)` 消费）；`LoadFileAsString` 返回 `std::string`（Task 2 定义，Task 4 Almanac、Task 5 间接 JSON 使用）；`GetSaveRoot()` 返回 `const std::string&`（Task 5 内自洽）。签名一致。✓
