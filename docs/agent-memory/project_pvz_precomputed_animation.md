---
name: pvz-precomputed-animation
description: 评估后放弃的 PvZ perf 方向 — 预计算 Reanim 骨骼动画。Risk #1（"reanim 已经 per-frame 缓存"）实测命中，预期 wall 收益从 1.2-1.7ms 下修到 0.35-0.7ms，用户决定放弃
metadata:
  node_type: memory
  type: project
  originSessionId: c8539e6f-a524-4eb7-90c7-61a142d01a1e
---

# 预计算动画 (precomputed Reanim sampling) — 已评估后放弃 (2026-05-23)

**起点 2026-05-23，同日终止。** 用户朋友提了三条优化建议，用户选定预计算动画作为下一档。**评估"开工第一件事"后用户决定放弃**——读代码证实风险 #1 命中，可获得 wall 收益不足。

**Why（放弃理由，按今天的发现）:** plan 设计基于"reanim 加载时只存稀疏关键帧、运行时做关键帧搜索 + trig + mat4 ctor"，实测代码不是这样。`TrackInfo::mFrames` 已经是密集 per-frame 数组（见下），关键帧搜索这一项**根本不存在**。剩下可攻击的只有 4× cosf/sinf（~6ms CPU = ~0.7ms wall after 8.77x parallel），SSE sincos 只能减半（~0.35ms wall）。100→104-107 FPS，ROI 远低于 phase-2/3。

**How to apply:** 不要再启动这个方向。如果未来 reanim 资源结构改变（比如改回稀疏关键帧 + 运行时插值），可以重新评估；当前结构下没有意义。GPU instancing 方向（见 [pvz-perf-optimization](project_pvz_perf_optimization.md) 末尾）依然是未来的下一档候选，但工程量 1-2 周、ROI ~2.8ms wall——除非需求出现（144 FPS+ 或 20000+ 僵尸），也不主动启动。

---

## 评估证据（2026-05-23 session 读代码所得，未跑实验）

### 实际 Reanim 数据结构

**`ReanimTypes.h:30-37`** — `TrackInfo` 已经是密集 per-frame：
```cpp
struct TrackInfo {
    std::string mTrackName;
    bool mAvailable = true;
    std::vector<TrackFrameTransform> mFrames;   // <-- per-frame 数组，不是稀疏关键帧
};
```

**`TrackFrameTransform`**（ReanimTypes.h:15-27）：每帧存 `x/y/kx/ky/sx/sy/a/f/image`——SoA 参数形式，不含 trig，不含 mat4。这等同于 plan 设计稿里想烘焙的 `PreSampledFrame`（plan 多两个字段，本质一致）。

### `GetInterpolatedTransform`（Animator.cpp:739-775）

```cpp
int frameBefore = static_cast<int>(mFrameIndexNow);  // O(1) 数组下标
float fraction = mFrameIndexNow - frameBefore;
GetDeltaTransform(track->mFrames[frameBefore],
                  track->mFrames[frameAfter], fraction, result);
```

无关键帧二分搜索。无 trig。**就是参数 lerp。** plan 中 "6d.interp(34%) 是 trig + mat4 ctor 的目标"——位置定错了，trig 和 mat4 ctor 不在 interp 里。

### trig 和 mat4 ctor 的真实位置

`DrawInternal`（Animator.cpp:331-435）：
- 348-353 行：4× cosf/sinf（angleX/angleY 各 2 个）
- 354-357 行：tA/tB/tC/tD = cos/sin × scale
- 379-384 行：glm::mat4 16 字段直接构造
- 这些每 track 每实例每帧都跑——11000 zombie × ~15 track ≈ 165k/frame

### 重新估算 wall 收益

| 段 | CPU sum | 并行加速比 | wall 等价 |
|---|---|---|---|
| 关键帧搜索 | 0 | — | **0**（不存在） |
| 7 个 float lerp | ~2-3 ms | 8.77x | ~0.3 ms |
| 4× cosf/sinf | ~6 ms | 8.77x | **~0.7 ms** ← 唯一可拿 |
| mat4 ctor | ~3 ms | 8.77x | ~0.3 ms（编译器很可能已 SIMD） |
| 框架/递归/cache miss | ~13 ms | 8.77x | ~1.5 ms（难攻击） |

**SSE sincos** 砍 trig 50% = **~0.35 ms wall**（FPS 100→104）。
**预算 tA-tD** 进 mFrames（fraction==0 时跳过 trig）= **~0.5-0.7 ms wall**（FPS 100→106-107），但需要插值精度妥协（lerp tA-tD 不等于 lerp 角度后 trig）+ fallback path。

memory plan 末尾自己写的"SSE sincos 仅 ~0.4ms wall 收益"和上面估算吻合——plan 写的时候已经预感到，只是主体 1.2-1.7ms 估算来自"假设关键帧搜索还在"。

---

## 不再调用的设计草图（仅作存档，不要重启）

原 plan 的烘焙 + 数组查表方向已被代码现状证伪——重做一遍已经做的事。如果未来 Reanim 改回稀疏关键帧需重新评估。其他 plan 草图（SoA 不存 mat4 / 保留 fallback / 子动画递归改表查询）也已无意义。

---

## 排除的另外两条（朋友的建议，仍然排除）

不变：indirect draw（submit 仅 29%，得叠 GPU instancing 才有意义）/ 遮挡剔除（俯视 2D 无遮挡）。详细同前。

---

## 历史背景

- 100 FPS / 11000z 已远超玩家需求 → 之前判 STOP（[pvz-perf-optimization](project_pvz_perf_optimization.md) POST-PHASE-3）。
- 朋友建议触发用户重启评估；用户选定预计算动画方向。
- 2026-05-23 session：读 `Reanimation/Animator.{h,cpp}` + `ReanimTypes.h` 证实风险 #1 命中。
- 用户基于"0.35-0.7ms wall 收益"决定放弃。**这次"打开 → 评估 → 放弃"的流程本身是值得的**——比写完代码再发现强很多。
- 教训：plan memory 的 ROI 估算前置假设要在"开工第一件事"用最便宜的成本验证（这次只读了 3 个文件），别在写完代码后才发现假设错位。
