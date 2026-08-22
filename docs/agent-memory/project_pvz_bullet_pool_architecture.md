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

2026-08-22 进一步为 `BulletPool` 增加 `mActiveIndices` 稠密表，每个池槽保存反向 `activeListIndex`。Acquire 把槽位追加到活跃表；Release 以末项交换 O(1) 移除并更新被移动槽位，重复 Release 会被拒绝，避免活跃计数变负或同一槽位重复进入空闲表。活跃计数直接由稠密表长度派生，`DrawShadows` 只遍历活跃槽位，历史峰值 256、当前活跃 64 时不再扫描其余 192 个 inactive 槽位。

池统计口径同时修正为 hit=从分型空闲表复用，miss=无空闲槽、必须创建新 `Bullet`；旧实现把新建也计为 hit 且 miss 永远为 0，不能用于优化决策。AutoTest 根状态导出 storage/active/peak/hit/miss、千分比命中率和活跃槽位双向一致性。`spawn_bullet` 新增 `count=1..512` 与 `xStep/yStep`，由 `stress_bullet_pool_active_slots.json` 可重复覆盖 256 发新建、全部回收、64 发复用与两帧窗口性能取证。

## 后续性能方向

CPU 缓存优化仍须以同场景、同脚本的 `-Profile` 前后数据为准。下一步优先测量 GOM 是否因池对象历史高水位而在 Update/Draw 排序中扫描大量 inactive 对象，再评估用稳定池槽下标替代释放时的指针哈希，以及把尖刺、抛射和玉米棒等互斥专属状态压缩为无堆分配的小状态。跨弹型共用空闲槽位只有在 `Reset` 能原子重建 Shadow、Animator、碰撞阵营与全部专属状态后才可考虑；连续块分配和 GOM 所有权改造不应在现有压力基线之前先行。

## 验证

- `clang-release` 完整配置、98 步增量全头文件重编译与 LTO 链接通过，Win7 x64 import audit 检查 378 项通过。
- 主人当前桌面可见运行 `stress_bullet_pool_active_slots.json -Seed 42 -Profile`：42 条命令、exit 0、`status=passed`；256 miss→活跃归零→64 hit，`ActiveSlotsValid` 始终为 true。最终 64 发截图逐张检查，豌豆与跨对象地面阴影正常；稳定窗口 `5a.Draw_bulletShadows` 约 0.02ms/帧，这只是后续前后对照基线，不是无对照的收益声明。
- 主人当前桌面可见运行 `smoke_projectile_shield_direction.json -Seed 42`：76 条命令、exit 0、`status=passed`、`script finished OK`，直接覆盖 Pea/SnowPea/Fireball/Puff/Spike 创建与碰撞；三张截图已逐张检查。
- 主人当前桌面可见运行 `smoke_torchwood.json -Seed 42`：90 条命令、exit 0、`status=passed`、`script finished OK`，覆盖火炬换型、SnowPea 退化、池槽复位、火球伤害/抗性与图鉴；五张截图已逐张检查。两次 `run.log` 均无 ERROR/WARN。
