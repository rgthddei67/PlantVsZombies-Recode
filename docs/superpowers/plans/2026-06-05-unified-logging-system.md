# 统一日志系统 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 引入一个流式、带级别、Release 编译期裁剪的统一日志系统，并把全部 ~297 处 `std::cout`/`std::cerr` 迁移过去。

**Architecture:** `Logger.h` 提供 `LOG_*` 宏（编译期级别地板 + 运行期软阈值 + RAII `LogStream` 流对象）；`Logger.cpp` 持有唯一共享状态（运行期级别 `std::atomic` + 写 `std::cout` 的 `std::mutex`）。宏用 glog 风格的 `cond ? (void)0 : LogVoidify() & LogStream(...)` 惯用法，使被裁剪级别的整个 `<< 表达式` 在编译期消除、不求值副作用。

**Tech Stack:** C++17、MSVC（VS 2026）、仅标准库（`<sstream> <iostream> <mutex> <atomic>`）。无新第三方依赖。

> **构建约定（CLAUDE.md）：** assistant **不自行构建**。每个"构建验证"步骤由主人在 Visual Studio 2026 中按 F7（Debug）/ 切 Release 重建并回贴结果。assistant 仅交付代码改动。

> **无自动化测试框架（CLAUDE.md 约束）：** 本项目没有单元测试设施且禁止 assistant 擅自搭建。因此本计划的"测试"= 主人在 VS 构建 + 肉眼确认控制台输出。设施任务含一个临时 smoke-test，迁移任务靠"构建通过 + 输出格式正确 + 无 `cout`/`cerr` 残留"验证。

---

## 文件结构

| 文件 | 责任 | 动作 |
|---|---|---|
| `PlantVsZombies/Logger.h` | `LogLevel` 枚举、`LOG_MIN_LEVEL` 编译期地板、`LogCompileEnabled`、`Logger` 静态接口声明、`LogStream` RAII 流、`LogVoidify`、`LOG_*` 宏 | 新建 |
| `PlantVsZombies/Logger.cpp` | 运行期级别（`std::atomic`）、`std::mutex`、`Logger::Write` 实际写 `std::cout`、`LevelName` | 新建 |
| `PlantVsZombies/PlantsVsZombies.vcxproj` | 把 Logger.h/.cpp 纳入工程（否则不参与编译/链接） | 改 |
| `PlantVsZombies/Reanimation/TodDebug.h` | `TOD_TRACE`/`TOD_TRACE_FORMAT` 重定向到 logger；保留 `TOD_ASSERT`/`TodErrorMessageBox` | 改 |
| 38 个调用方文件 | `cout`/`cerr` → `LOG_*`，加 `#include "Logger.h"`，删冗余 `#ifdef _DEBUG` | 改（分批） |

**关键设计点（实现者必须理解）：**

1. **枚举名用混合大小写 `Trace/Debug/Info/Warn/Error`，绝不用全大写 `ERROR`。** Windows `wingdi.h` 把 `ERROR` 定义成宏 `0`，`LogLevel::ERROR` 会被预处理成 `LogLevel::0` 直接编译失败。本项目含 Vulkan/SDL/Windows 头，必踩。
2. **编译期裁剪靠 `constexpr LogCompileEnabled` 短路**：`cond ? (void)0 : LogVoidify() & LogStream(...)`。`cond` 含编译期常量项，被裁级别在 `/O2` 下整条 dead-code 消除，`<< path` 等右侧表达式不求值。
3. **`operator&` 的优先级低于 `<<`、高于 `?:`**，所以 `LogVoidify() & LogStream() << a << b` 解析为 `LogVoidify() & ((LogStream()<<a)<<b)`，结果 void，与 `(void)0` 分支类型匹配。
4. **线程安全**：`LogStream` 把整条消息拼进自身 `ostringstream`，析构时一次性加锁写 `std::cout`，锁持有最短。

---

## Task 1: 创建 Logger 设施（Logger.h + Logger.cpp）

**Files:**
- Create: `PlantVsZombies/Logger.h`
- Create: `PlantVsZombies/Logger.cpp`

- [ ] **Step 1: 确认无宏名冲突**

