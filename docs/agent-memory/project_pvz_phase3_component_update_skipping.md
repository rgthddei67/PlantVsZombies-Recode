---
name: pvz-phase3-component-update-skipping
description: phase-3 完成（2026-05-23）—— NeedsUpdate virtual + mUpdatableComponents 视图跳过空虚调；11000z FPS 91→100 /总减 ~1ms；plan 子桶 GATE 漏抓 end-to-end 信号是关键教训
metadata:
  node_type: memory
  type: project
  originSessionId: 3f901516-d155-46fe-b003-57f9a978a434
---

# PvZ phase-3 跳过空 Component::Update 浪费虚调（已完成，2026-05-23）

## 结果一句话
`Component::NeedsUpdate()` virtual（默认 false）+ `GameObject::mUpdatableComponents` 缓存视图 → `GameObject::Update()` 只 iterate 真正需要 Update 的组件 → 11000 zombie 实测 **FPS 91 → 100 / total/frame ~10.99 → ~10.00 ms / 净 ~1.0 ms**。KEEP。HEAD = phase-3 完成 commit（紧跟 phase-2 commit `292f68e`）。

## 关键产出
- Spec: `docs/superpowers/specs/2026-05-23-phase3-component-update-skipping-design.md`
- Plan: `docs/superpowers/plans/2026-05-23-phase3-component-update-skipping.md`（6 Task 全打勾）
- 代码改动：
  - `Component.h` 加 `virtual bool NeedsUpdate() const { return false; }`
  - 4 个派生类 `.h` 加 `bool NeedsUpdate() const override { return true; }`：Click / Card / CardDisplay / CardSlotManager
  - `GameObject.h` 加 `std::vector<Component*> mUpdatableComponents` + AddComponent / RemoveComponent template 各加 1 处维护
  - `GameObject.cpp` `Update()` 切换为 vector iteration + `DestroyAllComponents()` 加 `mUpdatableComponents.clear()`
  - 临时拆段诊断（`AnimatedObject.cpp` 内 2da/2db/2dc PROFILE_SCOPE + Profiler.h include）已清

## 实测数据（11000 zombie 30s 稳态）

| 字段 | A 基线 (phase-2 HEAD 无 scope) | B phase-3 (无 scope) | Δ |
|---|---|---|---|
| **FPS** | 91 | 100 | **+9** |
| total/frame（推算 1000/FPS） | ~10.99 ms | ~10.00 ms | **-1.0 ms** ✓ |
| 2.GOM_Update | 2.46 ms | 2.38 ms | -0.08 ms |
| 2b.GOM_objUpdateLoop | 2.46 ms | 2.38 ms | -0.08 ms |
| 2d.PhaseB_serialUpdate | 1.43 ms | 1.45 ms | +0.02 ms（噪声） |

**带 scope 的 B 测量（已抛弃，仅留 2da 子桶证据）：** `2da.components = 0.25 ms`（phase-3 后 `GameObject::Update` 路径几乎归零，符合 spec §1 架构预期：多数 GameObject 的 mUpdatableComponents 是空 vector，outer loop 直接退出）。

## 关键 insight（写给下次会话——这次工作真正值钱的部分）

### 1. 子桶 GATE 漏抓 end-to-end 信号 —— 下次锁 FPS / total/frame 判 KEEP
**Why:** spec §7.2 锁 `2da.components` 削减 ≥ 1.5 ms 判 KEEP，但 phase-3 的削减分散到**整个 frame 的多个字段**——`GameObject::Update` 不止 zombie 走，Card/Scene/UI 等也走，它们的削减分散到不同 profile 桶。本次单看 `2d.PhaseB_serial` 是 +0.02ms（噪声），我据此最初判 NO-KEEP，是用户 FPS 91→100 的报告才翻转。
**How to apply:** 写 perf GATE 一律用 `total/frame` 或 FPS 作**主判据**，子桶削减作辅助诊断。即使 plan 已经写了子桶判据，到执行时也要追问"FPS 怎么样"再下结论。

