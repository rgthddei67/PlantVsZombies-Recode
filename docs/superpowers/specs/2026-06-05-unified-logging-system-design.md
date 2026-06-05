# 统一日志系统 设计文档

- 日期：2026-06-05
- 状态：设计已确认，待写实现计划
- 背景：backlog #4（见 memory `project_pvz_improvement_backlog`）

## 1. 目标与动机

当前项目约 **297 处 `std::cout` / `std::cerr`** 散落在 **38 个文件**，存在以下问题：

- **无级别**：错误、警告、诊断噪音混在一起，无法区分轻重。
- **无统一分类**：大量 `cerr` 已手写 `[ClassName::Method]` 前缀，但靠手工拼接，不成体系。
- **Release 无法静音**：要让某条日志在 Release 闭嘴，得逐处包 `#ifdef _DEBUG`（项目里已有十几处这种手工包裹），易漏、难维护。
- 已存在的 `Reanimation/TodDebug.h` 仅有极简 `TOD_TRACE`/`TOD_ASSERT`，无级别、无裁剪。

目标：引入一个轻量、流式、带级别、Release 可编译期裁剪的统一日志系统，并把全部 297 处迁移过去。

## 2. 范围

- **全量迁移**：建好日志设施 + 把全部 297 处 `cout`/`cerr` 改为新宏。
- **逐处语义分级**：按每处调用的真实语义定级（不做一刀切机械映射）。
- 不引入新第三方依赖（仅标准库 + 现有构建）。
- 仅控制台输出（文件 / OutputDebugString sink 留作未来扩展，本期不做）。

## 3. 关键设计决策

| 决策点 | 选择 | 理由 |
|---|---|---|
| 范围 | 设施 + 全量迁移 | 一步到位，彻底消除 `cout`/`cerr` 与手工 `#ifdef` |
| API 风格 | 流式 `LOG_LEVEL("tag") << ...` | 承接现有 `<<` 拼接习惯，迁移阻力最小；无需 `.c_str()` / 格式串 |
| 级别集 | TRACE / DEBUG / INFO / WARN / ERROR | 覆盖从逐帧诊断到失败的全谱 |
| Release 行为 | 仅 WARN/ERROR 可见，TRACE/DEBUG/INFO 编译期裁掉 | 兑现"Release 静音"，零运行期开销 |
| 运行期过滤 | 额外加 `Logger::SetLevel()` 软阈值 | Debug 构建里临时压噪音不必重编译 |
| 输出去向 | 仅控制台（`std::cout`） | 最简、零新依赖；文件 sink 留待未来 |
| 实现形态 | `Logger.h`（宏 + RAII 流类） + `Logger.cpp`（单例状态 + mutex） | 唯一共享状态集中在一个 TU，避免 inline 变量隐患 |
| 线程安全 | 线程局部拼接 + 析构时单次加锁写 | 项目有 Worker 线程并行 Draw/Update，多线程 `<<` 会交错乱码 |
| 分级策略 | 逐处语义判断 | 机械映射会让高频噪音淹没 Debug 输出、误升 WARN 为 ERROR |

## 4. 架构

### 4.1 文件

- `PlantVsZombies/Logger.h`：`LogLevel` 枚举、`LOG_*` 宏、`LogStream` RAII 类声明、编译期 `LOG_MIN_LEVEL`。
- `PlantVsZombies/Logger.cpp`：`Logger` 单例（全局运行期级别阈值 + `std::mutex`）、实际写 `std::cout` 的实现。

放在 `PlantVsZombies/` 根目录，与 `Profiler.h`、`GameApp.cpp` 同级。

### 4.2 公共 API

```cpp
enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR };

LOG_TRACE("tag") << "...";   // 最细：逐帧 / 逐对象诊断
LOG_DEBUG("tag") << "...";   // 开发期诊断（高频但有用）
LOG_INFO ("tag") << "...";   // 启动 / 里程碑 / 资源加载完成
LOG_WARN ("tag") << "...";   // 可恢复异常（如"警告: 图片尺寸…"）
LOG_ERROR("tag") << "...";   // 失败 / 错误（加载失败、空指针）
```