Run（Grep 工具，pattern，glob `**/*.{h,cpp}`）：`\bLOG_(TRACE|DEBUG|INFO|WARN|ERROR)\b|\bLogStream\b|\bLogVoidify\b`
Expected: 仅匹配本计划将创建的文件（当前应为 0 匹配）。若已有同名宏/类，先在此处改名再继续。

- [ ] **Step 2: 写 `Logger.h`**

```cpp
#pragma once
#include <sstream>
#include <string>

// 级别枚举：混合大小写，规避 wingdi.h 的 ERROR 宏冲突
enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

// 编译期地板：Debug 全开；Release 只留 Warn/Error
#ifdef _DEBUG
  #define LOG_MIN_LEVEL ::LogLevel::Trace
#else
  #define LOG_MIN_LEVEL ::LogLevel::Warn
#endif

// 编译期常量：该级别是否参与编译
constexpr bool LogCompileEnabled(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(LOG_MIN_LEVEL);
}

// 唯一共享状态封装（全静态，状态实体在 Logger.cpp 的文件作用域）
class Logger {
public:
    static bool ShouldLog(LogLevel level);                                   // 运行期软阈值
    static void SetLevel(LogLevel level);                                    // 运行期下限设置
    static LogLevel GetLevel();
    static const char* LevelName(LogLevel level);
    static void Write(LogLevel level, const char* tag, const std::string& msg);
};

// RAII 流：拼接到自身 buffer，析构时一次性加锁写出
class LogStream {
public:
    LogStream(LogLevel level, const char* tag) : mLevel(level), mTag(tag) {}
    ~LogStream() { Logger::Write(mLevel, mTag, mBuffer.str()); }

    template <typename T>
    LogStream& operator<<(const T& value) { mBuffer << value; return *this; }

    // 支持 std::hex / std::setw 等 ostream 操纵符
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) { mBuffer << manip; return *this; }

private:
    LogLevel mLevel;
    const char* mTag;
    std::ostringstream mBuffer;
};

// 把 LogStream& 吃成 void，使三元两分支类型一致；operator& 优先级低于 <<
class LogVoidify {
public:
    void operator&(LogStream&) {}
};

#define LOG_AT(level, tag)                                                    \
    ( !::LogCompileEnabled(level) || !::Logger::ShouldLog(level) )            \
        ? (void)0                                                             \
        : ::LogVoidify() & ::LogStream((level), (tag))

#define LOG_TRACE(tag) LOG_AT(::LogLevel::Trace, tag)
#define LOG_DEBUG(tag) LOG_AT(::LogLevel::Debug, tag)
#define LOG_INFO(tag)  LOG_AT(::LogLevel::Info,  tag)
#define LOG_WARN(tag)  LOG_AT(::LogLevel::Warn,  tag)
#define LOG_ERROR(tag) LOG_AT(::LogLevel::Error, tag)
```

- [ ] **Step 3: 写 `Logger.cpp`**

```cpp
#include "Logger.h"
#include <iostream>
#include <mutex>
#include <atomic>

namespace {
    std::mutex g_logMutex;
    std::atomic<LogLevel> g_runtimeLevel{ LogLevel::Trace }; // 默认放行全部（编译期地板已先过滤）
}

bool Logger::ShouldLog(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(g_runtimeLevel.load(std::memory_order_relaxed));
}

void Logger::SetLevel(LogLevel level) {
    g_runtimeLevel.store(level, std::memory_order_relaxed);
}

LogLevel Logger::GetLevel() {
    return g_runtimeLevel.load(std::memory_order_relaxed);
}

const char* Logger::LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::Write(LogLevel level, const char* tag, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << '[' << LevelName(level) << "][" << tag << "] " << msg << '\n';
    if (level >= LogLevel::Warn) {
        std::cout.flush(); // Warn/Error 立即落屏，保证崩溃前可见；高频 Trace/Debug 不 flush 省开销
    }
}
```

- [ ] **Step 4: 提交（设施代码）**

```bash
git add PlantVsZombies/Logger.h PlantVsZombies/Logger.cpp
git commit -m "feat: 添加统一日志系统 Logger.h/.cpp"
```

---

## Task 2: 纳入工程 + smoke-test 验证（含 Release 裁剪确认）

