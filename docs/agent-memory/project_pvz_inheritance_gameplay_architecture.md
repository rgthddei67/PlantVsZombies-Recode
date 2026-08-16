---
name: project-pvz-inheritance-gameplay-architecture
description: 2026-08-16 主人确认继承式玩法对象为正式架构；Component 容器按阶段收缩，EntityManager 保留为稳定 ID 注册表与查询索引
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-16
---

# 继承式玩法对象与组件容器收缩

2026-08-16 主人确认：植物、僵尸及其他有独立生命周期的玩法实体正式采用继承式对象模型。共享流程留在稳定基类，品种差异通过派生类、窄虚接口和 `GameDataManager` 注册式工厂表达；不为形式统一把植物/僵尸改写成 ECS 或复制平行状态。

当前 `GameObject` 的 `unordered_map<type_index, unique_ptr<Component>>` 是早期框架遗留的横切附件容器，不再作为新玩法系统的默认扩展点。它仍承载 Transform、Collider、Clickable、Shadow 及卡片域三个组件，必须按可独立验证的阶段迁移，不能一次性删除能力。

已批准的顺序：

1. Card 状态并回 `Card`，CardDisplay 变直接 View，CardSlotManager 由 `GameScene` 明确拥有。
2. Transform 改为显式空间值；Collider 改为宿主显式拥有并原子注册/注销。
3. Shadow 与 Clickable 保留可选组合能力，但脱离通用 Component 生命周期。
4. 运行源码中不再有 Component 派生类后，删除类型表、更新/绘制视图和 Component 基类。

`EntityManager` 不属于 ECS 组件系统。它保留稳定实体 ID、指定 ID 读档、弱引用清理、按行和稀有品种热查询；未来可单独重命名 `EntityRegistry` 或拆 `ZombieQueryIndex`，但不得随组件收缩删除。

设计：`docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

计划：`docs/superpowers/plans/2026-08-16-component-system-contraction.md`

本次只记录决策与计划，没有修改运行代码、构建或运行 AutoTest。执行任何阶段前必须重新核实当前源码和 `clang-release` 基线，旧 phase-3 性能数据不能冒充当前收益。