### 2. PROFILE_SCOPE 装置自身污染高达 ~4.6 ms / 11000 zombie / 3 嵌套 scope
**Why:** spec §7.3 估算 ~2.5 ms 测量开销，实测 4.64 ms（推算自 `2d=5.51ms` 但子和 `2da+2db+2dc=0.87ms`，差额是 scope enter/exit 开销）—— **偏低 ~80%**。这就是用户"加了这个掉帧"的物理原因。装置污染了被测对象，本次让 plan §7.2 "A/B 都带 scope 同刻度对比"路径彻底走不通。
**How to apply:** (a) 拆段 PROFILE_SCOPE 不要嵌套 3 层以上；(b) 高频对象场景（>1k）埋点前先单独测装置开销；(c) A/B 测量必须用**同一套**装置，不能 A 已删 scope / B 还带着。

### 3. phase-2 内存 baseline 数据已经不成立 —— 性能数据有保质期
**Why:** [pvz-parallel-update-phase2](project_pvz_parallel_update_phase2.md) 记录 `2d.PhaseB_serial = 2.53 ms`（commit 292f68e 当晚），本次实测同一 HEAD 是 1.43 ms，**漂移 1.1 ms**。可能原因：编译器/驱动更新、Drain Events 内核效应、cache 状态、其它非显式优化副作用。Plan 据此假设 phase-3 能削 ~2.5 ms 已经不成立。
**How to apply:** 开新 phase 前**必须先重测当前 baseline**，不要直接信旧 memory 里的数字。Memory 里的数据加个"测于 YYYY-MM-DD"时间戳，超过 ~1 周一律重测。

### 4. 现代 CPU 分支预测对"恒空虚调"非常友好
**Why:** spec 假设 11000 × 3 = 33000 次虚调到空 body 浪费 ~2.5 ms，实测分量远小——`vtable lookup → indirect call → tiny ret` 在分支预测器命中后几乎零成本。phase-3 真实削减分散在多个二阶效应（cache friendly vector 替代 unordered_map iteration、相邻代码 prefetch 改善等）。
**How to apply:** 虚调成本不要预先想当然估"几 ns"。如果优化目标是"消除虚调浪费"，先用 probe 实测虚调本身的成本（比如把虚调改成 inline 直调测一次差异）再决定值不值得做架构改动。

## 仓库状态
- HEAD = `c435a57 阶段三完成：跳过空 Component::Update 浪费虚调（FPS 91→100 / -~1ms）`（master，未 push，10 files / +697 / -6）
- 改前 HEAD = `292f68e 阶段二并行 Update：deferred-events 完成`
- 7 文件改动 + AnimatedObject.cpp 拆段诊断清理
- 临时 `Profiler.h` include / 2da/2db/2dc PROFILE_SCOPE 全清

## 下一阶段方向（按 ROI 排序，基于本次数据）

1. **`1.Particles_Update` 6.16 ms** —— phase-2 后稳居 #1 大头，phase-3 没动。下一轮独立 profile 拆段（emitter count / particle count / per-frame work 分布）。
2. **`6.Draw_submit` ~4-9 ms**（场景相关） —— 旧瓶颈，本质 memory-bandwidth bound 已验证（见 [pvz-perf-optimization](project_pvz_perf_optimization.md) PROBE RESULT 段）；Design B 跨核 preallocated-offset fill 是唯一可能突破点。
3. **`2d.PhaseB_serial` 1.4-1.5 ms** —— phase-3 后基本干净，剩 Animator::Update fallback + UpdateGlowingEffect + 自动销毁判断，每段都很小。**不建议继续投入**。

## 相关
[pvz-perf-optimization](project_pvz_perf_optimization.md) · [pvz-parallel-update-phase2](project_pvz_parallel_update_phase2.md) · [pvz-parallel-update-phase1](project_pvz_parallel_update_phase1.md) · [collaboration-style](feedback_collaboration_style.md)