**Files:**
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj`（约 line 27、line 106 附近）
- Modify: `PlantVsZombies/main.cpp`（临时 smoke 代码，本任务末尾移除）

- [ ] **Step 1: 把 Logger.cpp 加入 vcxproj 的 ClCompile 组**

在 `PlantsVsZombies.vcxproj` 中，`<ClCompile Include="GameMonitor.cpp" />`（约 line 27）之后插入一行：

```xml
    <ClCompile Include="Logger.cpp" />
```

- [ ] **Step 2: 把 Logger.h 加入 vcxproj 的 ClInclude 组**

在同文件 `<ClInclude Include="GameMonitor.h" />`（约 line 106）之后插入一行：

```xml
    <ClInclude Include="Logger.h" />
```

（`.vcxproj.filters` 只影响 Solution Explorer 分组，不影响构建；可选地在其 ClCompile/ClInclude 组各加一条同名 Include。）

- [ ] **Step 3: 在 `main.cpp` 顶部加临时 smoke 代码**

`#include "Logger.h"`（若未含），在 `main(` 函数体最前面插入：

```cpp
    // TEMP smoke test —— 验证后删除
    LOG_TRACE("Smoke") << "trace level " << 1;
    LOG_DEBUG("Smoke") << "debug level " << 2;
    LOG_INFO ("Smoke") << "info level "  << 3;
    LOG_WARN ("Smoke") << "warn level "  << 4;
    LOG_ERROR("Smoke") << "error level " << 5;
```

- [ ] **Step 4: 【主人】Debug 构建并运行，确认输出**

Run: 主人在 VS 选 Debug|x64，F7 构建，F5 运行，看控制台。
Expected（五行齐全、格式 `[LEVEL][tag] message`）：
```
[TRACE][Smoke] trace level 1
[DEBUG][Smoke] debug level 2
[INFO][Smoke] info level 3
[WARN][Smoke] warn level 4
[ERROR][Smoke] error level 5
```

- [ ] **Step 5: 【主人】Release 构建并运行，确认裁剪**

Run: 主人在 VS 选 Release|x64，**Rebuild**（Optimization 变更需全量重建），运行。
Expected（仅两行，TRACE/DEBUG/INFO 被编译期裁掉）：
```
[WARN][Smoke] warn level 4
[ERROR][Smoke] error level 5
```
若 Release 仍出现 INFO/DEBUG/TRACE → 编译期短路未生效，停下排查 `LogCompileEnabled`/宏展开，勿继续。

- [ ] **Step 6: 移除 smoke 代码**

删掉 Step 3 插入的临时块（保留 `#include "Logger.h"` 仅当 main.cpp 后续 Task 10 会用；否则一并删）。

- [ ] **Step 7: 提交**

```bash
git add PlantVsZombies/PlantsVsZombies.vcxproj PlantVsZombies/main.cpp
git commit -m "build: Logger 纳入工程并验证 Debug/Release 级别裁剪"
```

---

## 迁移配方（Tasks 3–14 共用，先读懂再执行各批）

每个迁移批次对每个文件执行以下机械变换。**逐处语义判断定级**，不要一刀切。

### A. 加头文件

文件顶部 include 区加：`#include "Logger.h"`（按该文件 include 风格用相对或项目根路径；多数文件与 `iostream` 同处即可）。若该文件迁移后不再用到 `<iostream>`，可一并移除其 `#include <iostream>`（不确定就留着，无害）。

### B. 定级决策表

| 现状特征 | 目标级别 |
|---|---|
| `cerr` + “失败/无法/错误/error/失效/未初始化/空指针” | `LOG_ERROR` |
| `cerr`/`cout` + “警告/warning/警告:” 或“可恢复、已回退、用默认值” | `LOG_WARN` |
| `cout` 一次性里程碑：启动完成、资源加载完毕、场景切换、存档成功 | `LOG_INFO` |
| `cout` 开发期诊断、状态打印（非逐帧） | `LOG_DEBUG` |
| `cout` 逐帧/逐对象/高频（如每 N 帧打印、循环内打印） | `LOG_TRACE` |

### C. 行变换（流式，承接现有 `<<`）

- 取出消息里现有的 `[Class]` 或 `[Class::Method]` 方括号前缀作为 **tag**（取类名粒度，方法名若有用留在消息体）。无前缀的，用所在类/文件的子系统名当 tag。
- 去掉行尾 `<< std::endl;`（或 `<< "\n"`）——`LogStream` 析构自动换行。
- `std::cout`/`std::cerr` 头替换成 `LOG_级别("tag")`。

