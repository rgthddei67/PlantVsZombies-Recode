---
name: project_pvz_bullet_shadow
description: Bullet 地面阴影尺寸、对象池复用和跨对象绘制顺序
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-22
---

# Bullet 地面阴影

2026-07-19 完成；2026-08-22 迁移为 `GameObject` 显式拥有的可选 `ShadowComponent`，不再进入通用 Component 容器。阴影仍不跟随 Bullet 本体的 `LAYER_GAME_BULLET` 提交；`BulletPool::DrawShadows` 在 `GameObjectManager::DrawAll` 主体绘制前统一画所有活跃子弹阴影，因为宿主内部固定阶段同样无法跨对象层把 Bullet 阴影排到植物对象之前。

## C# 与视觉口径

- C# `Projectile.DrawShadow`：Pea 使用 `IMAGE_PEA_SHADOWS`；Snowpea 两轴 `1.3x`；Puff 直接返回、不画影子。
- Fireball 复用 Pea 阴影并按 C# 使用两轴 `1.4x`，X 偏移为 `0`。
- 原分辨率 `IMAGE_PEA_SHADOWS` 为日/夜两格共 `42x9`，所以 Pea 单格按 `21x9`；Snowpea 为 `27.3x11.7`。当前没有独立 projectile shadow 资源，复用 `IMAGE_PLANTSHADOW` 并非等比缩放到上述目标尺寸。
- X 偏移沿 C#：Pea 左边 `+3`、Snowpea 左边 `-1`。Y 经主人可见校对，中心与同一行豌豆射手默认影子一致，即 `格子中心 + 28`。
- 对象池复用会改变 row/position，`Bullet::Reset` 必须重新执行 `UpdateShadowLayout`，不能只在构造时计算一次。
- Torchwood 会让池内 Pea 在运行时换成 Fireball；`ConfigurePresentation` 完成换型后也必须立即重算阴影，回收复用恢复 `mPoolType` 时同理。

## 绘制与验证

- `Bullet::Draw` 只画本体；`Bullet::DrawShadow` 通过 `GetShadow()` 取得宿主独占附件，仅供池的地面阴影预绘制阶段调用，避免重复绘制。
- 串行路径中预绘制天然先于所有 GameObject；并行路径随后由 `Graphics::BeginParallelRecord` 先 Flush 主线程批次，因此同样保持 shadow → 植物/僵尸/子弹本体的顺序。
- clang-release 编译通过。可见 AutoTest `demo_peashooter.json` 验 Y；`smoke_bullet_shadow.json -Seed 42` exit 0，`bulletCount=2`，截图中普通/寒冰子弹本体压在坚果上，而阴影被坚果遮挡，证明跨对象顺序正确。仍须同时核对 `run.log`、state JSON 和 PNG。
