# 继承式玩法对象架构与组件容器收缩设计

日期：2026-08-16

状态：已完成；Card、CardSlotManager、Transform、纯 UI、Collider、Shadow、Clickable 与通用 Component 框架删除均已落地

## 1. 决策

本项目正式采用**继承式玩法对象**作为植物、僵尸及其他有独立生命周期玩法实体的主模型。

- `Plant` / `Zombie` 基类拥有共享生命周期、动画、碰撞、状态与存档契约。
- 具体品种继续通过派生类、窄虚接口和注册式工厂表达差异。
- 不为追求“全项目都是 ECS”的形式统一而把品种状态机拆成组件组合。
- 早期 `Component` 容器已删除，不再作为玩法系统扩展点；可选横切能力由宿主显式拥有。
- `EntityRegistry` 是稳定 ID 注册表与查询索引，不属于待收缩的组件系统。

这是一项架构边界决策。2026-08-16 已完成前两阶段：`CardComponent` 与 `CardDisplayComponent` 并入 `Card`，`CardSlotManager` 改由 `GameScene` 明确拥有；2026-08-22 又把 Transform 从组件表迁为 `GameObject` 的可选值，让纯 UI 脱离 `GameObjectManager`，把 Collider、Shadow、Clickable 依次改为宿主显式拥有的可选对象，并最终删除没有派生类型的通用 Component 基类、类型表与生命周期视图。存档格式保持不变。

## 2. 当前事实

当前对象模型已经以继承为中心：

```text
GameObject
└── AnimatedObject
    ├── Plant → Shooter / Shroom / 各具体植物
    ├── Zombie → 各具体僵尸
    └── Coin / Mower / 其他动画对象
```

`GameObject` 直接拥有按需创建的 `std::optional<Transform>`，并分别用 `unique_ptr` 独占可选的 Collider、Shadow 与 Clickable。全项目已没有通用 `Component` 基类、派生类、按 `type_index` 索引的类型表，以及待初始化、更新和绘制视图。

单张卡的冷却、选中、方向、可用性、主线程文本缓存与绘制职责已由 `Card` 直接拥有；`CardSlotManager` 是 `GameScene` 独占的普通控制器。二者都不再通过组件容器参与通用 Component 生命周期。

它不具备典型 ECS 的数据布局和执行方式：实体不是轻量 ID，附件不在按类型连续存储中，System 也不按组件签名批量查询。植物、僵尸和子弹直接访问宿主 Transform/Collider，主要行为继续由派生类虚函数驱动。碰撞与点击各自维护专用注册表；Collider、Shadow 与 Clickable 均不再经过类型表、`dynamic_cast` 或通用 Component 生命周期。

因此当前结构应准确称为“继承式对象 + 显式附件”，而不是两套并行 ECS/OO 玩法架构。`ColliderComponent`、`ShadowComponent`、`ClickableComponent` 只保留过渡类名，不代表通用组件系统仍然存在。

纯 UI 走独立边界：`MainMenuScene` 直接拥有 `MainMenuButtons` 控制器，`UIManager` 直接拥有按钮、滑块和模态 `GameMessageBox`。弹窗关闭请求在 Button/Slider 完成本帧遍历后统一解除控件注册，不再进入 GOM 的玩法对象更新、排序和绘制生命周期。

## 3. 目标

1. 让项目文档、后续设计与代码扩展统一遵循继承式玩法对象模型。
2. 保留碰撞、点击、阴影、卡片等现有能力，同时逐步去除通用 `Component` 容器带来的类型表、独立堆分配和多套生命周期视图。
3. 每一阶段都可独立构建、验证、提交和停止，不做一次性大爆炸重写。
4. 保持当前对象所有权、渲染顺序、并行更新、输入仲裁、存档和 AutoTest 契约。
5. 只在当前基线数据证明值得时宣称性能收益；主要收益首先是降低维护与认知复杂度。

## 4. 非目标

