---
name: project_pvz_logging_system
description: "统一日志系统 Logger.h/.cpp 的 API/级别语义/Release 裁剪/扩展点；backlog #4 的成品"
metadata:
  node_type: memory
  type: project
  originSessionId: 33349dcd-4dd0-4875-b43e-05d0b05f537c
---

2026-06-06 完成（branch `feature/unified-logging-system`，15 commits）。backlog #4 的成品，见 [project_pvz_improvement_backlog](project_pvz_improvement_backlog.md)。用 superpowers:subagent-driven-development 执行 `docs/superpowers/plans/2026-06-05-unified-logging-system.md`。

## API
- `Logger.h`（项目根 `PlantVsZombies/`）：`LOG_TRACE/LOG_DEBUG/LOG_INFO/LOG_WARN/LOG_ERROR("tag") << ...;`
- 流式，**不要写 `std::endl`**——`LogStream` 析构自动补 `\n`。输出格式 `[LEVEL][tag] message`。
- tag = 类名/子系统短串（"ResourceManager"/"Board"/"Reanim"/"VulkanContext"/"GameData" 等），方法名放进消息体。
- 实现：`LogStream` 拼进自身 `ostringstream`，析构时 `Logger::Write` 一次性加锁写 `std::cout`（线程安全，worker 线程可直接打）。`Logger::Write` 对 ≥Warn 才 flush。

## 级别语义（逐处语义判断，非机械映射）
- ERROR=失败/返回错误/abort（未注册/找不到/无法/未初始化/空指针）；WARN=可恢复降级（"警告"、回退默认、overflow 丢数据但继续渲染）；INFO=一次性里程碑（设备选中、场景切换、存档、资源计数汇总）；DEBUG=事件级诊断（加载/卸载/注册/track 查询）；TRACE=逐帧/逐对象热循环（仅 GameApp 每40帧鼠标坐标一处）。
- **TRACE vs DEBUG 区别只在 Debug 构建有意义**（Release 两者都被裁）；价值在 `Logger::SetLevel(LogLevel::Debug)` 运行期软阈值——不重编译即可单独静音帧噪音 TRACE。

## 双层裁剪
- 编译期地板 `LOG_MIN_LEVEL`：Debug=Trace（全开），Release=Warn。被裁级别整条 `<<` 表达式 dead-code 消除、右侧不求值、零开销。靠 `cond ? (void)0 : LogVoidify() & LogStream(...)` 惯用法（`operator&` 优先级低于 `<<`、高于 `?:`）。
- 运行期软阈值 `Logger::SetLevel()`（`std::atomic<LogLevel>`），在编译地板之上再过滤。
- **枚举混合大小写 `Trace/Debug/Info/Warn/Error`，绝不用全大写 `ERROR`**——`wingdi.h` 把 `ERROR` 定义成宏 0，会编译失败。

## 关键约束/陷阱（改这套时必看）
- include 深度按文件位置：根 `"Logger.h"`、`Game/` `"../Logger.h"`、`Game/ObjectPool/`+`Game/Plant/` `"../../Logger.h"`、`Reanimation/` `"../Logger.h"`。
- `AttachmentSystem.cpp` 是 **GBK/ANSI 编码**（注释 mojibake），改它只碰 ASCII 行、改完 `git diff` 必须只显示意图行，否则整文件被重编码为 UTF-8 会毁掉中文注释。
- 删 `<iostream>` 的雷：若文件曾靠 iostream 间接获得别的符号会编译报错；`Logger.h` 自带 `<string>/<sstream>` 兜住 std::string。修法=把 iostream 加回去。
- `TodDebug.h` 的 `TOD_TRACE`/`TOD_TRACE_FORMAT` 已重定向到 `LOG_DEBUG("Reanim")`；`TOD_ASSERT`/`TodErrorMessageBox` 保留。
- Renderer 子系统原本用 `std::fprintf(stderr/stdout)` 不是 cout/cerr（VK_CHECK 宏体也是）——已一并迁移。

## 排除/未迁移（不是遗漏）
- `GameMonitor.cpp`(70 处)：**主人指示不迁移**（基本不用）。验收 grep 必须显式排除它。
- `Logger.cpp` 内 2 处 `std::cout`：合法 sink。
- `Profiler.h`(5 std::printf 性能 dump)、`CrashHandler.cpp` `printf("\a")`（响铃）：非日志，本就不在范围。

## 扩展点
- 加文件 sink / OutputDebugString / 颜色：只改 `Logger.cpp::Write` 单点，不动 ~220 个调用方。设计 §7 YAGNI 留白。
