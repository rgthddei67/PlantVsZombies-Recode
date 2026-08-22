---
name: project-pvz-inheritance-gameplay-architecture
description: 继承式玩法对象正式架构；Card、CardSlotManager、显式 Transform、纯 UI 与 Collider 所有权已完成，后续迁移 Shadow/Clickable
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-22
---

# 继承式玩法对象与组件容器收缩

2026-08-16 主人确认：植物、僵尸及其他有独立生命周期的玩法实体正式采用继承式对象模型。共享流程留在稳定基类，品种差异通过派生类、窄虚接口和 `GameDataManager` 注册式工厂表达；不为形式统一把植物/僵尸改写成 ECS 或复制平行状态。

当前 `GameObject` 的 `unordered_map<type_index, unique_ptr<Component>>` 是早期框架遗留的横切附件容器，不再作为新玩法系统的默认扩展点。2026-08-16 已把 `CardComponent` 与 `CardDisplayComponent` 的状态、主线程缓存和绘制职责直接并入 `Card`，并把 `CardSlotManager` 从组件改为 `GameScene` 独占的普通控制器；存档 JSON 字段不变。2026-08-22 又把 Transform 改为 `GameObject` 内按需创建的 `std::optional<Transform>`，删除 `TransformComponent` 与派生类重复缓存；Collider 随后改为 `GameObject` 的显式可选所有权并脱离 Component 生命周期。容器当前只承载 Clickable、Shadow，后续必须按可独立验证的阶段迁移，不能一次性删除能力。

已批准的顺序：

1. Card 状态与显示已并回 `Card`；CardSlotManager 也已由 `GameScene` 明确拥有。
2. Transform 已改为显式空间值；纯 UI 已脱离 GameObjectManager。
3. Collider 已改为宿主显式拥有并原子注册/注销。
4. 下一阶段先迁移 Shadow，再独立迁移 Clickable；两者保留可选组合能力，但脱离通用 Component 生命周期。
5. 运行源码中不再有 Component 派生类后，删除类型表、更新/绘制视图和 Component 基类。

`EntityManager` 不属于 ECS 组件系统。它保留稳定实体 ID、指定 ID 读档、弱引用清理、按行和稀有品种热查询；未来可单独重命名 `EntityRegistry` 或拆 `ZombieQueryIndex`，但不得随组件收缩删除。

Transform 当前契约：只有空间对象调用 `CreateTransform()`，所有消费者用 `GetTransform()` 访问唯一权威值；仍属于 GOM 的无空间对象 `MistFuel`、`Shovel` 保持 optional 为空。该取舍以每个 GameObject 少量内联空间换掉每个空间对象的一次堆分配、`type_index` 哈希节点和组件生命周期；BulletPool 的 `Reset` 必须同时恢复位置、缩放和旋转。逻辑 `row/column` 与 `mVisualOffset` 继续独立，Transform 不自动消费屋顶坡面。

Collider 当前契约：`GameObject` 用 `unique_ptr<ColliderComponent>` 独占至多一个碰撞附件，`ColliderComponent` 仅保留过渡名称、不再继承 `Component`。创建、替换和移除只能走 `CreateCollider()` / `RemoveCollider()`，由入口原子处理 owner、CollisionSystem 注册/注销、ID 与缓存；消费者用 `GetCollider()` 或基类兼容窄访问器取非拥有指针，不得再调用 `Add/Get/RemoveComponent<ColliderComponent>` 或维护重复裸缓存。构造期创建、Clickable 在 Start 后补默认 Collider、预览/咖啡豆运行时移除、对象销毁、场景清理和 BulletPool 复用都已覆盖；Debug 绘制由 `GameObject::Draw()` 显式提交。

纯 UI 阶段已完成：`GameButton` 收敛为 `MainMenuScene` 独占的 `MainMenuButtons` 普通控制器；`GameMessageBox` 由场景 `UIManager` 直接拥有，保留 Builder 和弱引用调用方。`Close()` 立即失活并请求关闭，UIManager 在 Button/Slider 遍历结束后解除控件注册，场景退出也先断开外部引用；弹窗不再进入全局 GOM 更新、排序和绘制遍历。该阶段只宣称所有权清晰和固定调度减少，没有 A/B 数据，不宣称 FPS 数字。

CardSlotManager 的当前契约：`GameScene` 用 `unique_ptr` 覆盖整个 Board 生命周期；`Board`、`GameInfoSaver`、`ChooseCardUI` 与实战 `Card` 只持窄非拥有引用。`Scene::UpdateAfterGameObjects()` 在全部 GameObject 更新后、Clickable/Collision 前调用控制器更新；绘制阶段在 `LAYER_UI - 1` 同步手持/落点预览坐标，路灯花挡位菜单仍用独立的晚层 UI 命令。退出场景时先清 Cell 回调、预览和 Card 绑定，再销毁 Board。

