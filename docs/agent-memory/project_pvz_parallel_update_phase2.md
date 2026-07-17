---
name: pvz-parallel-update-phase2
description: phase-2 已完成（2026-05-23 commit 292f68e）；deferred-events 整 Animator 并行实测 -3.44ms / 69.3→91 FPS；vs phase-1 拆对了切点
metadata:
  node_type: memory
  type: project
  originSessionId: deferred
---

# PvZ 并行 Update phase-2（已完成，2026-05-23）

## 一句话结果
deferred-events 设计整个 Animator::Update 并行（callback 入 per-worker 队列、主线程串行 drain）→ 11000 zombie 实测 `total/frame 14.43→10.99 ms / 69.3→91 FPS / -3.44 ms` → KEEP。HEAD = `292f68e 阶段二并行 Update：deferred-events 完成`。

## 关键产出（已落盘 + commit）
- **Spec：** `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\superpowers\specs\2026-05-23-parallel-update-phase2-deferred-events-design.md`
- **Plan：** `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\superpowers\plans\2026-05-23-parallel-update-phase2-deferred-events.md`（7 Task 全打勾 + 结论/数据填好）
- **Commit：** `292f68e`（master，未 push；9 files / +227 / -29）

## 实测数据（11000 zombie，60-frame steady state）

| 段 | A 基线 (阈值 999999, fallback=原码) | B 阶段二 (阈值 200, 并行) | Δ |
|---|---|---|---|
| **total/frame** | 14.43 ms / 69.3 FPS | 10.99 ms / 91.0 FPS | **-3.44 ms** ✓ |
| 2.GOM_Update | 6.52 ms | 3.38 ms | **-3.14 ms** ← 核心收益 |
| 2c.PhaseB_DrainEvents | — | **0.00 ms** | drain 几乎免费 |
| 2d.PhaseB_serialUpdate | — | **2.53 ms** | 剩余串行段 (components+Glow) |
| 1.Particles_Update | 9.26 ms | 6.16 ms | **-3.10 ms** ← 意外间接受益 |
| 3.Collision_Update | 1.56 ms | 1.63 ms | +0.07 ms（噪声） |

## 关键 insight（写给下次会话）

1. **2c.DrainEvents = 0.00 ms** → 证伪 spec §9.2 担心的 `std::function<void()>` copy heap alloc 隐藏成本。11000 zombie 场景每帧实际触发的 frame events 数量很有限（zombies 多走 walk track，events 集中在少数帧），drain 队列基本是空的。**下次类似设计可放心用 std::function 入队，不必预先 SBO 化。**

2. **2d.PhaseB_serial = 2.53 ms** = phase B 剩余的串行成本（GameObject 组件遍历 + AnimatedObject::UpdateGlowingEffect + 自动销毁判断），印证 phase-1 教训"phase B 大部分是组件遍历"。**这就是下一阶段优化的明确目标。**

3. **Particles_Update 顺带 -3.10 ms** 是没动粒子系统的间接受益。猜测：(a) cache 热度差异；或 (b) PROFILE_SCOPE 之间穿插 frame 边界，前面快了它测得也快。**未深究——记下来下次有兴趣再 profile。**

4. **vs phase-1 教训印证：** phase-1 拆 AdvanceFrame（占 12%）= -0.82ms 负改善；phase-2 拆整 Animator::Update（占 66%）= -3.44ms 正改善。**正确假设位置就是一切。** dispatch 开销 (0.05ms) 在此项目几乎免费这条 phase-1 教训也再次验证（phase A 实际 ≈ 0.85ms 包含工作 + 同步）。

## 实施关键点（核心机制保留）

| 符号 | 文件 | 角色 |
|---|---|---|
| `DeferredEvent { std::function<void()> cb; }` | `Game/DeferredEvent.h` | event 入队结构 |
| `Animator::UpdateParallelDeferred(vector<DeferredEvent>&)` | `Reanimation/Animator.cpp` | worker 安全版 Update（与原 Update 等价，callback 改入队） |
| `GameObject::UpdateParallel(...)` virtual 默认空 | `Game/GameObject.h` | 派生类入口 |
| `AnimatedObject::UpdateParallel` + `mAdvancedInParallel` flag | `Game/AnimatedObject.{h,cpp}` | 双轨 Update：并行段推进 + 串行段跳过推进 |
| `GameObjectManager::mDeferredEventBuffers` | `Game/GameObjectManager.h` | per-worker event 队列 |
| 阶段 A `Dispatch` → B-1 drain → B-2 serial Update + `kParallelUpdateThreshold=200` fallback | `Game/GameObjectManager.cpp` | 两相执行框架 |

## Task 1 audit 结果（child Animator 跨父共享）

PASS。整个项目只有 **`Shooter::SetupPlant()` (Shooter.cpp:23)** 是实际 caller，`mHeadAnim = std::make_shared<Animator>` 每 Shooter 实例独占。`AnimatedObject::AttachAnimatorToTrack` 公开方法**零调用**（可作为后续清理候选）。`mAttachedReanims` 是 `weak_ptr` 容器，strong 持有权在父对象本地，并发安全前提强成立。

## 下一阶段方向（按 ROI 排序，基于本次数据）

1. ~~**2d.PhaseB_serial 2.53ms** —— 主目标~~ → **phase-3 已削（2026-05-23）**：FPS 91→100 / ~-1ms。同时实测发现本数字（2.53ms）已 stale，phase-3 期间同 HEAD 实测仅 1.43ms。详见 [pvz-phase3-component-update-skipping](project_pvz_phase3_component_update_skipping.md)。phase-3 后这块基本干净，剩 Animator::Update fallback + UpdateGlowingEffect + 自动销毁判断每段都很小，**不再建议投入**。
2. **Particles_Update 6.16ms** —— phase-3 后稳居 #1 大头，下一轮独立 profile 拆段。
3. **2c.DrainEvents 几乎免费** —— 不要再去优化，已经 0ms。

## 仓库状态
- HEAD = `292f68e 阶段二并行 Update：deferred-events 完成（-3.44ms / 69.3→91 FPS）` (master, ahead of origin/master by 1, 未 push)
- 改前 = `550457e 多线程TEST01 仅保留Plans和PROFILE`
- 临时验证脚手架 (Task 4 oracle / Task 5 验证模式) 已在 Task 7 全部清理，grep 零残留
- 唯一未跟踪改动 = `.claude/settings.local.json`（无关本地配置）

## 相关
[pvz-parallel-update-phase1](project_pvz_parallel_update_phase1.md) · [pvz-perf-optimization](project_pvz_perf_optimization.md) · [collaboration-style](feedback_collaboration_style.md)
