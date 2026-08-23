---
name: project-pvz-inheritance-gameplay-architecture
description: 继承式玩法对象正式架构；Card、CardSlotManager、显式 Transform、纯 UI 与显式附件迁移完成，通用 Component 框架已删除
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-23
---

# 继承式玩法对象与组件容器收缩

2026-08-16 主人确认：植物、僵尸及其他有独立生命周期的玩法实体正式采用继承式对象模型。共享流程留在稳定基类，品种差异通过派生类、窄虚接口和 `GameDataManager` 注册式工厂表达；不为形式统一把植物/僵尸改写成 ECS 或复制平行状态。

`GameObject` 早期的 `unordered_map<type_index, unique_ptr<Component>>` 横切附件容器已删除，不再作为玩法系统扩展点。2026-08-16 已把 `CardComponent` 与 `CardDisplayComponent` 的状态、主线程缓存和绘制职责直接并入 `Card`，并把 `CardSlotManager` 从组件改为 `GameScene` 独占的普通控制器；存档 JSON 字段不变。2026-08-22 又依次把 Transform 改为 `GameObject` 内按需创建的 `std::optional<Transform>`，把 Collider、Shadow 与 Clickable 改为 `GameObject` 的显式可选所有权，最后删除 Component 基类、类型表、模板接口和通用生命周期视图。`*Component` 后缀目前只属于三个显式附件的过渡名称。

已批准的顺序：

1. Card 状态与显示已并回 `Card`；CardSlotManager 也已由 `GameScene` 明确拥有。
2. Transform 已改为显式空间值；纯 UI 已脱离 GameObjectManager。
3. Collider 已改为宿主显式拥有并原子注册/注销。
4. Shadow 与 Clickable 已分别改为显式可选附件，稀疏点击注册能力和阴影绘制阶段保持不变。
5. 空类型表、更新/绘制视图和 Component 基类已删除；迁移完成。

通用框架删除后的契约：`GameObject::Start/Update/Draw/DestroyAttachments` 只显式处理 Collider、Shadow 与 Clickable，不提供按类型查询或任意附件生命周期。Shadow 固定在宿主本体前提交，Collider 只在 Debug 路径绘制，Clickable 保持稀疏注册和宿主更新时序。新增横切能力必须先确定宿主所有权与明确调度阶段，不能恢复通用组件容器。

`EntityRegistry` 不属于 ECS 组件系统。2026-08-22 已把原 `EntityManager` 及 Board 成员纯语义重命名为 `EntityRegistry` / `mEntityRegistry`；稳定实体 ID、指定 ID 读档、弱引用清理、按行和稀有品种热查询全部保持。若类继续增长，可按需拆出 `ZombieQueryIndex`，但主注册表仍是唯一实体全集，不能复制所有权或倒退查询复杂度。

Transform 当前契约：只有空间对象调用 `CreateTransform()`，所有消费者用 `GetTransform()` 访问唯一权威值；仍属于 GOM 的无空间对象 `MistFuel`、`Shovel` 保持 optional 为空。该取舍以每个 GameObject 少量内联空间换掉每个空间对象的一次堆分配、`type_index` 哈希节点和组件生命周期；BulletPool 的 `Reset` 必须同时恢复位置、缩放和旋转。逻辑 `row/column` 与 `mVisualOffset` 继续独立，Transform 不自动消费屋顶坡面。

Collider 当前契约：`GameObject` 用 `unique_ptr<ColliderComponent>` 独占至多一个碰撞附件，`ColliderComponent` 仅保留过渡名称、不再继承 `Component`。创建、替换和移除只能走 `CreateCollider()` / `RemoveCollider()`，由入口原子处理 owner、CollisionSystem 注册/注销、ID 与缓存；消费者用 `GetCollider()` 或基类兼容窄访问器取非拥有指针，不得再调用 `Add/Get/RemoveComponent<ColliderComponent>` 或维护重复裸缓存。构造期创建、`CreateClickable()` 按需补默认 Collider、预览/咖啡豆运行时移除、对象销毁、场景清理和 BulletPool 复用都已覆盖；移除 Collider 会先同步注销 Clickable，替换 Collider 则先建好新对象再换入，使 Clickable 始终只观察完整 Collider。2026-08-22 起五个 `std::function` 不再常驻每个 Collider：热对象只保留几何、掩码、帧缓存和两个冷侧车指针，触发器三回调与实体碰撞两回调分别首次配置时分配，全部清空即释放；Bullet/普通 Zombie 使用触发侧车，多数植物无回调，PotatoMine 只使用实体碰撞侧车。必须同时报告热对象和实际侧车总量，不能只引用 88B 热对象。Debug 绘制由 `GameObject::Draw()` 显式提交。

