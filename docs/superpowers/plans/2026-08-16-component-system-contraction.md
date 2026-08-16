# 组件系统渐进收缩 Implementation Plan

日期：2026-08-16

状态：执行中；Card 专属组件与 CardSlotManager 已完成，Transform 及后续阶段待实施

**目标：** 在保持继承式植物/僵尸、现有运行行为、存档、输入和绘制契约的前提下，分阶段移除通用 `Component` 容器；保留并显式化 Transform、Collider、Shadow、Clickable 与卡片能力。

**架构：** 玩法对象继续走 `Plant` / `Zombie` 派生体系。横切能力改为宿主明确拥有的值或可选小对象。`EntityManager` 不属于本计划的删除范围；最后的重命名/拆分是独立可选阶段。

**默认验证：** 所有运行阶段使用 `clang-release`。涉及游戏或 AutoTest 时从 `build/clang-release/` 以主人当前桌面可见窗口运行，并核对退出码、`run.log`、状态 JSON、断言和截图。旧性能记录不充当当前证据。

**设计文档：** `docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

---

## Task 0：冻结当前契约并建立基线

**只读审计：**

- [x] 列出全部 `Component` 派生类以及 `AddComponent/GetComponent/RemoveComponent` 调用点；计划建立时为七类，Card 前两阶段后已核实只剩 Transform、Collider、Clickable、Shadow 四类。
- [ ] 逐项记录运行期动态增删：植物/僵尸预览去影、无碰撞品种、图鉴 Clickable、ShovelBank 在 `Start` 后加组件、BulletPool 回收与 Shadow 特殊绘制。
- [ ] 记录 `GameObject::Start/Update/Draw/DestroyAllComponents`、Collider 注册注销、Clickable 自注册和 `Component::SetDrawOrder` 的当前顺序。
- [ ] 记录卡片存档字段和调用方：`GameInfoSaver`、`ChooseCardUI`、生存轮次冷却、路灯花菜单、三叶草方向、开发者模式。
- [ ] 记录 `EntityManager` 当前 Add/AddWithID、next ID、CleanupExpired、行索引和稀有索引入口，明确不随组件迁移改变。

**基线验证：**

- [ ] `cmake --preset clang-release`。
- [ ] `cmake --build --preset clang-release`。
- [ ] 可见运行卡片基线：`smoke_crater_card_select.json`、`smoke_choose_card_pagination.json`、`smoke_last_selected_cards.json`、`smoke_plant_almanac_card_host.json`。
- [ ] 可见运行基础对象基线：`smoke_gameplay.json`、`almanac_click.json`、`smoke_bullet_shadow.json`、`smoke_pool_instanced_shadows.json`、`smoke_mower_shadow.json`。
- [ ] 使用当前仓库既有压力场景记录端到端 FPS/总帧时间、内存和对象数量；不要在每对象路径新增高频 profiler scope。
- [ ] 保存基线提交、脚本、状态 JSON 和截图路径；若基线已有失败，先停下并单独处理，不能把既有失败带进迁移。

---

## Task 1：把 CardComponent 与 CardDisplayComponent 并入 Card

**主要文件：**

- `PlantVsZombies/Game/Card.h/.cpp`
- `PlantVsZombies/Game/CardComponent.h/.cpp`
- `PlantVsZombies/Game/CardDisplayComponent.h/.cpp`
- `PlantVsZombies/Game/ChooseCardUI.cpp`
- `PlantVsZombies/Game/CardSlotManager.cpp`
- `PlantVsZombies/Game/GameScene.cpp`
- `PlantVsZombies/Game/Board.cpp`
- `PlantVsZombies/GameInfoSaver.cpp`
- `PlantVsZombies/Game/AutoTest/TestDriver.cpp`

**步骤：**

- [x] 在 `Card` 中建立冷却、选中、选卡上下文、阳光成本、三叶草方向等权威状态；接口名称先兼容现有调用方。
- [x] 把 `CardComponent::Start/Update/StartCooldown/RestoreCooldown/ForceStateUpdate` 迁到 `Card`，保持开发者无冷却和生存词条倍率入口。
- [x] 把 `CardDisplayComponent` 的纹理/文字缓存、状态遮罩、紫卡底板、路灯花状态和三叶草方向绘制迁到 `Card::Start/Update/Draw`。
- [x] 把点击回调安装改为 `Card` 直接配置 Clickable；选卡与实战两种上下文仍走各自入口。
- [x] 将调用方从两个 Card 专属组件改为 `Card` 直接接口；迁移完成后删除 `CardComponent` 与 `CardDisplayComponent`。
- [x] 保持存档 JSON 字段和值域不变；本阶段没有因内部所有权变化升级 schema。
- [x] 审计并同步 `.agents/skills/adding-plant/SKILL.md` 与 `.agents/skills/adding-survival-perk/SKILL.md` 的 Card 状态、显示和冷却倍率接口；改过技能后运行 skill-creator `quick_validate.py`。

**验证：**

- [x] 构建 `clang-release`。
- [x] 可见运行 `smoke_crater_card_select.json`、`smoke_choose_card_pagination.json`、`smoke_last_selected_cards.json`、`smoke_grounding_shroom_card.json`、`smoke_plant_almanac_card_host.json`、`smoke_perks_balance.json`。
- [x] 专项覆盖冷却快照往返、三叶草方向与存档、路灯花卡片状态、开发者模式双条件门禁；`smoke_blover` 完整通过，开发者全脚本在 Card 门禁段通过后仍有一条独立出怪数量历史断言失败，详见项目记忆，不记作全绿。
- [x] 检查运行源码、存档实现和有效技能中不再引用 `CardComponent` / `CardDisplayComponent`；历史记录只保留明确的旧接口说明。
- [x] 独立提交，禁止与 Transform 迁移混在一个提交。

---

## Task 2：CardSlotManager 归 GameScene 所有

**主要文件：**

- `PlantVsZombies/Game/CardSlotManager.h/.cpp`
- `PlantVsZombies/Game/GameScene.h/.cpp`
- `PlantVsZombies/Game/Board.h/.cpp`
- `PlantVsZombies/GameInfoSaver.h/.cpp`

**步骤：**

- [x] 将 `CardSlotManager` 从 `Component` 改成普通场景控制器，由 `GameScene` 使用 `unique_ptr` 明确拥有。
- [x] `GameScene` 在固定阶段显式调用其 Start/Update/Draw，保持输入相对 GameObject 更新、Clickable 处理、Collision 更新的先后顺序。
- [x] `Board`、`GameInfoSaver` 和 `ChooseCardUI` 继续持有窄观察指针/参数，不把场景 UI 所有权移入 Board。
- [x] 删除匿名 `CardUI` GameObject 宿主及 `CardSlotManager` Component 派生关系。
- [x] 审计并同步 `.agents/skills/adding-plant/SKILL.md` 中 CardSlotManager、图鉴宿主和卡槽菜单路径；改过技能后运行 skill-creator `quick_validate.py`。

**验证：**

- [x] 构建 `clang-release`，LTO 链接与 Win7 import audit 通过。
- [x] 可见运行 `smoke_zombie_row_index_lifetime.json`、`smoke_plantern_fog_core.json`、`smoke_choose_card_pagination.json`、`smoke_last_selected_cards.json`、`smoke_plant_almanac_card_host.json`、`smoke_blover.json`、`smoke_crater_card_select.json`，均为 `status=passed`、退出码 0。
- [x] 截图核对分页过场、活动路灯花卡/菜单层级、图鉴卡片和轮间弹坑场景；卡图、阳光数字和冷却遮罩保持正常。
- [x] `smoke_plantern_fog_core` / `smoke_blover` 覆盖 `GAME` 快照，`smoke_crater_card_select` 覆盖生存 `CHOOSE_CARD` 保存门禁。
- [x] 检查本阶段结束后，Component 派生类仅剩 Transform、Collider、Shadow、Clickable，且仅 Clickable 仍声明 `NeedsUpdate()`。
- [x] 独立提交，不与 Transform 迁移混合。

---

## Task 3：Transform 改为显式空间数据

**主要文件：**

- `PlantVsZombies/Game/GameObject.h/.cpp`
- `PlantVsZombies/Game/AnimatedObject.h/.cpp`
- `PlantVsZombies/Game/TransformComponent.h`
- 所有 `GetComponent<TransformComponent>` / `AddComponent<TransformComponent>` 调用方

**步骤：**

- [ ] 统计无 Transform 的 GameObject，决定使用基类直接值成员还是 `SpatialGameObject`；不得凭名称假设全部对象都有空间数据。
- [ ] 引入非多态 `Transform` 值对象，保持 position/scale/rotation 语义和单位不变。
- [ ] 先提供兼容 `GetTransform()`/位置访问器并迁移调用方，避免一次修改与删除交叉。
- [ ] Collider、Shadow、Animator 和存档统一读取新 Transform 权威值，禁止保留双写镜像。
- [ ] 所有调用点迁移后删除 `TransformComponent`，并确认对象池复用会重置完整 Transform 状态。
- [ ] 审计 adding-plant、adding-zombie 及其 references 中 `Transform`/坐标权威描述；只改接口名称，不削弱逻辑位置与视觉偏移分离契约，并校验改过技能。

**验证：**

- [ ] 构建 `clang-release`。
- [ ] 可见运行普通草地、泳池和屋顶最小专项，核对植物、僵尸、子弹、割草机、弹坑、卡片和 UI 位置。
- [ ] 同步截图核对 `smoke_bullet_shadow.json`、`smoke_pool_plant_shadow_bob.json`、屋顶地形专项。
- [ ] 存档快照往返后断言位置、行列和视觉偏移未被混成同一字段。
- [ ] 独立提交。

---

## Task 4：Collider 脱离 Component 容器

**主要文件：**

- `PlantVsZombies/Game/ColliderComponent.h/.cpp`
- `PlantVsZombies/Game/CollisionSystem.h`
- `PlantVsZombies/Game/GameObject.h/.cpp`
- `Plant`、`Zombie`、`Bullet`、`Cell`、`Card` 及其他碰撞宿主

**步骤：**

- [ ] 将 Collider 改为宿主明确拥有的可选对象；可保留 `ColliderComponent` 名称作为过渡，但先移除对 `Component` 生命周期的依赖。
- [ ] 提供唯一 `CreateCollider/RemoveCollider` 入口，原子完成 owner 绑定、注册/注销、collider ID 和缓存初始化。
- [ ] 删除 `GameObject::RegisterComponentIfNeeded/UnregisterComponentIfNeeded` 的 `dynamic_cast` 特判。
- [ ] 审计构造时创建、Start 后创建、运行时移除、对象销毁、场景清理和 BulletPool 回收六条路径。
- [ ] 保持回调对象存活期、Enter/Stay/Exit 顺序、layer/mask、按行分桶和 Debug 绘制。
- [ ] 审计 adding-plant、adding-zombie 中 collider、相对几何、啃食/碰撞回调与 AutoTest 投影接口，更新并校验改过技能。

**验证：**

- [ ] 构建 `clang-release`。
- [ ] 可见运行 `smoke_gameplay.json`、`smoke_crater_card_select.json` 及代表植物/僵尸接触、子弹命中、割草机碰撞的最小专项。
- [ ] `-Debug` 显示碰撞框，截图核对位置和大小。
- [ ] 场景退出与大量对象销毁后检查无悬空 Collider、重复 ID 或残留回调。
- [ ] 压力场景确认 Collision 总帧时间和 sweep 计数没有复杂度倒退。
- [ ] 独立提交。

---

## Task 5：Shadow 与 Clickable 改为显式可选附件

**Shadow：**

- [ ] 将 Shadow 改为宿主明确拥有的可选对象或参数结构，由对象固定绘制阶段提交。
- [ ] 把 `RemoveComponent<ShadowComponent>` 改为显式禁用/移除，保持预览、入水、出土、异形品种和 AutoTest 取证。
- [ ] 保留植物动态视觉锚点、BulletPool 的跨对象阴影阶段、实例化队列与 `-NoInstance` fallback。
- [ ] 用 `smoke_bullet_shadow.json`、`smoke_pool_instanced_shadows.json`、`smoke_pool_plant_shadow_bob.json`、`smoke_mower_shadow.json` 做可见回归。

**Clickable：**

- [ ] 将 Clickable 改为宿主明确拥有的可选对象/注册句柄，不再继承 `Component`。
- [ ] 保留构造注册、析构注销、渲染顺序降序、ConsumeEvent、悬停光标计数和 UI/世界坐标选择。
- [ ] 明确 Clickable 与 Collider 的绑定时机，不允许出现已注册 Clickable 持有未初始化 Collider。
- [ ] 保留 O(可点击对象) 自注册表，禁止恢复 `GetAllGameObjects()` 每帧全表扫描。
- [ ] 用 `almanac_click.json`、`smoke_choose_card_pagination.json` 和真实卡片/格子边界点击回归。
- [ ] 审计 adding-plant、adding-zombie 中 Shadow/Clickable 现行接口、输入仲裁和视觉取证描述，更新并校验改过技能。

**提交：**

- [ ] Shadow 与 Clickable 分成两个提交；任一视觉或输入回归时可以单独回退。

---

## Task 6：删除通用 Component 框架

**主要文件：**

- `PlantVsZombies/Game/Component.h/.cpp`
- `PlantVsZombies/Game/GameObject.h/.cpp`
- 项目指南、架构设计、相关项目记忆与技能

**步骤：**

- [ ] 反向审计所有 `: public Component`、`AddComponent/GetComponent/RemoveComponent` 和 `Component*`，运行源码必须为零。
- [ ] 删除 `mComponents`、`mComponentsToInitialize`、`mUpdatableComponents`、`mDrawableComponents` 和 `mDrawableSortDirty`。
- [ ] 删除通用 Component Start/Update/Draw/OnDestroy/NeedsUpdate/SetDrawOrder 生命周期。
- [ ] 将仍需的绘制顺序写成对象 Draw 的明确阶段，不能用删除组件排序掩盖视觉差异。
- [ ] 删除 Component 源文件并清理失效 include、注释和历史 TODO；源码由 GLOB 自动收集，无需手改构建列表。

**完整验证：**

- [ ] `cmake --preset clang-release`。
- [ ] `cmake --build --preset clang-release`。
- [ ] 重跑 Task 0 的全部可见 AutoTest，并逐项核对退出码、日志、状态与截图。
- [ ] 默认实例化和 `-NoInstance` 各跑阴影/卡片视觉专项。
- [ ] 快照往返覆盖卡片、植物、僵尸、子弹、金币和割草机。
- [ ] 用与 Task 0 相同压力场景比较 FPS/总帧时间和内存；性能无提升可以接受，但不得显著回退。任何结论只引用本次 A/B 数据。
- [ ] 更新项目指南，组件章节改为已完成迁移；更新相关技能路径与接口，并用 skill-creator `quick_validate.py` 校验改过的技能。
- [ ] 独立提交完整移除；工作树干净后再决定是否 push。

---

## Task 7（可选、独立）：EntityManager 重命名与索引拆分

本阶段不属于删除组件的完成条件，建议另开设计与计划，不与 Task 1～6 混做。

- [ ] 评估只重命名为 `EntityRegistry` 是否已足够降低“它是 ECS”的认知混淆。
- [ ] 若类继续增长，再把僵尸按行、黄色冰道、督军、劫持者、引雷单位、急救员等查询结构拆成 `ZombieQueryIndex`，主注册表仍是唯一实体全集。
- [ ] 保持所有 next ID、AddWithID、过期清理、同帧索引失效和查询复杂度。
- [ ] 存档字段不因 C++ 类型重命名变化；无 schema 变化就不升级版本。
- [ ] 单独构建、AutoTest、提交和性能验证。

---

## 执行纪律

- 任一 Task 开始前先重新核实当前源码、Git 状态和相关历史记忆；本计划中的路径与调用点会随仓库演进而漂移。
- 每个 Task 只解决一种所有权/生命周期变化，不顺手改玩法数值、动画时序或资源。
- 每个 Task 提交前都要审计本阶段相关的 `.agents/skills/` 与 references；发现接口、路径或所有权已经变化就同步更新，所有改过的技能都运行 skill-creator `quick_validate.py`，不能推迟到最终删除阶段一次处理。
- 新工作若需要添加动画帧事件，必须先询问主人；本计划预期不需要新增帧事件。
- 任何阶段发现必须改变存档字段、外部行为或 Board 所有权时，先暂停并更新设计，经主人确认后继续。
- Card 专属状态与显示组件已按主人后续指令落地；其余 Task 仍须逐项重新核实后执行。
