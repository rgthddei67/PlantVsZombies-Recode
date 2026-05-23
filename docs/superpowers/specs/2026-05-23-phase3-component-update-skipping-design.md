# Phase-3 设计：跳过空 Component::Update 的浪费虚调

**Status:** approved (2026-05-23 brainstorming) · ready for plan

**Goal:** 削减 `2d.PhaseB_serial = 2.53 ms` ≈ 全部，预期 11000 zombie `total/frame 10.99 → ~8.8 ms / 91 → ~113 FPS`。

**Driving data:** phase-3 拆段实测显示 `2da.components ≈ 2.5 ms`（剔除 PROFILE_SCOPE 测量污染后），`2db.anim_skip+destroy / 2dc.glow / 子类业务` 加起来 < 0.5 ms。`GameObject::Update()` 当前 iterate `unordered_map<type_index, unique_ptr<Component>>`，对 11000 zombie × ~3 component = ~30000+ 次虚函数派发，**其中绝大多数派发到 `Component` 基类的空默认 `Update()` body**。Zombie/Plant/Bullet 常挂的 `TransformComponent` / `ColliderComponent` / `ShadowComponent` 都不 override `Update()`——它们是被动数据 holder，调用 `Update()` 是纯浪费。

## 1. 架构总览

把"哪些 Component 真的需要 Update"信息**放到 Component 自己**（不是 GameObject 中央集权），并在 GameObject 内**预算好一份缓存视图**。`GameObject::Update()` 不再 iterate 整个组件 map，而是 iterate 这份很短（通常长度 0）的视图。

```
变更前：
  GameObject::Update
    └─ iterate mComponents (unordered_map, heap-node iteration, cache miss)
       └─ for each component:
          ├─ check mEnabled
          └─ virtual Update()  ← 多数派发到空 body

变更后：
  GameObject::Update
    └─ iterate mUpdatableComponents (vector<Component*>, contiguous, cache friendly)
       └─ for each component:
          ├─ check mEnabled
          └─ virtual Update()  ← 一定是非空（因为只有 NeedsUpdate()=true 的入列）
```

## 2. 接口变化

### 2.1 `Component.h` 公共区新增

```cpp
/**
 * @brief 本 Component 是否需要每帧 Update。默认 false。
 *        type-level 静态属性（不依赖实例状态）；运行时 disable 走 mEnabled，与本方法正交。
 *        新增的 Component 派生类如果 override 了 Update() 做实事，必须 override 此方法返回 true。
 */
virtual bool NeedsUpdate() const { return false; }
```

### 2.2 `GameObject.h` 私有区新增

```cpp
// 阶段三：仅缓存 NeedsUpdate()=true 的 Component 视图，避免每帧 iterate mComponents 全表
std::vector<Component*> mUpdatableComponents;
```

非所有权 raw pointer 视图，所有权仍在 `mComponents` 的 `unique_ptr`。GameObject 销毁时 `mComponents` 先于 `mUpdatableComponents` 析构 OK（vector dtor 不解引用 raw ptr）。

## 3. 维护点（GameObject.h template inline 代码内）

| 位置（GameObject.h） | 操作 |
|---|---|
| `AddComponent<T>` template，line ~63 (`mComponents[typeIndex] = std::move(component);`) 之后，line ~69 (`InitializeComponent(raw)`) 之前 | `if (raw->NeedsUpdate()) mUpdatableComponents.push_back(raw);` |
| `RemoveComponent` 内（line ~97-102，`mComponents.erase(it)` 之前） | `mUpdatableComponents.erase(std::remove(mUpdatableComponents.begin(), mUpdatableComponents.end(), it->second.get()), mUpdatableComponents.end());` |
| `DestroyAllComponents()` 内（线性扫 mComponents 调 OnDestroy 后） | `mUpdatableComponents.clear();` |

**故意省略**：`SetEnabled(false)` 不从 vector 移除，运行时 toggle 频次低，vector add/remove 的 churn 不值得。`Update()` 内 `if (c->mEnabled)` 守卫覆盖此情况。

## 4. `GameObject::Update()` 重写

`GameObject.cpp:38-46` 替换为：

```cpp
void GameObject::Update() {
    if (!mActive || !mStarted) return;
    for (Component* c : mUpdatableComponents) {
        if (c->mEnabled) c->Update();
    }
}
```

**关键性能特征：** 11000 zombie 每个对象的 mUpdatableComponents 是**空 vector**（因为 Zombie 只挂 Transform/Collider/Shadow 三个 NeedsUpdate=false 的组件）→ outer loop 几乎 zero work。

## 5. Component 派生类 override 清单（4 个 .h 各加一行）

| 文件 | 当前 Update 实现位置 | 加这一行 |
|---|---|---|
| `Game/ClickableComponent.h` | `ClickableComponent.cpp:131` | `bool NeedsUpdate() const override { return true; }` |
| `Game/CardComponent.h` | `CardComponent.cpp:85` | 同上 |
| `Game/CardDisplayComponent.h` | `CardDisplayComponent.cpp:37` | 同上 |
| `Game/CardSlotManager.h` | `CardSlotManager.cpp:38` | 同上 |

剩余 3 个派生类（`TransformComponent` / `ColliderComponent` / `ShadowComponent`）**不动**，继承 default `false`。

## 6. Audit GATE（plan 第 1 步 read-only）

类比 phase-2 Task 1 的 child-Animator-跨父审计：本次 audit 在 plan 启动时强制执行，**FAIL 即停止**。

**Audit 步骤：**