本阶段的 `smoke_plantern_fog_core` 还暴露了一个与 Card 语义无关、但被堆布局变化稳定触发的旧生命周期漏洞：`EntityManager::mZombiesByRow` 保存裸指针，若同帧先构建行桶再 `Zombie::Die()`，下一帧 GOM 会在 Board 的兜底 `CleanupExpired()` 前释放对象。修复契约是死亡与 `CommitRow()` 立即调用 `InvalidateZombieRowIndex()`，行遍历同时复核 active/dying；不能再假设“延迟删除天然保证裸指针到下一次查询都有效”。

设计：`docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

计划：`docs/superpowers/plans/2026-08-16-component-system-contraction.md`

Card 第一阶段已通过 `clang-release` 构建；卡槽选卡/分页/上次选卡、三叶草方向/冷却/存档、图鉴无 manager 宿主、词条冷却倍率与特殊卡面专项取得退出 0。CardSlotManager 阶段先以 `clang-playtest` 在原崩溃脚本 `smoke_plantern_fog_core` 完整通过 217 条命令；随后正式 `clang-release` LTO 构建与 Win7 import audit 通过，可见运行 `smoke_zombie_row_index_lifetime`、`smoke_plantern_fog_core`、`smoke_choose_card_pagination`、`smoke_last_selected_cards`、`smoke_plant_almanac_card_host`、`smoke_blover`、`smoke_crater_card_select` 均为 `status=passed`、退出码 0，相关菜单/分页/图鉴/生存轮间截图已检查。新增的 19 命令行索引专项固定先建桶、同帧杀两只、下一帧射手查询的原 UAF 窗口。`smoke_blover` 同步修正到当前 125 阳光/20 秒资源值、现行消失帧余量和 1100 宽场景的前线退出时间。开发者全脚本在 Card 的无冷却/免费与 0 阳光卡面通过后，停在既有出怪数量 6 与旧断言 3 不符，不能把该脚本记作全绿。执行后续阶段前仍须重新核实当前源码和 `clang-release` 基线，旧 phase-3 性能数据不能冒充当前收益。

Transform 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_gameplay`、`smoke_bullet_shadow`、`smoke_pool_plant_shadow_bob` 默认/`-NoInstance`、`smoke_roof_terrain_consumers`、`smoke_zombie_row_index_lifetime`、`smoke_mower_shadow`、`smoke_choose_card_pagination` 均退出 0 且 `status=passed`。屋顶专项的 51 条断言与正式快照往返覆盖预览僵尸、植物/花盆、弹坑、子弹、冰道、小推车和二维视觉偏移；泳池三相位影子与选卡分页截图已目验。`smoke_mower_shadow` 原脚本缺少从 `CHOOSE_CARD` 进入 `GAME` 的前置步骤，首跑在 cmd#1 超时；只补 `choose_cards` 测试前置后通过，没有为测试修改游戏行为。

纯 UI 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_mainmenu_buttons`、`smoke_mainmenu_console`、`pause_menu_shot`、`smoke_particle_layers`、`smoke_perk_select`、`smoke_perk_select_skip_all`、`smoke_perk_select_skip_then_pick`、`smoke_perk_view`、`smoke_dev_panel_lifecycle -develop` 均退出 0、`status=passed`、`script finished OK`。截图已核对主菜单、控制台反复开关、Esc 菜单打开/关闭、词条刷新重建、翻页、开发者面板重建/召唤模式往返与最终清空，无残留按钮或双框。旧综合 `smoke_develop` 的 UI、开关和召唤段通过后仍在 cmd#35 失败（`wave` 期望 2、实际 0）；该脚本不能记作全绿，生命周期证据以新增 28 命令专项为准。

Collider 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_gameplay`、`smoke_potatomine`、`smoke_torchwood`、`smoke_pool_cleaner`、`smoke_crater_card_select`、`smoke_coffeebean`、`smoke_zombie_row_index_lifetime`、`smoke_blover`、`smoke_plant_almanac_card_host` 与新增 `smoke_collider_ownership` 均退出 0、`status=passed`。新增 26 命令专项以真实点击覆盖 Card/Cell、ShovelBank 在 `Start()` 后按需创建默认 Collider、运行时铲除和第二次场景重建；`-Debug` 截图已核对 Card、Cell 网格与场景边界碰撞框。土豆地雷专项改为暂停自然出怪后再锁定两段接触/范围爆炸数量，避免 20 秒首波成为不稳定对照；Debug 三张截图确认植物与僵尸碰撞框相交和爆炸后目标结果。Collision sweep 与按行分桶算法未改，本阶段没有同场景 A/B 数字，不宣称 FPS 收益。
