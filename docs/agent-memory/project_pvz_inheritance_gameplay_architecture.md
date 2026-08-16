---
name: project-pvz-inheritance-gameplay-architecture
description: 2026-08-16 主人确认继承式玩法对象为正式架构；Card 专属组件已并入 Card，CardSlotManager 已改为 GameScene 独占控制器
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-16
---

# 继承式玩法对象与组件容器收缩

2026-08-16 主人确认：植物、僵尸及其他有独立生命周期的玩法实体正式采用继承式对象模型。共享流程留在稳定基类，品种差异通过派生类、窄虚接口和 `GameDataManager` 注册式工厂表达；不为形式统一把植物/僵尸改写成 ECS 或复制平行状态。

当前 `GameObject` 的 `unordered_map<type_index, unique_ptr<Component>>` 是早期框架遗留的横切附件容器，不再作为新玩法系统的默认扩展点。2026-08-16 已把 `CardComponent` 与 `CardDisplayComponent` 的状态、主线程缓存和绘制职责直接并入 `Card`，并把 `CardSlotManager` 从组件改为 `GameScene` 独占的普通控制器；存档 JSON 字段不变。容器当前只承载 Transform、Collider、Clickable、Shadow，后续必须按可独立验证的阶段迁移，不能一次性删除能力。

已批准的顺序：

1. Card 状态与显示已并回 `Card`；CardSlotManager 也已由 `GameScene` 明确拥有。
2. Transform 改为显式空间值；Collider 改为宿主显式拥有并原子注册/注销。
3. Shadow 与 Clickable 保留可选组合能力，但脱离通用 Component 生命周期。
4. 运行源码中不再有 Component 派生类后，删除类型表、更新/绘制视图和 Component 基类。

`EntityManager` 不属于 ECS 组件系统。它保留稳定实体 ID、指定 ID 读档、弱引用清理、按行和稀有品种热查询；未来可单独重命名 `EntityRegistry` 或拆 `ZombieQueryIndex`，但不得随组件收缩删除。

CardSlotManager 的当前契约：`GameScene` 用 `unique_ptr` 覆盖整个 Board 生命周期；`Board`、`GameInfoSaver`、`ChooseCardUI` 与实战 `Card` 只持窄非拥有引用。`Scene::UpdateAfterGameObjects()` 在全部 GameObject 更新后、Clickable/Collision 前调用控制器更新；绘制阶段在 `LAYER_UI - 1` 同步手持/落点预览坐标，路灯花挡位菜单仍用独立的晚层 UI 命令。退出场景时先清 Cell 回调、预览和 Card 绑定，再销毁 Board。

本阶段的 `smoke_plantern_fog_core` 还暴露了一个与 Card 语义无关、但被堆布局变化稳定触发的旧生命周期漏洞：`EntityManager::mZombiesByRow` 保存裸指针，若同帧先构建行桶再 `Zombie::Die()`，下一帧 GOM 会在 Board 的兜底 `CleanupExpired()` 前释放对象。修复契约是死亡与 `CommitRow()` 立即调用 `InvalidateZombieRowIndex()`，行遍历同时复核 active/dying；不能再假设“延迟删除天然保证裸指针到下一次查询都有效”。

设计：`docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

计划：`docs/superpowers/plans/2026-08-16-component-system-contraction.md`

Card 第一阶段已通过 `clang-release` 构建；卡槽选卡/分页/上次选卡、三叶草方向/冷却/存档、图鉴无 manager 宿主、词条冷却倍率与特殊卡面专项取得退出 0。CardSlotManager 阶段先以 `clang-playtest` 在原崩溃脚本 `smoke_plantern_fog_core` 完整通过 217 条命令；随后正式 `clang-release` LTO 构建与 Win7 import audit 通过，可见运行 `smoke_zombie_row_index_lifetime`、`smoke_plantern_fog_core`、`smoke_choose_card_pagination`、`smoke_last_selected_cards`、`smoke_plant_almanac_card_host`、`smoke_blover`、`smoke_crater_card_select` 均为 `status=passed`、退出码 0，相关菜单/分页/图鉴/生存轮间截图已检查。新增的 19 命令行索引专项固定先建桶、同帧杀两只、下一帧射手查询的原 UAF 窗口。`smoke_blover` 同步修正到当前 125 阳光/20 秒资源值、现行消失帧余量和 1100 宽场景的前线退出时间。开发者全脚本在 Card 的无冷却/免费与 0 阳光卡面通过后，停在既有出怪数量 6 与旧断言 3 不符，不能把该脚本记作全绿。执行后续阶段前仍须重新核实当前源码和 `clang-release` 基线，旧 phase-3 性能数据不能冒充当前收益。