1. Grep 项目所有 `void \w+::Update\s*\(\s*\)\s*\{` 匹配——这会捕获**全部** Component 派生类的 Update 函数定义（包括 .h 内 inline 与 .cpp 内 out-of-line）。
2. 对每个匹配确认：
   - 是 Component 派生类的 Update override（不是 GameObject 或 Scene 的 Update）
   - 是否在第 5 节的 4 个 .h 清单内
3. 若发现遗漏 → **立即停止 plan 执行**，加入清单，更新 spec。

**预期 PASS 形态：** 4 个 match 完全对应清单。
**预期 FAIL 形态：** 出现第 5 个 Component 派生类有非空 Update 实现。

## 7. 验证策略

### 7.1 一致性验证（功能正确性）

无自动化测试。用户在 VS F7 构建后做 5 分钟目视交互：
- 选关卡 → 卡牌冷却 mask 正常下降（CardDisplay）
- 鼠标悬停卡牌 / shovel 高亮变化（Clickable）
- 选中卡牌后 hover cell 显示种植预览（CardSlotManager）
- 种植 / shovel 铲除 / 拿阳光 / 切换卡 / 进/出 chooseCard UI 都正常（CardComponent）

**若任一卡牌相关功能"动不了"** → 立即停止，audit 漏了某个组件。

### 7.2 性能 GATE（A/B 对比）

复用 phase-2 已留的 `2da.components` PROFILE_SCOPE（在 AnimatedObject.cpp 未提交工作目录内）。流程：

- **A 基线**：`git stash` 工作目录 phase-3 改动，保留 `2da` PROFILE_SCOPE，跑 11000 zombie 30s 稳态 → 记 `2da.components`（应 ≈ 5 ms 即 phase-3 拆段时数据，确认 baseline 重现）
- **B phase-3**：恢复改动 + F7，跑 11000 zombie 30s 稳态 → 记 `2da.components`

**决策规则**（预期 `2da` ~5 ms → ~2.5 ms = 削减 ~2.5 ms，即完美削干净实际组件工作，剩下的 2.5 ms 是 PROFILE_SCOPE 测量开销本身）：

| 决策 | 条件 |
|---|---|
| KEEP | `2da` 削减 ≥ 1.5 ms（即 ≤ 3.5 ms）**且** 卡牌目视测试 PASS |
| ITERATE | `2da` 削减 < 1.5 ms（远低于预期 ~2.5 ms） → 加诊断埋点核实哪部分仍重 |
| REVERT | 卡牌目视有功能不动 → audit 漏标，回 phase-2 HEAD (`292f68e`) + 重新 audit |

### 7.3 测量污染说明

`2da.components` PROFILE_SCOPE 本身有 ~250 ns × 10000 对象 = ~2.5 ms 测量开销（phase-3 拆段实测验证）。这 2.5 ms 是**测量自身**，不是真实工作。因此：

- phase-3 改动前：`2da` 实测 ~5 ms = 2.5 ms 真实组件遍历 + 2.5 ms 测量开销
- phase-3 改动后：`2da` 实测 ~2.5 ms = ~0 ms 真实工作 + 2.5 ms 测量开销

**意味着 2da 实测从 ~5 ms → ~2.5 ms 就是已经完美削干净了。** 若想看真实净收益，需在 KEEP 后**移除 2da PROFILE_SCOPE 再测一次** —— 那时整 `total/frame` 应从 10.99 → ~8.5-8.8 ms（净削 ~2.2 ms）。

## 8. 永久 vs 临时

**永久保留（phase-3 commit 内）：**
- `Component::NeedsUpdate()` virtual
- `GameObject::mUpdatableComponents` 私有成员 + 3 处维护点
- `GameObject::Update()` 重写为 vector iteration
- 4 个派生类的 `NeedsUpdate() override { return true; }`

**临时（plan 内删除）：**
- AnimatedObject.cpp 内的 `#include "../Profiler.h"` + 三段 `2da/2db/2dc` PROFILE_SCOPE — phase-3 KEEP 后整体清除（与 phase-2 临时 oracle 处理同款节奏：用一次 KEEP 后即删）

## 9. 风险评估

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| Audit 漏掉某个 Component 派生类 → 该组件每帧不再 Update | 中（项目里可能有动态加的 Component 没遵循命名约定） | 该组件功能悄无声息坏 | §6 audit GATE + §7.1 5 分钟目视交互 |
| AddComponent 时 NeedsUpdate() 调用——构造完成后 vptr 已就位 OK | 低 | N/A | C++ 保证（构造函数返回后对象 vptr 是 T 的）|
| 未来新增 Component 派生类忘标 NeedsUpdate=true | 中 | 该组件每帧不再 Update | Component.h 文档说明；目视测试时也能捕获 |
| `mEnabled` 运行时 toggle 不维护 vector → "我 disable 了为什么还在调用" 直觉冲击 | 低 | 无功能影响（Update 内 if 守卫） | 注释说明设计意图 |
| 改 Component.h 触发大面积重编 | 高（Component.h 被多数 Component 派生类 include） | 仅时间成本 | N/A，用户自行 build |

## 10. 范围外（明确不做）

- **不**改 `GameObject::Draw()` 内组件遍历（mComponents iteration）—— Draw 路径不在 phase-3 范围
- **不**改 `mComponents` 容器类型（unordered_map 保留作为权威存储）
- **不**做组件并行 Update（phase-1 教训：phase B 主要工作必须串行）
- **不**改 `SetEnabled` 路径维护 vector
- **不**触碰 `RegisterComponentIfNeeded` / `RegisterAllColliders` 等独立维护通路
