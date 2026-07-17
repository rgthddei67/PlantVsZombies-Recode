---
name: build-permission-msbuild
description: 2026-06-12 主人解除构建限制 — 允许 Claude 直接用 MSBuild 命令行编译，无需主人 VS F7
metadata:
  node_type: memory
  type: feedback
  originSessionId: 69e31e04-74c3-4128-8db4-89c425da06e5
---

2026-06-12 主人明确授权："把限制删了吧，不要查看时间了，我的电脑还可以，构建全部项目也只用30s"。

**Why:** 为了自动化开发套件（autotest）实现「改代码→编译→跑测试→看截图」全自动闭环，主人不想再当人肉编译节点。机器够快（全量构建约 30s）。

**How to apply:**
- 可以直接 `msbuild PlantsVsZombies.sln /p:Configuration=Release /p:Platform=x64`（或 Debug）编译，不必请主人 F7。
- 不必再交叉核对输出二进制时间戳（旧 MCP build_status 注意事项作废）。
- MSVC-Debug-MCP 的 `build_solution`/`build_project`/`clean_solution` 是否仍禁用以 CLAUDE.md 当前文本为准（CLAUDE.md 待同步更新）。
- 取代 [collaboration-style](feedback_collaboration_style.md) 中"user builds in VS"的旧惯例。