- 不把现有植物或僵尸重写为 ECS。
- 不把派生类差异集中回 `PlantType` / `ZombieType` 大型 switch。
- 不删除 `EntityRegistry`、实体 ID 或稀有品种/按行索引。
- 不在本次规划中改变存档 schema、关卡行为、资源键或动画帧事件。
- 不承诺删除组件容器一定带来可见 FPS 提升；历史性能数据只能作为线索，执行前必须重测当前 `clang-release` 基线。

## 5. 目标架构

```text
Board（玩法权威）
├── EntityRegistry（稳定 ID、弱索引、存档枚举）
├── GameObjectManager（对象所有权、生命周期、更新与绘制顺序）
└── 继承式玩法对象
    ├── Plant / 具体植物
    ├── Zombie / 具体僵尸
    ├── Bullet / Coin / Mower
	├── 可选 Transform 值（位置、缩放、旋转）
    └── 显式可选附件
        ├── Collider（碰撞注册与回调）
        ├── Clickable（输入注册与回调）
        └── Shadow（绘制参数与可见性）

Scene（展示生命周期）
└── UIManager
    ├── Button / Slider
    └── GameMessageBox（模态对象，回调后安全清理）
```

目标中的“附件”是宿主明确拥有的字段或小对象，不再依赖 `unordered_map<type_index, unique_ptr<Component>>`、通用 `Component::Start/Update/Draw` 虚生命周期或运行时 `GetComponent<T>()` 服务定位。

## 6. 职责边界

### 6.1 玩法品种

- 品种长期状态、一次性动作、动画轨、死亡/魅惑/压扁和存档状态继续属于具体派生类或稳定基类。
- 多品种共享行为优先上提到共同基类或提取无状态/窄状态助手；只有宿主间没有合理共同基类且能力确实独立可选时，才考虑显式附件。
- 新增品种继续通过 `GameDataManager` 注册式工厂创建，不能恢复大型集中 switch。

### 6.2 Transform

Transform 是绝大多数世界对象的基础空间数据，不需要多态生命周期。当前由 `GameObject` 直接持有 `std::optional<Transform>`：空间对象显式调用 `CreateTransform()`，其余调用方统一通过 `GetTransform()` 访问；仍属于 GOM 的 `MistFuel`、`Shovel` 等无空间对象保持为空，`GameMessageBox` 与主菜单按钮则已移出 GOM。该取舍移除了每个 Transform 的独立堆分配、类型哈希与 Component 生命周期，同时避免为现有对象树增加一层 `SpatialGameObject` 和大范围多继承改造。

### 6.3 Collider

碰撞功能与 `CollisionSystem` 必须保留。当前 Collider 由 `GameObject` 通过 `unique_ptr` 显式独占，`CreateCollider()` / `RemoveCollider()` 原子处理 owner、注册/注销、collider ID 与缓存；调用方只通过 `GetCollider()` 或兼容的窄访问器取非拥有指针。构造期、Start 后按需创建、预览/咖啡豆运行时移除、对象销毁、场景清理和 BulletPool 复用均走同一入口；触发回调顺序、layer/mask、行桶和 Debug 绘制保持不变。

### 6.4 Shadow 与 Clickable

两者确实具有跨无关宿主复用价值，但不要求通用组件表：

- Shadow 保留独立参数、可见性与最终提交取证；植物动态视觉锚点、BulletPool 跨对象绘制顺序、默认实例化/`-NoInstance` 双路径都必须保持。
- Clickable 保留稀疏自注册表、渲染顺序仲裁、事件消费和光标计数；不得退回每帧扫描全部 GameObject。

2026-08-22 当前实现中，Shadow 已由 `GameObject` 通过 `unique_ptr` 显式独占，并统一使用 `CreateShadow()` / `GetShadow()` / `RemoveShadow()`；普通对象在固定本体前阶段绘制，BulletPool 继续保留跨对象地面阶段。Clickable 同样由宿主用 `unique_ptr` 独占，并统一使用 `CreateClickable()` / `GetClickable()` / `RemoveClickable()`；创建时先保证 Collider 完整就绪，移除 Collider 时同步注销 Clickable，单纯替换 Collider 则保持注册有效。两类过渡名称都不再继承或进入通用 `Component` 容器。

