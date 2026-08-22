---
name: project-pvz-entity-memory-layout
description: Collider、Animator 与 Zombie 的冷热内存布局，真实侧车总量、存档契约和验证边界
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-22
---

# 实体冷热内存布局与僵尸常驻收缩

2026-08-22 主人决定暂不实现 `ZombiePool`，本阶段只收缩高数量实体共享的常驻状态。设计原则是先测目标 ABI 的 record layout，再把真正稀有的大状态移到宿主独占、首次生效才分配的侧车；对象池和继承架构均不因此扩张成 ECS 或通用组件系统。

## 当前布局

当前 clang-release 工具链的 record layout：`ColliderComponent` 400→88B，触发回调侧车 192B，实体碰撞回调侧车 128B；`TrackExtraInfo` 152→24B，`Animator::SparseTrackState` 56B，`Animator` 392→352B；`Zombie` 568→488B，按需 `Zombie::ToxinState` 88B。`ShadowComponent` 仍为 56B。

Collider 必须按实际组合计算：Bullet 和普通 Zombie 使用触发侧车，常见为 88+192=280B，较旧 400B 省 120B；多数植物没有回调，为 88B，省 312B；PotatoMine 只用实体碰撞侧车，为 216B，省 184B。未来若同一 Collider 同时需要两组侧车，总量为 408B，比旧布局多 8B；该罕见组合不应引用热对象数字声称节省。

以普通 26 轨僵尸、一个 follower 稀疏状态、触发侧车、Animator 与 Shadow 为代表，旧静态下限为 `568 + 392 + 26×152 + 400 + 56 = 5368B`，新静态下限为 `488 + 352 + 26×24 + 56 + 88 + 192 + 56 = 1856B`，约 5.24→1.81KiB，减少 3512B（65.4%）。这是同口径的对象/向量元素静态下限，不含共享控制块、向量容量、堆分配器元数据和 Reanimation 包装；也不是进程 RSS 或 FPS 数据。

## 所有权与行为契约

- Collider 的几何、掩码、注册 ID 与碰撞帧缓存留在 88B 热对象。触发器 enter/stay/exit 与实体碰撞 enter/exit 分为两个 `unique_ptr` 侧车；公开 setter 首次配置时分配，整组清空时释放。`CollisionSystem` 仍保持旧 enter/exit/stay 顺序和 trigger/collision 分派。
- 每条动画轨道常用的可见性、自定义纹理和偏移留在连续 `TrackExtraInfo`；只有 follower 或附加子 Animator 的轨道进入有序 `SparseTrackState`。绘制时以轨道索引线性合并，默认实例与 `-NoInstance` 顺序一致；同名轨道的广播语义保留。第一个轨道名索引归不可变 `Reanimation` 资源所有，并在实例之间共享，不再由每个 Animator 重建哈希表。
- `Zombie` 删除了与 `AnimatedObject::mBoard` 重复的 Board 指针。二十层毒计时器、活动计数与小数余量进入 `ToxinState`，只在首次中毒或读到有效旧档时分配；活动层压紧在前缀，到期交换删除，未中毒每帧 O(1) 早退，毒尽、魅惑和死亡统一释放。
- 毒素保存继续输出固定 20 项 `toxinLayerTimers` 和原 `toxinDamageRemainder`；加载旧档时保留有效计时并压紧。Collider、Animator 稀疏状态都是运行时派生或回调配置，不新增存档字段。

## 验证与边界

`clang-release` 完整构建、LTO 链接与 378 项 Win7 import audit 通过。桌面可见回归：`smoke_toxic_peashooter` 66 断言/10 截图，`smoke_zombie_butter_layers` 默认与 `-NoInstance` 各 79 断言/5 截图，`smoke_roof_marshal_assault_visual` 16 断言/2 截图，`smoke_imp_eat` 5 断言/1 截图，`smoke_potatomine` 7 断言/4 截图，均退出 0 且 `status=passed`。人工目验确认中毒 overlay、黄油层级、巨人遮挡、督军附件读档和地雷碰撞画面正常。

本阶段没有运行同场景迁移前后 A/B，也没有采集 L1/L2 miss、分支或 allocator 计数器。更小的工作集有利于缓存驻留是合理方向，但只有压力场景、ETW/采样器和硬件计数器才能确认实际收益；后续优化仍应先找高乘数常驻字段，避免为了缩小 `sizeof` 引入频繁分配或更差的遍历局部性。