真实示例（取自当前代码）：

```cpp
// 之前 (ResourceManager.cpp:44)
std::cerr << "[ResourceManager::LoadTexture] 无法加载图片: " << filepath << " - " << IMG_GetError() << std::endl;
// 之后
LOG_ERROR("ResourceManager") << "LoadTexture 无法加载图片: " << filepath << " - " << IMG_GetError();

// 之前 (ResourceManager.cpp:127) —— “警告:”
std::cerr << "警告: 图片尺寸 " << imgW << "x" << imgH ...
// 之后
LOG_WARN("ResourceManager") << "图片尺寸 " << imgW << "x" << imgH ...

// 之前 (ResourceManager.cpp:92) —— 已包 #ifdef _DEBUG 的诊断
#ifdef _DEBUG
std::cout << "卸载纹理: " << key << std::endl;
#endif
// 之后（删掉 #ifdef，级别本身决定可见性）
LOG_DEBUG("ResourceManager") << "卸载纹理: " << key;

// 之前 (Board.cpp:119) —— 逻辑错误提示
std::cout << "无效的行列位置: (" << row << ", " << column << ")" << std::endl;
// 之后
LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << column << ")";
```

### D. 清理

- 删除随之变冗余的 `#ifdef _DEBUG ... #endif` 包裹（仅当其内**只**含被迁移的日志时；若还含别的代码，只删日志行、留 ifdef）。
- 删除注释掉的死日志，如 `// std::cout << ...;`。

### E. 每批次收尾

- Run（Grep 工具）：在本批次文件上搜 `std::cout|std::cerr`，Expected: 0 残留。
- 【主人】VS Debug 构建通过、跑一下相关场景，输出格式正确、无中文乱码（`/utf-8` 已开）。
- 提交：`git commit -m "refactor(log): 迁移 <子系统> 到统一日志"`。

---

## Task 3: 重定向 TodDebug.h

**Files:**
- Modify: `PlantVsZombies/Reanimation/TodDebug.h`

- [ ] **Step 1: 重写宏，重定向到 logger**

把现有 `TOD_TRACE`/`TOD_TRACE_FORMAT` 改为转发到 `LOG_DEBUG`，保留 `TOD_ASSERT` 与 `TodErrorMessageBox`：

```cpp
#pragma once

#ifndef _TODDEBUG_H
#define _TODDEBUG_H
#include <cassert>
#include <cstdio>
#include "../Logger.h"

#define TOD_ASSERT(expr) assert(expr)

// 重定向到统一日志（Reanim 子系统，Debug 级）
#define TOD_TRACE(msg) LOG_DEBUG("Reanim") << msg

// 保留 printf 风格语义，但走 logger 缓冲：格式化到栈缓冲再交给 LOG_DEBUG
#define TOD_TRACE_FORMAT(fmt, ...)                                  \
    do {                                                            \
        char _todBuf[512];                                          \
        std::snprintf(_todBuf, sizeof(_todBuf), fmt, __VA_ARGS__);  \
        LOG_DEBUG("Reanim") << _todBuf;                             \
    } while (0)

inline void TodErrorMessageBox(const char* message, const char* title) {
    LOG_ERROR("Reanim") << title << ": " << message;
}

#endif
```

- [ ] **Step 2: 收尾（按"迁移配方 §E"）** — Grep `TOD_TRACE` 找调用点确认仍编译；【主人】Debug 构建通过；提交 `refactor(log): TodDebug 重定向到统一日志`。

---

## Task 4: 迁移 UI 批（最小热身，3 处）

**Files:** `UI/Button.cpp`(1)、`UI/Slider.cpp`(1)、`UI/GameMessageBox.cpp`(1)

- [ ] **Step 1:** 对三个文件套用"迁移配方 A–D"，tag 用 `"UI"`。
- [ ] **Step 2:** 收尾（迁移配方 §E）。提交 `refactor(log): 迁移 UI 到统一日志`。

---

## Task 5: 迁移 Reanimation 批（10 处）

