---
name: project-pvz-clickable-optimization
description: Clickable 稀疏自注册与显式宿主所有权契约；保留 2026-05-24 历史热点数据，并记录 2026-08-22 脱离 Component 容器后的生命周期
metadata:
  node_type: memory
  type: project
  originSessionId: 1de960cb-8dd0-42ad-a37e-19114f7a21fd
---

2026-05-24 同会话完成 ClickableComponent 稀疏注册优化；以下数字只代表当时压力基线，不得当作当前架构迁移的性能实测。

## 2026-08-22 当前契约

- `ClickableComponent` 已不再继承 `Component`，由 `GameObject` 通过 `unique_ptr` 显式独占；调用方只用 `CreateClickable()` / `GetClickable()` / `RemoveClickable()`。
- `CreateClickable()` 先确保唯一 Collider 完整就绪再构造并注册；`RemoveCollider()` 先注销 Clickable，单纯替换 Collider 则保持 Clickable 注册有效，不暴露半初始化窗口。
- 构造注册、析构注销和 `s_allClickables` 稀疏表保持不变；`ProcessMouseEvents()` 仍是 O(可点击对象)，按宿主渲染顺序降序处理并保留事件消费、光标计数以及 UI/世界坐标分流。
- 显式所有权使 Clickable 早于旧 Component `Start()` 注册，因此输入阶段必须跳过 `!owner->HasStarted()`，保持新宿主在完成 Start 前不可点击的旧时序。
- `clang-release` LTO、378 项 Win7 import audit 与五个可见点击专项通过；本阶段未做相同压力场景 A/B，不宣称新的 FPS 数字。

## 改动一句话
`ClickableComponent::ProcessMouseEvents` 从每帧扫 `GameObjectManager::GetAllGameObjects()` 全表（11000 zombies）+ per-object `GetComponent<ClickableComponent>()` 查表，改成遍历静态自注册表 `s_allClickables`（数量 ≈ 10）。顺手修了一个 dead duplicate `ContainsPoint`+push 的正确性 bug，并删掉 shared_ptr 拷贝。

## 实测
- `1b.Clickable`：1.22 ms → **0.01 ms**（-122×，超出预测的 0.05-0.15ms 下限）
- `total / frame`：9.47 ms → ~8.26 ms 估算（用户贴的是部分 profile 输出，未完整对账）
- FPS：105.6 → 预计 ~121

## Why
- 在加完 outer PROFILE_SCOPE 把"未测量盲区"从 2.40ms 拆开后，`1b.Clickable` 突然以 1.22ms / 12.9% 显形成第 4 大热点——之前完全不在任何人雷达里。
- 根因纯粹是 O(N) vs O(K)：N=11000 全场对象、K≈10 真正可点击对象，差距 1000× 量级；profiler 实测 122× 是因为每次迭代有 cache miss/branch 等固定成本。

## How to apply（关键经验）
- **`GetAllGameObjects()` 在 per-frame hot path 里是这个仓库的 foot-gun**：stress test (11000 zombies) 下任何 "for-each game object + GetComponent<T>() 筛选" pattern 都会爆。下次看到类似代码就该警觉。
- **同类潜在嫌疑**（未确认，有空可查）：任何 `for (auto& obj : GetAllGameObjects())` 调用，特别是 collision 系统、UI hover 检测、save/load 序列化、广播事件。
- **优化 pattern**：对"全场少数对象需要特殊处理"的场景，让该类型自己维护 `inline static std::vector<T*> s_all`，构造注册、析构注销（swap-with-back-and-pop O(1)）。线程安全前提：注册/注销/读取都在主线程。
- **可信度对照**：1.22ms 这个数字在没加 `1b.Clickable` outer scope 之前完全不可见——它藏在"2.40ms 未测量盲区"里。**没有 profile 就没有这次发现**，再次验证 measure-first。

## 相关
- 接下来的目标：`3.Collision_Update` 1.68ms（怀疑同样有 GetAllGameObjects 全扫 pattern，等待确认）
- 见 [project-pvz-perf-optimization](project_pvz_perf_optimization.md) / [project-pvz-gpu-instancing-reanim](project_pvz_gpu_instancing_reanim.md)
