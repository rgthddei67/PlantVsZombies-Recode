# AGENTS.md

本文件包含 Codex 在本仓库中始终生效的规则。保持本文件精简；详细说明只通过下方路由按需读取。

## 任务路由

- 构建、运行、使用 AutoTest，或修改架构、资源、存档行为前，先阅读 `docs/agent-guide/PROJECT_GUIDE.md` 中的相关章节。
- 涉及现有子系统或历史决策时，先搜索 `docs/agent-memory/MEMORY.md`，只读取与当前任务匹配的主题文件。记忆属于历史上下文：其中带日期的状态、路径、提交和测试结论必须根据当前仓库重新核实。
- 涉及任何植物、粒子特效、生存模式词条或僵尸时，必须使用 `.agents/skills/` 下对应的技能，并完整遵循其 `SKILL.md`。
- 涉及雨天天气本身，或任何按小/中/大雨生效的能力、变异、条件生成与系统联动时，必须使用 `.agents/skills/adding-rain-weather/SKILL.md`。
- 新增经典植物、僵尸或子弹前，先查阅 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`。动画出问题时，同时检查对应的 `resources/reanim/` 文件和 C# 参考实现。
- 新工作需要添加动画帧事件时，必须先询问主人。

## 构建与验证

- 本项目是面向 x64 Windows 的 C++17 CMake/vcpkg 项目。Codex 可以自主构建。
- 主人已长期授权本项目正常构建所需的 vcpkg 依赖安装、CMake 配置/生成和编译；若沙箱阻止写入工作区外的 vcpkg 目录，直接申请提升权限执行，无需再次询问是否允许构建。该授权不包含删除 vcpkg、清空缓存或其他破坏性操作。
- CMake 已加入系统 `PATH`，直接使用 `cmake` 命令，不要再定位或硬编码 Visual Studio 自带的 `cmake.exe`。运行 CMake 前仍需先把 Visual Studio Installer 目录加入 `PATH`，用 `vswhere` 定位 VS，再导入 `VsDevCmd.bat -arch=x64 -no_logo`；准确的 PowerShell 步骤见项目指南。
- Release 验证依次运行 `cmake --preset clang-release` 和 `cmake --build --preset clang-release`。调试/F5 使用 `msvc-debug`；不存在 MSVC Release 预设。
- 必须从 `build\<preset>\` 运行；可执行文件为 `build\<preset>\PlantsVsZombies.exe`。禁止使用根目录下陈旧的 `x64\Release` 产物。
- Codex 启动任何需要主人看到的游戏或 AutoTest 窗口时，必须以 `build\<preset>\` 为工作目录，通过申请 `sandbox_permissions="require_escalated"` 的 shell 使用 `Start-Process -WindowStyle Normal -PassThru` 启动到主人当前桌面；普通沙箱 shell 即使指定 Normal 也不算可见运行。完整命令见项目指南。
- 修改游戏逻辑后，从构建目录运行范围最小且相关的 `-AutoTest` 脚本。AutoTest 默认用主人当前桌面可见的游戏窗口依次运行（不得默认隐藏或仅后台执行），并同时检查退出码、`run.log`、状态文件和截图；只有主人明确要求后台运行或执行环境确实无法显示窗口时才可例外，并须说明。仅修改文档时无需构建游戏。

## 仓库规则

- 源文件由 `GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集；新增 `.cpp` 无需手动修改构建列表。
- 每个新 `.h` 必须以 `#pragma once` 开头；pre-commit hook 会自动检查。
- 中文文本保持 UTF-8。逻辑网格位置与视觉偏移（`mVisualOffset`）必须分离。
- 当前任务指令、当前源码/Git 状态和当前构建/测试证据优先于历史记忆。
- 对已记录的子系统做出实质修改后，更新对应主题文件及 `docs/agent-memory/MEMORY.md` 中的条目。

## 代码注释

- 以 `Graphics.h/.cpp` 的现有风格为参考：头文件注释说明接口契约，函数体注释说明实现意图与关键约束。
- 每个新增或实质修改的函数都必须评估是否需要函数级功能注释；非平凡函数必须简要说明“做什么”。公共接口优先使用 Doxygen，并仅在语义不直观时补充参数、返回值、单位、范围、副作用或生命周期。自解释的一行 getter/setter、简单转发和显然的重写可省略。
- 函数内部应在关键逻辑块前适量写注释，重点解释“为什么这样做”，包括算法阶段、特殊分支、边界条件、状态或调用顺序、线程安全、所有权、坐标/单位转换，以及性能快慢路径；不要逐行翻译代码或重复变量名已经表达的内容。
- 在 `namespace`（尤其匿名命名空间）中集中声明的可调常量、权重、时间、倍率等参数，必须在声明行末接中文注释，说明用途、单位或调整含义，方便后期集中修改；纯实现细节且无调参含义的常量可按需注释。
- 注释必须与当前实现同步；修改行为时同时更新或删除失效注释。中文注释保持 UTF-8，代码标识符和必要术语保留原名以便搜索。

## Git 与沟通

- Codex 对已完成且验证通过的工作进行提交，然后根据当前风险和仓库状态决定是否 push。
- 工作已完成并验证、改动范围与任务一致、目标上游明确，且可常规 fast-forward 时执行 push；否则保留本地提交并说明原因。
- 未经明确批准，不得 force-push、改写已发布历史，或发布无关/敏感改动。主人的明确指令始终优先。
- 始终称呼用户为 **主人**。