**Files:** `Reanimation/Animator.cpp`(5)、`Reanimation/Reanimation.cpp`(3)、`Reanimation/AttachmentSystem.cpp`(2)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"Reanim"`。注意 `Animator`/`Reanimation` 中循环内或逐帧打印 → `LOG_TRACE`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Reanimation 到统一日志`。

---

## Task 6: 迁移 ParticleSystem 批（11 处）

**Files:** `ParticleSystem/ParticleXMLLoader.cpp`(9)、`ParticleSystem/ParticleConfig.cpp`(1)、`ParticleSystem/ParticleSystem.cpp`(1)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"Particle"`。XMLLoader 中“解析失败/找不到节点”→ `LOG_ERROR`/`LOG_WARN`；已有 `#ifdef _DEBUG` 包裹的解析日志 → `LOG_DEBUG` 并删 ifdef。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 ParticleSystem 到统一日志`。

---

## Task 7: 迁移 Renderer 批（26 处）

**Files:** `Renderer/VulkanContext.cpp`(12)、`Renderer/VulkanRenderer.cpp`(5)、`Renderer/VulkanPipeline.cpp`(4)、`Renderer/VulkanTexturePool.cpp`(4)、`Renderer/VulkanBuffer.cpp`(1)

- [ ] **Step 1:** 套用配方 A–D，tag 用各自类名（`"VulkanContext"` 等）。Vulkan 校验/创建失败 → `LOG_ERROR`；设备能力/扩展枚举等启动信息 → `LOG_INFO`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Renderer 到统一日志`。

---

## Task 8: 迁移 Resource 批（47 处）

**Files:** `ResourceManager.cpp`(38)、`ResourcesXMLConfigReader.h`(9)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"ResourceManager"`。参照"配方 §C"里的 4 个真实示例（均出自本文件）。`ResourcesXMLConfigReader.h` 是头文件：迁移后确保 `#include "Logger.h"`（注意相对路径 = 同目录 `"Logger.h"`），并把内联日志同样分级。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 ResourceManager 到统一日志`。

---

## Task 9: 迁移 Graphics 批（32 处）

**Files:** `Graphics.cpp`(32)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"Graphics"`。Graphics 含 worker 线程并行路径（见 memory：parallel Draw）——**确认迁移的日志确实是同步路径或已在 `tl_record` guard 之外**；逐帧/overflow 警告（如 `Worker slot N overflowed slice`）→ `LOG_WARN`，缓冲耗尽 → `LOG_ERROR`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Graphics 到统一日志`。

---

## Task 10: 迁移 Bootstrap/App 批（32 处）

**Files:** `main.cpp`(2)、`GameApp.cpp`(19)、`CrashHandler.cpp`(5)、`CursorManager.cpp`(5)、`FileManager.cpp`(1)

- [ ] **Step 1:** 套用配方 A–D。tag 用 `"GameApp"`/`"CrashHandler"`/`"CursorManager"`/`"FileManager"`。注意 `GameApp.cpp:370` 附近“每 40 帧打印鼠标坐标”→ `LOG_TRACE`；`GameApp.cpp:110/356` 的 `#ifdef _DEBUG` 块迁移后删 ifdef。CrashHandler 的崩溃信息 → `LOG_ERROR`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Bootstrap/App 到统一日志`。

---

## Task 11: 迁移 Game 核心批（18 处）

**Files:** `Game/Board.cpp`(5)、`Game/GameObjectManager.cpp`(1)、`Game/GameScene.cpp`(3)、`Game/SceneManager.cpp`(3)、`Game/Scene.cpp`(4)、`Game/GameProgress.cpp`(2)

- [ ] **Step 1:** 套用配方 A–D，tag 用各自类名。`Board.cpp` 的“无效行列/对象池未初始化”→ `LOG_ERROR`（见配方 §C 示例）；`GameProgress.cpp:40` 的 `#ifdef _DEBUG` 块删 ifdef。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Game 核心到统一日志`。

---

## Task 12: 迁移 Game 组件批（28 处）

**Files:** `Game/AudioSystem.cpp`(6)、`Game/ObjectPool/BulletPool.cpp`(9)、`Game/CardComponent.cpp`(3)、`Game/CardDisplayComponent.cpp`(3)、`Game/CardSlotManager.cpp`(3)、`Game/ShadowComponent.cpp`(3)、`Game/AnimatedObject.cpp`(1)

- [ ] **Step 1:** 套用配方 A–D，tag 用各自类名。`CardComponent.cpp:200` 的 `#ifdef _DEBUG` 删 ifdef。BulletPool 池满/复用诊断 → `LOG_DEBUG`/`LOG_TRACE`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 Game 组件到统一日志`。

