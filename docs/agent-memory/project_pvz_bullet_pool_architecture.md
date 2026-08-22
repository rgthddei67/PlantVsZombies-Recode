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
- `BulletPool` 继续按 `BulletType` 保存空闲下标，`Acquire` 与 `Release` 的渐近复杂度不变；合并空派生类是结构清理，不作为性能收益证据。
- `Reset` 必须清除伤害、速度、旋转、飞行模式、Animator、碰撞回调、火炬列、尖刺穿透与小数余量，并按槽位类型重建表现和阴影。
- 存档继续同时保存当前弹型与 `poolType`，读档先按槽位类型获取对象，再恢复运行时换型表现。

## 后续性能方向

CPU 缓存优化必须以同场景、同脚本的 `-Profile` 前后数据为准。当前更值得优先验证的是：让阴影阶段只遍历稠密活跃槽位；避免池历史高水位让 GOM 每帧扫描大量 inactive 对象；以池槽下标替代释放时的指针哈希；把尖刺、抛射和玉米棒等互斥专属状态压缩为无堆分配的小状态。跨弹型共用空闲槽位只有在 `Reset` 能原子重建 Shadow、Animator、碰撞阵营与全部专属状态后才可考虑。

## 验证

- `clang-release` 完整配置、98 步全量构建与 LTO 链接通过，Win7 x64 import audit 检查 378 项通过。
- 主人当前桌面可见运行 `smoke_projectile_shield_direction.json -Seed 42`：76 条命令、exit 0、`status=passed`、`script finished OK`，直接覆盖 Pea/SnowPea/Fireball/Puff/Spike 创建与碰撞；三张截图已逐张检查。
- 主人当前桌面可见运行 `smoke_torchwood.json -Seed 42`：90 条命令、exit 0、`status=passed`、`script finished OK`，覆盖火炬换型、SnowPea 退化、池槽复位、火球伤害/抗性与图鉴；五张截图已逐张检查。两次 `run.log` 均无 ERROR/WARN。
