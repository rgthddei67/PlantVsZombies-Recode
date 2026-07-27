# PvZ 最终绘制坐标语义取证

## 结论（2026-07-27）

新增植物、僵尸、动画特效或粒子时，C# 原版 800×600 的绝对坐标只用于确认“锚到谁、相对偏多少、何时出现”，不得直接成为当前 1100×600 项目的实现值或 AutoTest 期望值。

AutoTest 已能从当前项目实际渲染路径取得坐标证据：

- 所有 `AnimatedObject`（植物、僵尸、动画子弹、动画特效）的根 `Animator::Draw` 聚合本帧实际提交的最终世界四边形。
- 默认 GPU 实例化与 `-NoInstance` 矩阵慢路径写入同一 `AnimatorRenderProbe`；探针只在 AutoTest 模式启用。
- 每个粒子发射器在 `DrawTextureRegion` 前按同一最终矩阵记录粒子四边形，`ParticleEffect` 再聚合多发射器包围盒；探针同样只在 AutoTest 模式启用。
- `screenshot` ticket 是落盘屏障，因此脚本可先截图，再在同一稳定取证点断言最近一帧的渲染探针。

## 状态字段

`dump_state` / `assert_state` 的根节点提供：

- `animatedObjectsByTag.<Tag>.N`
  - `renderProbeReady`、`renderPath`、`renderQuadCount`
  - `renderBase*`、`renderScale*`、`renderPivot*`
  - `worldBounds`
  - `visualToRenderCenterDxInt/DyInt`
  - `nearestPlant` / `nearestZombie`（中心差与 `boundsIntersect`）
- `particleEffectsByName.<Name>.N`
  - `origin*`、`renderProbeReady`、`renderQuadCount`
  - `worldBounds`（裁剪前实际提交几何）
  - `originToRenderCenterDxInt/DyInt`
  - `clipRightXInt`
  - `nearestPlant` / `nearestZombie`

数组顺序来自运行时容器。专项脚本应让每个待测 tag/effect name 只存在一个目标实例，或先断言对应 count，再索引 `.0`。

## 稳定断言口径

1. 先建立静止或可重复的目标状态，再执行同步 `screenshot`。
2. 断言 `renderProbeReady=true` 与正的包围盒尺寸，排除“状态存在但根本没画”的假绿。
3. 断言相对稳定锚点的整数投影：植物对格子/自身 collider，僵尸对自身 collider/目标植物，命中特效和粒子对被击实体。
4. 随机粒子用合理区间；移动对象的瞬时绝对 X/Y 只供诊断。
5. 修改 Animator 附件、翻转、pivot、整株世界变换或实例记录时，用同一静止脚本分别跑默认与 `-NoInstance`，比较整数 `worldBounds`。
6. 最后逐张查看截图，确认自动语义断言与肉眼脚底、命中点、铺开方向一致。

## 首个实证

- `smoke_animated_object_probe.json`：同一株火炬树桩、普通僵尸和 `FireballImpact` 在默认/`-NoInstance` 下的整数世界包围盒逐项一致；火焰命中特效与僵尸 collider 相交。
- `smoke_particle_render_probe.json`：`PeaBulletHit` 的多发射器实际粒子包围盒相对发射点落在小范围内，并与命中僵尸 collider 相交。

## 关联

- `[[project_pvz_autotest_harness_enhancements]]`
- `[[project_pvz_torchwood_firepea]]`
- `[[project_pvz_particle_render_layer]]`
