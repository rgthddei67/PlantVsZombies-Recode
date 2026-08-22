---
name: project_pvz_bullet_pool_architecture
description: 单一 Bullet 具体类型、分型对象池合同与后续缓存优化边界
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-22
---

# 单一 Bullet 与分型对象池

2026-08-22 将 `PeaBullet`、`SnowPeaBullet`、`PuffBullet`、`FirePeaBullet`、`SpikeBullet` 五个仅继承构造函数、没有覆写或专属状态的空派生类删除。`Bullet` 设为 `final`，所有已具备完整表现与碰撞合同的弹型由 `BulletPool` 统一创建同一具体类型；预留但尚未接入表现/碰撞的 `BULLET_ZOMBIE_PEA` 继续拒绝创建，本次不改变玩法范围。

## 保持的合同

- `BulletType` 仍是当前玩法/表现类型，`mPoolType` 仍是固定池槽类型；豌豆经火炬树桩变火球时不得改变槽位归属。
- `BulletPool` 继续按 `BulletType` 保存空闲下标；合并空派生类本身只是结构清理，不作为性能收益证据。
- `Reset` 必须清除伤害、速度、旋转、飞行模式、Animator、碰撞回调、火炬列、尖刺穿透与小数余量，并按槽位类型重建表现和阴影。
- 存档继续同时保存当前弹型与 `poolType`，读档先按槽位类型获取对象，再恢复运行时换型表现。

## 稠密活跃槽位

2026-08-22 进一步为 `BulletPool` 增加 `mActiveIndices` 稠密表，每个池槽保存反向 `activeListIndex`。Acquire 把槽位追加到活跃表；Release 以末项交换 O(1) 移除并更新被移动槽位，重复 Release 会被拒绝，避免活跃计数变负或同一槽位重复进入空闲表。活跃计数直接由稠密表长度派生，`DrawShadows` 只遍历活跃槽位，历史峰值 512、当前活跃 64 时不再扫描其余 448 个 inactive 槽位。

池统计口径同时修正为 hit=从分型空闲表复用，miss=无空闲槽、必须创建新 `Bullet`；旧实现把新建也计为 hit 且 miss 永远为 0，不能用于优化决策。AutoTest 根状态导出 storage/active/inactive/peak/hit/miss、千分比命中率和活跃槽位双向一致性。`spawn_bullet` 支持 `count=1..512` 与 `xStep/yStep`，由 `stress_bullet_pool_active_slots.json` 可重复覆盖 512 发新建、全部回收、64 发复用；三段各保留 180 帧稳定性能窗口。

## 所有权与稳定槽位

2026-08-22 的所有权审计确认 `Bullet` 不持有 `BulletPool`，因此池槽由 `weak_ptr` 改为 `shared_ptr` 不会形成循环；池与 GOM 共同持有运行时对象，`Clear` 先把弹丸运行时槽位置为无效并交给 GOM 销毁队列，再释放池引用。这样活跃阴影遍历可直接解引用，避免逐帧 `weak_ptr::lock()` 的原子引用计数操作。

每颗 `Bullet` 保存私有 `mPoolSlotIndex`：只在新建槽位时赋值，复用 `Reset` 不清除，且不进入存档。池的 `vector` 下标在扩容后仍稳定，Release 先校验范围和 `slot.bullet.get()==bullet` 再 O(1) 回收，因此删除了每颗弹丸一项的 `unordered_map<Bullet*, int>` 节点和桶数组。活跃槽位一致性投影同时验证强指针、正反索引和 Bullet 内槽位三者一致。

## 历史高水位与并行阈值

基线证明主要浪费不是 512 个 `shared_ptr` 的线性判活本身，而是 GOM 用总存储量判并行：512 颗全部休眠后，Update 仍约花 0.05～0.07 ms/逻辑调用执行并行阶段，Draw 仍约花 0.06～0.08 ms/渲染帧做 parallel record。现在 Update/Draw 的并行阈值只把 `total - BulletPool::inactive` 作为候选数量；实际遍历、render order、附件生命周期和高活跃时的并行切片均未改变。512 休眠时 Update 并行阶段消失、GOM 循环舍入为 0.00 ms/调用，Draw 改走 0.02～0.03 ms/帧的串行提交；64 活跃+448 休眠时 GOM 约 0.01 ms/调用、Draw 约 0.02 ms/帧。512 活跃仍走原并行路径，阴影阶段仅从常见 0.03～0.04 ms/帧变为约 0.03 ms/帧，不能把它宣传为显著帧率收益。

CPU 缓存优化继续以同场景、同脚本的 `-Profile` 前后数据为准。当前不把 GOM 拆成 SoA、不把池弹丸移出 GOM，也不压缩尖刺/抛射/玉米棒专属状态：现有数据只支持消除错误 dispatch 与哈希/weak-lock 开销，尚不支持扩大生命周期改造。后续只有在真实高活跃弹幕中出现可重复的 Bullet Update/Draw 热点时，再评估冷热字段拆分或无堆小状态。

## 验证

- `clang-release` 完整配置、增量编译与 LTO 链接通过；Win7 x64 import audit 检查 378 项通过。
- 主人当前桌面可见运行 `stress_bullet_pool_active_slots.json -Seed 42 -Profile`：55 条命令、39 项断言、exit 0、`status=passed`；512 miss→512 inactive→64 hit，`ActiveSlotsValid` 始终为 true，最终 storage/active/inactive=`512/64/448`、hit/miss=`64/512`。最终 64 发截图检查，豌豆与跨对象地面阴影正常；前后性能数据见上节。
- 主人当前桌面可见运行 `smoke_projectile_shield_direction.json -Seed 42`：76 条命令、exit 0、`status=passed`、`script finished OK`，直接覆盖 Pea/SnowPea/Fireball/Puff/Spike 创建与碰撞；三张截图已逐张检查。
- 主人当前桌面可见运行 `smoke_torchwood.json -Seed 42`：90 条命令、exit 0、`status=passed`、`script finished OK`，覆盖火炬换型、SnowPea 退化、池槽复位、火球伤害/抗性与图鉴；五张截图已逐张检查。两次 `run.log` 均无 ERROR/WARN。
- 主人当前桌面可见运行 `smoke_melonpult.json -Seed 42`：139 条命令、57 项断言、exit 0、`status=passed`，覆盖池弹丸的中途快照销毁/重建、`fromPool/poolType`、抛物线、溅射和屋顶；关键截图检查正常。四项当前 `run.log` 均无测试失败或运行时错误。