Shadow 当前契约：`GameObject` 用 `unique_ptr<ShadowComponent>` 独占至多一个阴影附件，`ShadowComponent` 仅保留过渡名称、不再继承 `Component`。创建、访问和移除统一走 `CreateShadow()` / `GetShadow()` / `RemoveShadow()`；介质/出土等生命周期显隐用 `SetVisible()`，跳跃/投掷等动作阶段门控用 `SetEnabled()`，两者独立并取 AND，保持旧 `mVisible` 与 Component enabled 的双原因语义。植物、僵尸、子弹和割草机不再维护重复裸缓存。普通对象由 `GameObject::Draw()` 在剩余附件和本体前固定提交，Bullet 保持不调用该入口，由 `BulletPool::DrawShadows()` 在 GOM 主体前统一提交。默认实例路径仍用 `DrawTextureInstanced()` 与 reanim 本体进入同一实例流，`-NoInstance` 仍走普通 batch；植物动态视觉锚点、最终提交中心取证和运行时无影子品种语义保持不变。

Clickable 当前契约：`GameObject` 用 `unique_ptr<ClickableComponent>` 独占至多一个输入附件，创建、访问和移除统一走 `CreateClickable()` / `GetClickable()` / `RemoveClickable()`。构造注册、析构注销仍由 Clickable 自己维护主线程稀疏表；静态输入阶段继续按宿主渲染顺序降序仲裁，保留 `ConsumeEvent`、悬停光标计数与 UI/世界坐标选择。由于所有权从 `Start()` 前移到构造路径，处理器显式跳过尚未完成 `GameObject::Start()` 的宿主，以保持旧组件待初始化阶段不可点击的时序。禁止恢复 `GetAllGameObjects()` 每帧全表扫描。

纯 UI 阶段已完成：`GameButton` 收敛为 `MainMenuScene` 独占的 `MainMenuButtons` 普通控制器；`GameMessageBox` 由场景 `UIManager` 直接拥有，保留 Builder 和弱引用调用方。`Close()` 立即失活并请求关闭，UIManager 在 Button/Slider 遍历结束后解除控件注册，场景退出也先断开外部引用；弹窗不再进入全局 GOM 更新、排序和绘制遍历。该阶段只宣称所有权清晰和固定调度减少，没有 A/B 数据，不宣称 FPS 数字。

CardSlotManager 的当前契约：`GameScene` 用 `unique_ptr` 覆盖整个 Board 生命周期；`Board`、`GameInfoSaver`、`ChooseCardUI` 与实战 `Card` 只持窄非拥有引用。`Scene::UpdateAfterGameObjects()` 在全部 GameObject 更新后、Clickable/Collision 前调用控制器更新；手持植物预览以逻辑鼠标为锚，但本体属于世界层，因此须由 `GameScene::Draw` 在本帧最终相机确定后、`GameObjectManager` 绘制前只做一次 `LogicalToWorld` 同步，不能在 Update 写逻辑坐标后再由晚层命令二次修正。路灯花挡位菜单仍用独立的晚层 UI 命令。退出场景时先清 Cell 回调、预览和 Card 绑定，再销毁 Board。

本阶段的 `smoke_plantern_fog_core` 还暴露了一个与 Card 语义无关、但被堆布局变化稳定触发的旧生命周期漏洞：`EntityRegistry::mZombiesByRow` 保存裸指针，若同帧先构建行桶再 `Zombie::Die()`，下一帧 GOM 会在 Board 的兜底 `CleanupExpired()` 前释放对象。修复契约是死亡与 `CommitRow()` 立即调用 `InvalidateZombieRowIndex()`，行遍历同时复核 active/dying；不能再假设“延迟删除天然保证裸指针到下一次查询都有效”。

设计：`docs/superpowers/specs/2026-08-16-inheritance-gameplay-object-architecture-design.md`

计划：`docs/superpowers/plans/2026-08-16-component-system-contraction.md`

