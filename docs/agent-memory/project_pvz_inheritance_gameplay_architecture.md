---
name: project-pvz-inheritance-gameplay-architecture
description: 2026-08-16 主人确认继承式玩法对象为正式架构；Card 专属状态与显示组件已并入 Card，其他 Component 按阶段收缩
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-16
---

# 继承式玩法对象与组件容器收缩

2026-08-16 主人确认：植物、僵尸及其他有独立生命周期的玩法实体正式采用继承式对象模型。共享流程留在稳定基类，品种差异通过派生类、窄虚接口和 `GameDataManager` 注册式工厂表达；不为形式统一把植物/僵尸改写成 ECS 或复制平行状态。

当前 `GameObject` 的 `unordered_map<type_index, unique_ptr<Component>>` 是早期框架遗留的横切附件容器，不再作为新玩法系统的默认扩展点。2026-08-16 已把 `CardComponent` 与 `CardDisplayComponent` 的状态、主线程缓存和绘制职责直接并入 `Card`，并删除这两个专属组件；存档 JSON 字段不变。容器仍承载 Transform、Collider、Clickable、Shadow 与场景级 `CardSlotManager`，后续必须按可独立验证的阶段迁移，不能一次性删除能力。

已批准的顺序：

1. Card 状态与显示已并回 `Card`；下一步再让 CardSlotManager 由 `GameScene` 明确拥有。
2. Transform 改为显式空间值；Collider 改为宿主显式拥有并原子注册/注销。
3. Shadow 与 Clickable 保留可选组合能力，但脱离通用 Component 生命周期。
4. 运行源码中不再有 Component 派生类后，删除类型表、更新/绘制视图和 Component 基类。

`EntityManager` 不属于 ECS 组件系统。它保留稳定实体 ID、指定 ID 读档、弱引用清理、按行和稀有品种热查询；未来可单独重命名 `EntityRegistry` 或拆 `ZombieQueryIndex`，但不得随组件收缩删除。

设计：`docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

计划：`docs/superpowers/plans/2026-08-16-component-system-contraction.md`

Card 第一阶段已通过 `clang-release` 构建；卡槽选卡/分页/上次选卡、三叶草方向/冷却/存档、图鉴无 manager 宿主、词条冷却倍率与特殊卡面专项取得退出 0。`smoke_blover` 同步修正到当前 125 阳光/20 秒资源值、现行消失帧余量和 1100 宽场景的前线退出时间。开发者全脚本在 Card 的无冷却/免费与 0 阳光卡面通过后，停在既有出怪数量 6 与旧断言 3 不符，不能把该脚本记作全绿。执行后续阶段前仍须重新核实当前源码和 `clang-release` 基线，旧 phase-3 性能数据不能冒充当前收益。
