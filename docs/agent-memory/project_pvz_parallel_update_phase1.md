---
name: pvz-parallel-update-phase1
description: phase-1（Animator AdvanceFrame 并行）已执行并按 GATE REVERT；记录数据、教训、为什么"拆错了点"
metadata:
  node_type: memory
  type: project
  originSessionId: 5f37e182-5446-45c0-aaa4-13d30e09e2b1
---

# PvZ 并行 Update phase-1（已 REVERT，2026-05-23）

## 结果一句话
按 plan 完整执行 Task 0-5 + GATE 实测；A/B 对比显示**负改善**（GOM_Update +0.87 ms / total +0.82 ms），按 §6 决策规则 → **REVERT**。working tree 已 git restore 回 097df77 起点，只留 Task 0 的 6 处 PROFILE_SCOPE 取消注释。

## 实测数据（11000 僵尸，Release x64，vsync OFF，60-frame avg）

| 维度 | Task 0 baseline | A 强制串行 | B 并行开 | 诊断（B 拆分） |
|---|---|---|---|---|
| total / frame | 15.18 | 14.54 | 15.36 | 15.92 |
| 2.GOM_Update | 7.09 | 6.63 | 7.50 | 7.58 |
| 2b objUpdateLoop | — | 6.63 | 7.50 | 7.58 |
| **2c AdvanceFrame(par+dispatch)** | — | — | — | **0.85** |
| **2c2 dispatch_emptyCal** | — | — | — | **0.05** |
| **2d serialPhaseB** | — | — | — | **6.67** |

## 为什么 REVERT —— 拆错了点

**plan §"设计核心"假设 "Animator 帧推进占 Update 80%"，实测只占 ~12%。**

诊断数据破解：
- AdvanceFrame 实际并行工作 = 2c − 2c2 = **0.80 ms**（10000 对象的全部并行 work）
- dispatch 纯开销 = **0.05 ms**（ThreadPool 唤醒/同步几乎免费，原"dispatch overhead 吃光收益"假设是错的）
- 阶段 B 串行 = **6.67 ms** ≈ A 基线 6.63 ms（拆出 AdvanceFrame **几乎没让 phase B 减少**）
- 总成本 = 0.85 (phase A) + 6.67 (phase B) = **7.52 ms** vs 原 6.63 ms = 多了 0.89 ms（拆分本身的开销：额外函数调用 / 两次进入 Animator / 函数内联失效）

**Why:** Animator::Update 的真正工作不在"推帧"（标量算术 + 几个 if），而在 phase B：
- FireFrameEvents（mFrameEvents 多重 map 查表 + 回调）
- UpdateGlowingEffect / GameObject::Update（组件 vector 遍历）
- IsAnimationFinished / 自动销毁判断
- 子节点 Animator 递归（这部分在 AdvanceFrame 和 FireFrameEvents 都做了一遍——重复遍历的开销不小）

**How to apply:**
1. **不要假设瓶颈位置**——下次任何 perf plan 起手必须先 profile 拆段（Task 0 等价物），用数据定瓶颈再决定方向。本次教训：5 天前 plan 写时没拆段就假定 80%。
2. **dispatch 开销不是问题**——ThreadPool 在这里实际开销只有 0.05ms，未来设计可以放心 dispatch 数千次。瓶颈通常是"可并行工作量太少"或"phase B 必须串行的工作太多"。
3. **下次方向**：phase B 6.67ms 的真实组成需要再拆段才能知道——但其中大部分（FireFrameEvents 调音频/spawn/destroy/RNG、GameObject 组件遍历）**本质上必须串行**。继续在 Update 维度做并行 ROI 已经很低。

## 下次更可能有收益的方向（用户决策时参考）

按 ROI 排序（基于本次数据）：

1. **算法层削减工作**（最高 ROI）：
   - Zombie 行进静态时跳过 Update（最远 zombies 已停在墓地附近）
   - Plant 没敌人在行时跳过 shooting 检查（需要 per-row zombie 索引，plan 头部"挂后续"那条）
   - AnimatedObject::UpdateGlowingEffect 只对 `mGlowingTimer > 0` 的对象跑（已经是 if 守卫，但还可以从 Update 链路移到独立 GlowSystem，避免每个对象进函数）

2. **Animator 帧事件批量化**：把 mFrameEvents 从 unordered_multimap 改成 sorted vector + 帧索引线性扫描——对 4000+ 帧事件的 zombie 群可能显著降低 phase B 内的 FireFrameEvents 成本。需要先 profile 确认是否是瓶颈。

3. **Phase B 内的对象并行**（高难度）：把 phase B 拆成"可并行业务逻辑"（如 GlowEffect、TransformComponent 更新）和"必须串行"（音效/spawn）。但本次诊断显示 phase B 大部分时间在不可分割路径上。

## 仓库状态
- working tree = 097df77 + Task 0 的 14 行 PROFILE_SCOPE 取消注释（GameObjectManager.cpp 10 行 / Scene.cpp 4 行）
- 用户预存改动 `GameApp.cpp` 与 phase-1 无关，未动
- `~/.claude/plans/10000-update-66fps-imperative-wand.md` 含 Baseline 测量记录；可作历史参考保留

## 相关
[pvz-perf-optimization](project_pvz_perf_optimization.md) · [collaboration-style](feedback_collaboration_style.md) · [pvz-phase6-opengl-cleanup](project_pvz_phase6_opengl_cleanup.md)
