# AGENTS.md

本文件包含 Codex 在本仓库中始终生效的规则。保持本文件精简；详细说明只通过下方路由按需读取。

## 任务路由

- 构建、运行、使用 AutoTest，或修改架构、资源、存档行为前，先阅读 `docs/agent-guide/PROJECT_GUIDE.md` 中的相关章节。
- 涉及现有子系统或历史决策时，先搜索 `docs/agent-memory/MEMORY.md`，只读取与当前任务匹配的主题文件。记忆属于历史上下文：其中带日期的状态、路径、提交和测试结论必须根据当前仓库重新核实。
- 涉及任何植物、粒子特效、生存模式词条或僵尸时，必须使用 `.agents/skills/` 下对应的技能，并完整遵循其 `SKILL.md`。
- 新增经典植物、僵尸或子弹前，先查阅 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`。动画出问题时，同时检查对应的 `resources/reanim/` 文件和 C# 参考实现。
- 新工作需要添加动画帧事件时，必须先询问主人。

## 构建与验证

- 本项目是面向 x64 Windows 的 C++17 CMake/vcpkg 项目。Codex 可以自主构建。
- 运行 CMake 前，先把 Visual Studio Installer 目录加入 `PATH`，用 `vswhere` 定位 VS，再导入 `VsDevCmd.bat -arch=x64 -no_logo`；准确的 PowerShell 步骤见项目指南。
- Release 验证依次运行 `cmake --preset clang-release` 和 `cmake --build --preset clang-release`。调试/F5 使用 `msvc-debug`；不存在 MSVC Release 预设。
- 必须从 `build\<preset>\` 运行；可执行文件为 `build\<preset>\PlantsVsZombies.exe`。禁止使用根目录下陈旧的 `x64\Release` 产物。
- 修改游戏逻辑后，从构建目录运行范围最小且相关的 `-AutoTest` 脚本，并按需检查 `run.log`、状态文件和截图。仅修改文档时无需构建游戏。

## 仓库规则

- 源文件由 `GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集；新增 `.cpp` 无需手动修改构建列表。
- 每个新 `.h` 必须以 `#pragma once` 开头；pre-commit hook 会自动检查。
- 中文文本保持 UTF-8。逻辑网格位置与视觉偏移（`mVisualOffset`）必须分离。
- 当前任务指令、当前源码/Git 状态和当前构建/测试证据优先于历史记忆。
- 对已记录的子系统做出实质修改后，更新对应主题文件及 `docs/agent-memory/MEMORY.md` 中的条目。

## Git 与沟通

- Codex 对已完成且验证通过的工作进行提交，然后根据当前风险和仓库状态决定是否 push。
- 工作已完成并验证、改动范围与任务一致、目标上游明确，且可常规 fast-forward 时执行 push；否则保留本地提交并说明原因。
- 未经明确批准，不得 force-push、改写已发布历史，或发布无关/敏感改动。主人的明确指令始终优先。
- 始终称呼用户为 **主人**。