### 6.5 卡片域

- `CardComponent` 的冷却、选中、方向和可用性状态已并入 `Card`。
- `CardDisplayComponent` 的主线程缓存与绘制职责已并入 `Card::Start/Update/Draw`。
- `CardSlotManager` 变成 `GameScene` 明确拥有的场景控制器，不再挂在匿名 `GameObject` 上借用组件生命周期。
- 卡片存档字段、选卡/生存轮次、路灯花菜单、三叶草方向、开发者模式和图鉴无场景宿主路径保持不变。

### 6.6 UI

- `MainMenuButtons` 是 `MainMenuScene` 独占的普通控制器；其具体按钮仍由场景 `UIManager` 统一输入仲裁。
- `GameMessageBox::Builder` 保持调用接口，但 `Show()` 将模态对象注册给当前场景 `UIManager`。`Close()` 只请求关闭并立即失活，UIManager 在控件遍历后解除按钮/滑块注册并释放自身所有权。
- 场景退出时 UIManager 先解除所有模态对象与控件的关联，因此外部 `weak_ptr/shared_ptr` 不会让旧场景控件泄漏到新场景。

### 6.7 EntityRegistry

组件收缩不触碰其核心职责。2026-08-22 已将原 `EntityManager` 纯语义重命名为 `EntityRegistry`，以明确它是 Board 范围内的稳定 ID 注册表与查询索引，不是 ECS 管理器；存档 schema 和运行行为均不随类型名变化。若体积继续增长，可再把僵尸按行与稀有品种索引拆成独立 `ZombieQueryIndex`，但稳定 ID、读档指定 ID、过期弱引用清理和查询复杂度不得倒退。

## 7. 迁移原则

1. 先迁移卡片域，再迁移基础空间/碰撞，最后处理可选附件并删除通用容器。
2. 每次只移除一种组件职责；兼容访问器在调用点全部迁走后再删除。
3. 不同时重构玩法行为和基础设施；发现既有错误时先留最小复现，再决定独立修复。
4. 每阶段从 `clang-release` 取得当前基线和最终证据；需要可见 AutoTest 时遵循项目指南的桌面运行契约。
5. 性能决策以相同场景的端到端 FPS/总帧时间为主，子桶只用于定位，避免高频 `PROFILE_SCOPE` 自污染。

## 8. 必须保持的不变量

- `GameObjectManager` 的延迟删除、稳定绘制顺序和并行 Animator 阶段不变。
- Collider 注册/注销在对象销毁、运行时移除、BulletPool 回收和场景退出时都不遗留悬空指针。
- 组件内绘制顺序迁移为明确的对象绘制阶段；阴影不能因为失去 `SetDrawOrder` 而跨对象层错序。
- Clickable 数量级保持 O(可点击对象)，不能退回 O(全场对象)。
- 卡片冷却、卡片自定义状态及生存轮次恢复保持原字段语义；若无需 schema 变化，不得无故升级 schema。
- `EntityRegistry` 的所有 Add/AddWithID/cleanup/索引失效入口保持闭环。

## 9. 完成标准

- 项目内不再存在 `Component` 派生类、`GameObject::mComponents`、`AddComponent/GetComponent/RemoveComponent` 和通用组件更新/绘制视图。
- 植物与僵尸仍通过派生类和注册式工厂扩展，新增品种指引不再要求创建玩法 Component。
- 碰撞、点击、阴影和卡片专项全部通过，默认实例化与 `-NoInstance` 视觉一致。
- 存档快照往返、指定实体 ID 恢复及实体查询索引无回归。
- 文档、项目记忆和相关技能与最终接口一致。

具体执行步骤见 `docs/superpowers/plans/2026-08-16-component-system-contraction.md`。
