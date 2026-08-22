---
name: project-pvz-entity-memory-layout
description: Collider、Animator 与 Zombie 的冷热内存布局，真实侧车总量、存档契约和验证边界
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-22
---

# 高频实体、动画事件与运行时字符串的冷热布局

2026-08-22 主人决定暂不实现 `ZombiePool`，本阶段只收缩高数量实体共享的常驻状态和动画事件分配。设计原则是先测目标 ABI 的 record layout，再按访问频率选择连续存储、互斥状态复用或宿主独占侧车；对象池和继承架构均不因此扩张成 ECS 或通用组件系统。

## 当前布局

当前 clang-release 工具链的 record layout：`ColliderComponent` 400→88B，触发回调侧车 192B，实体碰撞回调侧车 128B；`TrackExtraInfo` 152→24B，`Animator::SparseTrackState` 56B，`Animator` 392→264B；`GameObject` 160→112B，`Zombie` 568→392B，`Plant` 336→288B，`Bullet` 376→256B。按需侧车为 `Zombie::ToxinState` 88B、`Zombie::RoofMarshalAssaultState` 32B、`Zombie::TangleKelpState` 40B 和 `Bullet::SpikeState` 40B；`ShadowComponent` 仍为 56B。

Collider 必须按实际组合计算：Bullet 和普通 Zombie 使用触发侧车，常见为 88+192=280B，较旧 400B 省 120B；多数植物没有回调，为 88B，省 312B；PotatoMine 只用实体碰撞侧车，为 216B，省 184B。未来若同一 Collider 同时需要两组侧车，总量为 408B，比旧布局多 8B；该罕见组合不应引用热对象数字声称节省。

以普通 26 轨僵尸、一个 follower 稀疏状态、触发侧车、Animator 与 Shadow 为代表，旧静态下限为 `568 + 392 + 26×152 + 400 + 56 = 5368B`，当前静态下限为 `392 + 264 + 26×24 + 56 + 88 + 192 + 56 = 1672B`，约 5.24→1.63KiB，累计减少 3696B（68.9%）；相对上一轮 1856B 又减少 184B（9.9%）。这是同口径的对象/向量元素静态下限，不含共享控制块、向量容量、堆分配器元数据和 Reanimation 包装；也不是进程 RSS 或 FPS 数据。

普通僵尸当前注册 3 个帧事件。旧 `unordered_multimap` 每个事件节点的目标 ABI 布局为 96B，3 个节点至少 288B，另有桶和分配器开销；新连续表首次保留 4×32=128B，因此仅元素存储至少再省 160B，并把多次节点分配合并为一次。并行阶段的 `DeferredEvent` 也由 64B `std::function` 改成 24B 内联回调，每个队列高水位元素少 40B。两者属于动态容量估算，不能与上面的静态下限重复相加后冒充 RSS。

## 所有权与行为契约

- Collider 的几何、掩码、注册 ID 与碰撞帧缓存留在 88B 热对象。触发器 enter/stay/exit 与实体碰撞 enter/exit 分为两个 `unique_ptr` 侧车；公开 setter 首次配置时分配，整组清空时释放。`CollisionSystem` 仍保持旧 enter/exit/stay 顺序和 trigger/collision 分派。
- `GameObject` 的 tag/name 与 `Animator` 的当前/目标轨名通过线程安全运行时驻留池保存稳定 `const std::string*`，公开 getter/setter 仍保持 `const std::string&` 语义。每个唯一字符串只在共享池保存一份；`PlayTrack` 可能从 Animator worker 调用，因此命中路径使用共享锁，缺失时才升级为独占插入。
- 每条动画轨道常用的可见性、自定义纹理和偏移留在连续 `TrackExtraInfo`；只有 follower 或附加子 Animator 的轨道进入有序 `SparseTrackState`。绘制时以轨道索引线性合并，默认实例与 `-NoInstance` 顺序一致；同名轨道的广播语义保留。第一个轨道名索引归不可变 `Reanimation` 资源所有，并在实例之间共享，不再由每个 Animator 重建哈希表。
- Animator 帧事件改为按全局帧号排序的连续 `vector`，同帧保持注册顺序，一次性事件触发后删除、重复事件保留；串行直调与并行延迟路径共用相同区间遍历。回调使用一个指针大小的内联存储，只接受无捕获或 `[this]` 这类可无异常复制、平凡析构的小对象；额外上下文必须由宿主持有，禁止恢复逐事件 `std::function` 节点分配。
- `Zombie` 删除了与 `AnimatedObject::mBoard` 重复的 Board 指针。二十层毒计时器、活动计数与小数余量进入 `ToxinState`，只在首次中毒或读到有效旧档时分配；活动层压紧在前缀，到期交换删除，未中毒每帧 O(1) 早退，毒尽、魅惑和死亡统一释放。
- 督军屋顶突袭计时/倍率/旗帜 Animator 与缠绕水草拖拽偏移/前后 Animator 分别进入 32B、40B 侧车；水草目标 ID 仍内联，保持常用资格查询不增加指针追逐。督军侧车可在计时结束后休眠复用弱附件，死亡时隐藏并释放；水草关系失效时恢复目标并释放侧车。
- Bullet 的抛射与玉米炮弹道共用带种类标记的 `TrajectoryState`，不再同时常驻两套互斥字段。只有尖刺首次命中时才建立四目标固定槽位侧车；`Reset()` 清活动计数但保留 40B 分配供该池槽复用，避免每次发射重新分配。
- 原有毒素、督军、水草、弹道与尖刺 JSON 字段名和中性默认均保持；加载只在读到有效稀有状态时分配并恢复。Collider、Animator 稀疏状态、运行时字符串和回调配置不新增存档字段。

## 验证与边界

`clang-release` 完整构建、LTO 链接与 378 项 Win7 import audit 通过。最终二进制在桌面可见运行 `smoke_blover`、`smoke_row_depth_render_order`、`smoke_roof_marshal_assault_visual`、`smoke_tanglekelp`、`smoke_cabbagepult`、`smoke_cob_cannon_core` 与 `stress_bullet_pool_active_slots`，均退出 0 且 `status=passed`；其中覆盖空回调/`[this]` 回调、tag/name 查询、督军和水草侧车的存档与绘制、两类互斥弹道及 BulletPool 复用。人工目验确认水草拖拽下沉、督军旗帜读档与玉米炮弹飞行画面正常。

`smoke_cactus` 当前仓库存在与本次改动无关的基线漂移：源码 `kSpikeFrameDamage=2`，已跟踪脚本仍断言 3，并在后续生命值断言继续按 3 计算。临时只把两处伤害期望改成 2 的布局切片通过 24 个断言/4 张截图，随后脚本已完整还原；本次未借内存优化修改平衡或测试口径。

本轮只获得编译、布局和功能回归证据，没有运行同场景迁移前后 A/B，也没有采集 L1/L2 miss、分支或 allocator 计数器。按 20000 个普通僵尸估算，本轮 `Zombie+Animator` 直接对象只减少约 3.68MB，普通三事件的元素存储至少再少约 3.2MB；20000 个 BulletPool 高水位槽位直接对象少约 2.4MB。删除哈希桶、节点、控制块和分配器碎片会继续降低实际占用，但不能从 record layout 精确推导。更小且更连续的工作集有利于缓存驻留是合理方向，真实 FPS 仍应由主人用同一 20000 实体场景 A/B 确认。