---

## Task 13: 迁移 GameDataManager（18 处）

**Files:** `Game/Plant/GameDataManager.cpp`(18)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"GameData"`。“未注册的植物/僵尸类型”等工厂派发失败（见 memory factory-registry）→ `LOG_ERROR`；注册条目数等启动信息 → `LOG_INFO`。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 GameDataManager 到统一日志`。

---

## Task 14: 迁移 GameMonitor（70 处，最大单文件）

**Files:** `GameMonitor.cpp`(70)

- [ ] **Step 1:** 套用配方 A–D，tag 用 `"GameMonitor"`。此文件日志量最大、多为状态/统计打印——绝大多数应是 `LOG_DEBUG`/`LOG_TRACE`（监控诊断），仅真实异常用 WARN/ERROR。分级时尤其小心别把高频统计标成 INFO（Release 虽裁掉，但 Debug 会刷屏）。
- [ ] **Step 2:** 收尾（§E）。提交 `refactor(log): 迁移 GameMonitor 到统一日志`。

---

## Task 15: 全局收尾与验收

**Files:** 无新增（核对 + 文档）

- [ ] **Step 1: 确认零残留**

Run（Grep 工具，glob `**/*.{cpp,h}`）：`std::cout|std::cerr`
Expected: 仅 `PlantVsZombies/Logger.cpp` 内部那两处 `std::cout` 匹配，其余全为 0。若别处仍有，回到对应批次补迁。

- [ ] **Step 2: 确认 `#ifdef _DEBUG` 仅剩非日志用途**

Run（Grep）：`#ifdef _DEBUG` —— 人工核对剩余的都不再是"为静音日志而包"的（应只剩真正的调试专属逻辑，如 `-Debug` hitbox 之类）。

- [ ] **Step 3: 【主人】Debug + Release 全量重建并通历各主场景**

Expected: 两配置均构建通过；Debug 控制台五级输出格式统一；Release 仅 WARN/ERROR；无中文乱码；游戏行为无回归。

- [ ] **Step 4: 更新 memory backlog**

在 `project_pvz_improvement_backlog.md` 标记 #4 完成（仿照 #1 工厂的“已完成”写法），并新建一条 `project_pvz_logging_system.md` 记录最终 API/约定（tag 列表、级别语义、Release 裁剪行为、扩展点=Logger.cpp 单点加 sink），在 `MEMORY.md` 加索引行。

- [ ] **Step 5: 收束分支**

调用 superpowers:finishing-a-development-branch，向主人给出合并/PR/清理选项。

---

## 自检（spec 覆盖）

- spec §2 全量迁移 → Tasks 3–14 覆盖全部 38 文件 / ~297 处；Task 15 Step 1 验证零残留 ✓
- spec §4.2 API（流式、无 endl、`[LEVEL][tag]` 格式）→ Task 1 Logger.h/.cpp + Task 2 验证 ✓
- spec §4.3 双层裁剪（编译期 + 运行期）→ Task 1 `LOG_MIN_LEVEL`/`LogCompileEnabled` + `Logger::ShouldLog`/`SetLevel`；Task 2 Step 5 验证 Release 裁剪 ✓
- spec §4.4 线程安全 → Task 1 `LogStream` 缓冲 + `Logger::Write` mutex ✓
- spec §5 TodDebug.h 处理 → Task 3 ✓
- spec §6 逐处语义分级 + 清理 #ifdef/死日志 → 迁移配方 §B/§D，各批 ✓
- spec §7 非目标（仅控制台、无文件 sink/颜色/异步）→ 计划未引入，扩展点记于 Task 15 Step 4 ✓
- spec §8 验收 → Task 15 ✓
- 类型一致性：`LogLevel`/`Logger::{ShouldLog,SetLevel,GetLevel,LevelName,Write}`/`LogStream`/`LogVoidify`/`LOG_*` 全程一致；枚举混合大小写避 `ERROR` 宏冲突 ✓
