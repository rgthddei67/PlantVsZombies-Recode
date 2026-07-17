---
name: project_pvz_collision_seeker_target_fix
description: 2026-07-01 碰撞 O(k²) 僵尸×僵尸空转修复(seeker/target 拆分)已合并 master 未 push；20000 僵尸 Collision 5.07→2.17ms / 73.8→106.9 FPS
metadata:
  node_type: memory
  type: project
  originSessionId: 5d1440f7-e821-4df9-88a8-b37f2be4e916
---

2026-07-01：修掉 20000 僵尸下 `Collision_Update` 的超线性瓶颈。已合并 **master(fast-forward，7e2ae95..045704b，8 commits，未 push)**，clang-release 0 warn/err，AutoTest 冒烟绿，主人真实 20000 场景 -Profile 确认。

**根因(实测坐实，非推断)**：`CollisionSystem::Update` 每行 sweep-and-prune 在僵尸高度堆叠时退化逼近 O(k²)；且僵尸 collisionMask=PLANT|BULLET|MOWER **不含 ZOMBIE**(`Zombie.cpp:45-46`)→ 僵尸×僵尸 `CanCollide` 恒 false → 内层循环遍历所有 x 重叠僵尸对只为确认"不该碰"。加 `-Profile` 探针实测：`sweepIter=sweepReject=9,605,974/帧、check=hit=0`(精确 100% 空转)。同模式见 [project_pvz_trig_lut_gpu_suggestion_verdict](project_pvz_trig_lut_gpu_suggestion_verdict.md)/[project_pvz_gemini_perf_report_verdict](project_pvz_gemini_perf_report_verdict.md)——瓶颈靠 profiler 非读代码。

**修法(Approach A，只改 `CollisionSystem.h`)**：每行动态碰撞体按 `layerMask==ZOMBIE` 拆成 `mRowZombies`(被动目标，多)/`mRowOthers`(seeker：子弹/割草机，少)；`zb` 按 x 排序，少数 seeker + 静态植物 `std::lower_bound` 二分进 `zb`(下界 `left - mRowMaxZombieW[row]` 防漏宽僵尸)，**僵尸×僵尸整段不扫**。pair 集合与旧代码逐位相同(子弹×僵尸/割草机×僵尸/僵尸×植物)→ 回调链/存档零改。O(k²)→O(少数·log k)。

**结果**：`sweepIter 9.6M→0`、`Collision 5.07→2.17ms`、`total 13.55→9.35ms(73.8→106.9 FPS)`。残留 2.17ms 是碰撞 **O(n) 底噪**(给 2 万碰撞体缓存世界坐标+AABB、分桶、每行排序)，无超线性项——修更新 [project_pvz_perf_optimization](project_pvz_perf_optimization.md) 里"Collision 已按行并行"那条(现瓶颈已消)。

**关键 foot-gun(final review 才抓到，两阶段 per-task review 漏了)**：`HandleCollisionEnter(a,b)` **先触发 a 再触发 b，回调有先后**。旧僵尸×植物恒压 `{dynamic=僵尸, static=植物}`(僵尸 a)；新 `sweepAgainstZombies` 一度压 `{static, zombie}` 翻转了次序 → 对"接触即杀"植物(PotatoMine `onCollisionEnter→zombie->Die()`)，`Die()` 会先于 `StartEat` 跑，而 `Die()` 的 `mEaterCount` 清理(`Zombie.cpp:485`)依赖"先 StartEat(设 mIsEating) 后 Die"→ mEaterCount 短暂失衡 + 在已死僵尸上跑 StartEat。修=僵尸恒放 a(`{ zb[i], s, key }`)。**教训：pair (a,b) 顺序对"接触即杀/连锁"回调是可观测的，别信"顺序无所谓"；smoke 里没 PotatoMine 就漏，holistic 终审值回票价。**

**魅惑钩子(spec §4，只设计不实现)**：未来"魅惑僵尸啃普通僵尸"给魅惑一个独立 `CHARMED` 层→build 时落进 `mRowOthers`、走同一二分路径搜 `zb`，普通僵尸仍纯目标、魅惑数量少不引入 O(k²)。当前魅惑僵尸只反向右走+植物无视(`mIsMindControlled`)，啃普通并不存在。

**诊断探针**留在 master 的 `-Profile` 之后(关闭时仅一个高度可预测分支，近零开销)：`FRAME PROFILE` 多 `sweepIter/sweepReject/sweepCheck/sweepHit` 四行(CollisionSystem 每行独占槽位累加→派发屏障后主线程 `Profiler::CountSweep` 汇总)。主人若要删随时说。spec/plan：`docs/superpowers/specs|plans/2026-07-01-collision-seeker-target-partition*`。

**潜伏项(未修，本次范围外)**：`mNoRowDynamic` 串行段仍是 O(noRow×n)(常空)。