- `"tag"`：子系统/类名短串（如 `"ResourceManager"`、`"Board"`、`"Reanim"`），承接现有 `[Class]` 前缀习惯。tag 取**类名粒度**，方法名若有意义放进消息体。
- **无 `std::endl`**：`LogStream` 析构时自动补 `\n`。
- 输出格式：`[LEVEL][tag] message`，例如 `[ERROR][ResourceManager] 加载失败: x.png`。

### 4.3 级别裁剪（双层）

**编译期硬地板**（`Logger.h`）：

```cpp
#ifdef _DEBUG
  #define LOG_MIN_LEVEL LogLevel::TRACE   // Debug：全开
#else
  #define LOG_MIN_LEVEL LogLevel::WARN    // Release：只剩 WARN/ERROR
#endif
```

宏内先做**编译期常量短路**：当某级别 `< LOG_MIN_LEVEL` 时，整个 `LOG_X(...) << expr` 表达式被编译器优化消除——后续 `<< path` 的字符串拼接、函数调用都不进二进制。实现用 `if constexpr` 或等价的编译期常量分支保证零开销，且不求值 `<<` 右侧的副作用表达式。

**运行期软阈值**：`Logger::SetLevel(LogLevel)` 设全局运行期下限，在编译地板之上再过滤。主要服务 Debug 构建临时压噪音。Release 下它处于编译地板之上，无实际副作用。

### 4.4 线程安全

- `LogStream` 内部持有一个（线程局部生命周期的）`std::ostringstream`，把整条消息先拼好。
- 析构时：判运行期级别 → 通过则 `std::lock_guard` 锁 `Logger` 单例的 `std::mutex` → 一次性写完整行 + `\n` 到 `std::cout`。
- 锁持有时间 = 仅一次写，最短。Worker 线程上的 DEBUG/TRACE 在 Release 已被编译期裁掉，热路径无锁竞争。

## 5. `TodDebug.h` 处理

- **保留** `TOD_ASSERT`（断言，与日志正交）。
- **保留** `TodErrorMessageBox`（弹框，非纯日志）。
- **重定向** `TOD_TRACE(msg)` → `LOG_DEBUG("Reanim") << msg`；`TOD_TRACE_FORMAT` → 等价 `LOG_DEBUG`（格式化转流式或保留 printf 语义封进 logger）。保持 PvZ 原版命名兼容，现有调用点不必改。

## 6. 迁移计划（297 处 / 38 文件）

逐处语义判断，分级映射基线：

- `cerr` + "失败 / 无法 / 错误" → `ERROR`
- `cerr` + "警告:" → `WARN`
- `cout` 高频诊断（卸载纹理、每 N 帧打印鼠标坐标等） → `DEBUG` 或 `TRACE`
- `cout` 启动 / 里程碑 / 一次性完成提示 → `INFO`

附带清理：

- 删除随之冗余的 `#ifdef _DEBUG` 包裹（级别替代了它）。
- 现有 `[Class::Method]` 前缀转成 tag 参数，消息体去掉重复方括号前缀。
- 清掉注释掉的死日志（`// std::cout ...`）。

**分批交付**：每批数个文件，用户在 VS 构建验证后再继续，避免一次性 38 文件巨型 diff 难以审查。

## 7. 非目标（YAGNI）

- 文件 sink / 日志轮转 / OutputDebugString sink —— 未来需要再加（架构预留：输出集中在 `Logger.cpp` 单点，加 sink 不动调用方）。
- ANSI 颜色（VS 输出窗口对 ANSI 支持不佳，不做）。
- 异步日志队列（控制台 + 短锁已够，过度设计）。
- 结构化日志 / JSON 输出。

## 8. 验收

- Debug 构建：五级全部可见，格式 `[LEVEL][tag] message`。
- Release 构建：仅 WARN/ERROR 可见；TRACE/DEBUG/INFO 经反汇编/行为确认零残留（其右侧表达式不求值）。
- 全部 297 处 `cout`/`cerr` 迁移完毕（保留 `Logger.cpp` 内部对 `std::cout` 的唯一使用）。
- 多线程场景日志不交错乱码。
- 构建通过（用户在 VS 2026 验证，遵循 CLAUDE.md：assistant 不自行构建）。
