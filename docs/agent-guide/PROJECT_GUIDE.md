# PlantVsZombies 项目指南

[返回文档导航](../README.md) · [核心系统阅读地图](../systems/README.md)

本页只负责按任务路由。始终生效的规则见根目录 [AGENTS.md](../../AGENTS.md)；详细说明在下面三个主题中维护，按任务读取相关章节。

| 任务 | 必读主题 | 常用定位 |
|---|---|---|
| 配置、编译、F5、运行、崩溃排查 | [构建与调试](BUILD_AND_DEBUG.md) | [构建与运行](BUILD_AND_DEBUG.md#构建与运行)、[TestDriver 编译例外](BUILD_AND_DEBUG.md#testdriver-的-release-编译例外) |
| 运行 AutoTest、选择验证路径、查输出与夹具 | [AutoTest 验证](AUTOTEST.md) | [测试套件](AUTOTEST.md#autotest-测试套件) |
| 修改架构、对象生命周期、坐标、存档或资源 | [架构与资源契约](ARCHITECTURE_AND_RESOURCES.md) | [所有权](ARCHITECTURE_AND_RESOURCES.md#所有权与场景边界)、[网格](ARCHITECTURE_AND_RESOURCES.md#board-网格)、[存档](ARCHITECTURE_AND_RESOURCES.md#存档系统)、[资源](ARCHITECTURE_AND_RESOURCES.md#资源与资产) |
| 新增功能、查原版参考或编码约定 | [架构与资源契约](ARCHITECTURE_AND_RESOURCES.md#参考与实现指引)及 AGENTS.md 路由的对应技能 | [编码约定](ARCHITECTURE_AND_RESOURCES.md#编码约定) |

一次任务涉及多个方面时组合阅读；例如修改存档后运行验证，需要架构、构建和 AutoTest 三部分。

## 项目记忆

从 Claude Code 迁移而来的项目记忆保存在 `docs/agent-memory/`，现作为面向 Codex 的项目文档维护。

- 使用 `docs/agent-memory/MEMORY.md` 作为路由索引。诊断或修改现有子系统前，先按主题搜索索引，只读取相关的记忆文件。
- 记忆属于历史工程上下文，并非当前仓库状态的证明。依赖其中带日期的分支、提交、push、行号、构建和测试结论前，必须根据 Git 与当前代码重新核实。
- 明确的任务指令、根目录 `AGENTS.md`、当前源码以及当前测试/构建证据优先于冲突的记忆记录。
- 对已记录的子系统做出实质修改后，更新对应主题文件和 `MEMORY.md` 中的一行摘要；没有合适主题时，新建范围集中的主题文件。
- 不要依赖旧的 `~/.claude` 副本。仓库内副本是后续 Codex 工作的权威项目记忆。

## 版本控制

- 提交与推送统一遵循根目录 [AGENTS.md 的“Git 与沟通”](../../AGENTS.md#git-与沟通)：主人已长期授权向指定 `origin` 的明确上游分支常规 push，完成验证后默认提交并推送，无需逐次确认。授权目的地、推送前检查、例外和审批边界只在该处维护。

## 沟通风格

称呼与沟通规则统一遵循 [AGENTS.md](../../AGENTS.md#git-与沟通)。

## 文档维护

详细契约只在对应主题中修改，本页保持导航。移动章节时同步更新仓库中的链接和技能引用；历史方案中的当时记录保留原意。