Card 第一阶段已通过 `clang-release` 构建；卡槽选卡/分页/上次选卡、三叶草方向/冷却/存档、图鉴无 manager 宿主、词条冷却倍率与特殊卡面专项取得退出 0。CardSlotManager 阶段先以 `clang-playtest` 在原崩溃脚本 `smoke_plantern_fog_core` 完整通过 217 条命令；随后正式 `clang-release` LTO 构建与 Win7 import audit 通过，可见运行 `smoke_zombie_row_index_lifetime`、`smoke_plantern_fog_core`、`smoke_choose_card_pagination`、`smoke_last_selected_cards`、`smoke_plant_almanac_card_host`、`smoke_blover`、`smoke_crater_card_select` 均为 `status=passed`、退出码 0，相关菜单/分页/图鉴/生存轮间截图已检查。新增的 19 命令行索引专项固定先建桶、同帧杀两只、下一帧射手查询的原 UAF 窗口。`smoke_blover` 同步修正到当前 125 阳光/20 秒资源值、现行消失帧余量和 1100 宽场景的前线退出时间。开发者全脚本在 Card 的无冷却/免费与 0 阳光卡面通过后，停在既有出怪数量 6 与旧断言 3 不符，不能把该脚本记作全绿。执行后续阶段前仍须重新核实当前源码和 `clang-release` 基线，旧 phase-3 性能数据不能冒充当前收益。

Transform 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_gameplay`、`smoke_bullet_shadow`、`smoke_pool_plant_shadow_bob` 默认/`-NoInstance`、`smoke_roof_terrain_consumers`、`smoke_zombie_row_index_lifetime`、`smoke_mower_shadow`、`smoke_choose_card_pagination` 均退出 0 且 `status=passed`。屋顶专项的 51 条断言与正式快照往返覆盖预览僵尸、植物/花盆、弹坑、子弹、冰道、小推车和二维视觉偏移；泳池三相位影子与选卡分页截图已目验。`smoke_mower_shadow` 原脚本缺少从 `CHOOSE_CARD` 进入 `GAME` 的前置步骤，首跑在 cmd#1 超时；只补 `choose_cards` 测试前置后通过，没有为测试修改游戏行为。

纯 UI 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_mainmenu_buttons`、`smoke_mainmenu_console`、`pause_menu_shot`、`smoke_particle_layers`、`smoke_perk_select`、`smoke_perk_select_skip_all`、`smoke_perk_select_skip_then_pick`、`smoke_perk_view`、`smoke_dev_panel_lifecycle -develop` 均退出 0、`status=passed`、`script finished OK`。截图已核对主菜单、控制台反复开关、Esc 菜单打开/关闭、词条刷新重建、翻页、开发者面板重建/召唤模式往返与最终清空，无残留按钮或双框。旧综合 `smoke_develop` 的 UI、开关和召唤段通过后仍在 cmd#35 失败（`wave` 期望 2、实际 0）；该脚本不能记作全绿，生命周期证据以新增 28 命令专项为准。

Collider 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `smoke_gameplay`、`smoke_potatomine`、`smoke_torchwood`、`smoke_pool_cleaner`、`smoke_crater_card_select`、`smoke_coffeebean`、`smoke_zombie_row_index_lifetime`、`smoke_blover`、`smoke_plant_almanac_card_host` 与新增 `smoke_collider_ownership` 均退出 0、`status=passed`。新增 26 命令专项以真实点击覆盖 Card/Cell、ShovelBank 在 `Start()` 后按需创建默认 Collider、运行时铲除和第二次场景重建；`-Debug` 截图已核对 Card、Cell 网格与场景边界碰撞框。土豆地雷专项改为暂停自然出怪后再锁定两段接触/范围爆炸数量，避免 20 秒首波成为不稳定对照；Debug 三张截图确认植物与僵尸碰撞框相交和爆炸后目标结果。Collision sweep 与按行分桶算法未改，本阶段没有同场景 A/B 数字，不宣称 FPS 收益。

Collider 回调冷热分离的补充证据：当前 clang-release ABI 下 `ColliderComponent` 400→88B，触发侧车 192B、实体碰撞侧车 128B，因此 Bullet/普通 Zombie 常见总量 280B、无回调植物 88B、PotatoMine 216B；若未来同一对象同时使用两组则为 408B，反而比旧布局多 8B。桌面可见 `smoke_toxic_peashooter`、`smoke_imp_eat` 与 `smoke_potatomine` 分别覆盖子弹触发、僵尸触发和植物实体碰撞，均退出 0、`status=passed`；碰撞对顺序、enter/stay/exit 与 trigger/collision 分派未改。本阶段只测布局和功能，没有压力场景 A/B 或硬件计数器，不能宣称缓存命中率或 FPS 已提高。

Shadow 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过。`smoke_bullet_shadow`、`smoke_pool_instanced_shadows`、`smoke_pool_plant_shadow_bob`、`smoke_mower_shadow` 在默认实例路径和 `-NoInstance` 各可见运行一次，8 次均退出 0、`status=passed`、`script finished OK`，截图确认子弹跨对象层、泳池 8 组叠层、三相位浮动锚点和割草机落点。补充可见回归中 `smoke_coffeebean`、`smoke_seashroom`、`smoke_digger`、`smoke_dolphin_rider`、`smoke_catapult_zombie`、`smoke_zamboni` 通过；`smoke_starfruit` 的本阶段 `hasShadow=false` 已通过后停在既有星弹风力期望，`smoke_imp_zombie` 停在肢体粒子包围盒，`smoke_bungee_zombie` 停在 rollout 64/48 期望漂移，三者不得记作全绿，也不在 Shadow 阶段顺手修改玩法或测试语义。

Clickable 阶段的 2026-08-22 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；可见运行 `almanac_click`、`smoke_choose_card_pagination`、`smoke_collider_ownership`、`smoke_zombie_almanac_progression` 与新增 `smoke_clickable_ownership`，均退出 0、`status=passed`、`script finished OK`，合计 56 条断言无失败。截图已核对主图鉴入口、选卡分页、共享格边界与铲除、僵尸图鉴第二条路障详情、路灯花本体点击/III 挡菜单以及 Trophy 点击后的胜利状态。共享格边界应断言窗口坐标转换后的正式唯一格解析结果，不要直接把逻辑边界 X 当成像素完全等价；旧僵尸图鉴 UI 进入路径已漂移，专项用既有 `goto_zombie_almanac` 稳定进入后再真实点击目标条目。

通用 Component 删除阶段的 2026-08-22 当前证据：`clang-release` 完整配置、123 单元重编译、LTO 链接和 378 项 Win7 import audit 通过；运行源码中 `Component` 基类/派生、类型表、模板访问器、初始化/更新/绘制视图及 `NeedsUpdate/SetDrawOrder` 为零。主人当前桌面可见运行 Task 0 全集，并补跑 Collider/Clickable 所有权、AutoTest harness、屋顶对象、Blover、Digger、双子向日葵存档专项，均退出 0、`status=passed`、`script finished OK`；默认与 `-NoInstance` 的阴影、选卡分页截图均已目验。双子向日葵新增现场两枚阳光 Coin 的保存/全新 `GameScene` 重载断言，连同 Blover、Digger、harness 和屋顶专项覆盖 Card、Plant、Zombie、Bullet、Coin、Mower 往返。本阶段没有迁移前同提交压力 A/B，不能引用 2026-05 phase-3 数据宣称当前 FPS 收益。

`EntityRegistry` 语义重命名阶段的 2026-08-22 当前证据：全仓逐文件机械等价性审计确认运行代码只改变类型、文件、Board 成员和局部变量名，`EntityRegistry.cpp/.h` 与旧实现等价，源码旧名为零；存档字段、ID 计数器、AddWithID、CleanupExpired、行索引失效和稀有弱索引均未改变。`clang-release` 完整配置、120 步编译/链接和 378 项 Win7 import audit 通过，构建图只包含新文件。主人当前桌面可见运行 `smoke_zombie_row_index_lifetime`、`smoke_autotest_harness`、`smoke_roof_marshal_boss_bar`、`smoke_healer_coordination`，共 177 条命令、60 个状态断言、9 张同步截图，均退出 0、`status=passed`、`script finished OK`，快照恢复、督军血条与急救员群组截图已目验。三份受影响技能均通过 skill-creator `quick_validate.py`。本阶段没有运行行为或复杂度变化，不宣称性能收益；`ZombieQueryIndex` 拆分仅在注册表继续增长并出现明确维护收益时再做。

CardSlotManager 相机同步修复的 2026-08-23 当前证据：`clang-release` LTO 与 378 项 Win7 import audit 通过；桌面可见 `smoke_plant_preview_camera` 默认实例/`-NoInstance`、`smoke_advanced_pause`、`smoke_pool_visual_fixes` 均退出 0 且 `status=passed`。专项以 Animator 同帧实际提交基点反投影，锁定开场横移与暂停震屏两处预览相对鼠标均为 `(0,0)`，同步截图已目验。旧 `smoke_screen_shake` 的 0.12 秒樱桃起震窗口在不同预设跨越原 16～20 帧硬边界，当前 Release 失败于“第 16 帧仍应为 0”的过期时序断言；这条已知测试漂移不作为预览修复失败，也不能记作全绿。
