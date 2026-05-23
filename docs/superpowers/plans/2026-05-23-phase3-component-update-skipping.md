# Phase-3 跳过空 Component::Update 浪费虚调 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `Component` 加 `NeedsUpdate() const` 虚函数 + `GameObject` 缓存 `mUpdatableComponents` 视图；`GameObject::Update()` 不再 iterate 整个 mComponents map，只 iterate 这份很短（zombie/plant/bullet 是空）的视图。预期 11000 zombie `total/frame 10.99 → ~8.5-8.8 ms / 91 → ~110+ FPS`，削 ~2.2 ms。

**Architecture:** 信息放置正确——"我需不需要 Update"是 Component **自己的 type-level 属性**（不是 GameObject 的中央集权也不是 mEnabled 实例状态）。AddComponent / RemoveComponent / DestroyAllComponents 维护 `mUpdatableComponents` vector（raw ptr 非所有权视图，所有权仍在 mComponents 的 unique_ptr）；`mEnabled` 副道仍在 Update loop 内 check（运行时 toggle 不维护 vector，churn 不值得）。

**Tech Stack:** C++17，VS 2026，`Profiler.h` 粗粒度埋点（GATE 期复用 phase-2 留下的 `2da.components` PROFILE_SCOPE）。

**测试模型（本仓库无自动化测试框架，CLAUDE.md 禁止 assistant 构建）：** 每个代码任务的验证 = (1) 用户在 VS F7 构建；(2) **Task 4 的 5 分钟卡牌/交互目视一致性测试**（替代 phase-2 的串/并行 oracle —— 本次不是并行重构没 race 风险，但 audit 漏标 = 某组件功能悄无声息坏，目视必能抓到）；(3) Task 5 的 GATE 实测。提交均由用户驱动；plan 内"提交"步骤是用户检查点，assistant 不自动提交。

**原子性约束：** Task 2、3 各自行为逐字节一致（NeedsUpdate 加了没人调；mUpdatableComponents 维护着但 Update 不读它），是安全检查点。**Task 4 是首次行为变化点**——Update 切换为 vector iteration，audit 必须 PASS 才安全；卡牌目视测试在 Task 4 末段执行。

**前置事实（已核实，写给零上下文工程师）：**
- 源码根目录是嵌套的 `D:\PVZ\PlantsVsZombies\PlantVsZombies\PlantVsZombies\`；spec/plan 在 `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\`。
- 当前 working tree HEAD = `292f68e 阶段二并行 Update：deferred-events 完成（-3.44ms / 69.3→91 FPS）`。
- 工作目录有未提交改动：`AnimatedObject.cpp` 顶部 `#include "../Profiler.h"` + `Update()` 内 3 段 `2da.components / 2db.anim_skip+destroy / 2dc.glow` 临时 PROFILE_SCOPE（phase-3 拆段诊断遗留，本 plan Task 5 会复用 `2da` 做 GATE，Task 6 清理）+ phase-3 spec/plan 文件。
- `Component.h` 类定义 line 17；`virtual bool Update() {}` line 28；`public:` 区 line 22；`mEnabled` line 23。
- `GameObject.h` 类 line 28；`mComponents` line 40；`AddComponent<T>` template line 56-74（`mComponents[typeIndex] = std::move(component);` line 63，`InitializeComponent(raw)` line 69）；`RemoveComponent<T>` template line 88-105（`mComponents.erase(it)` line 101）；`DestroyAllComponents()` declaration line 173；类结束 `};` line 175。
- `GameObject.cpp` `DestroyAllComponents()` 实现 line 81-88（line 86 `mComponents.clear()`）；`GameObject::Update()` 实现 line 38-46。
- 4 个有非空 Update 的 Component 派生类清单（spec §5）：
  - `Game/ClickableComponent.h` (impl `ClickableComponent.cpp:131`)
  - `Game/CardComponent.h` (impl `CardComponent.cpp:85`)
  - `Game/CardDisplayComponent.h` (impl `CardDisplayComponent.cpp:37`)
  - `Game/CardSlotManager.h` (impl `CardSlotManager.cpp:38`)

**Spec：** `D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\superpowers\specs\2026-05-23-phase3-component-update-skipping-design.md`

---

### Task 1: Audit GATE — 列出所有 Component::Update override（只读代码）

**Files:** 无修改，纯 grep + 阅读。

- [x] **Step 1: grep 所有 Component 派生类的 Update 函数定义**

用 Grep 工具，pattern：`void \w+::Update\s*\(\s*\)\s*\{`，path：`PlantVsZombies/`，type `cpp`。

匹配的是函数定义（带开 `{`），不抓声明。预期看到至少这 4 条（顺序无所谓）：

```
PlantVsZombies\Game\ClickableComponent.cpp:131:void ClickableComponent::Update() {
PlantVsZombies\Game\CardComponent.cpp:85:void CardComponent::Update() {
PlantVsZombies\Game\CardDisplayComponent.cpp:37:void CardDisplayComponent::Update() {
PlantVsZombies\Game\CardSlotManager.cpp:38:void CardSlotManager::Update() {
```

可能也会匹配到 `GameObject::Update` / `AnimatedObject::Update` / `Plant::Update` / `Zombie::Update` / `Bullet::Update` / `Scene::Update` 等——这些是 **GameObject 派生类**不是 **Component 派生类**，**忽略**。识别方法：grep 对应 .h 看类继承——`X : public Component` 才是本 audit 关心的。

- [x] **Step 2: 对照 spec §5 的 4 个清单核验**

清单内：Click / Card / CardDisplay / CardSlotManager。逐条与 Step 1 grep 结果交叉对照：

- 清单内 4 个**都在** Step 1 grep 结果里 ✓
- Step 1 grep 结果里**没有第 5 个** Component 派生类 ✓

如果出现第 5 个有 Update 实现的 Component 派生类（即 `X : public Component` 且 .cpp 有 `void X::Update() {`），**FAIL** —— 更新 spec §5 清单 + 本 plan 前置事实 + Task 2/3 待加 override 清单后再继续。

- [x] **Step 3: 写下结论**

在本文件 Task 1 末尾追加：`结论：PASS`（与 spec §5 4 个清单一致）或 `结论：FAIL —— 漏标 <第 5 个 Component>，spec/plan 已更新`。

- **PASS 处理：** 进 Task 2。
- **FAIL 处理：** 更新 spec §5 + 本 plan 前置事实 + Task 2 加 override 清单。然后重新进 Task 2。

- [x] **Step 4: 用户检查点**

把结论贴给用户确认后再进 Task 2。无代码改动，无需构建/提交。

**结论：** PASS（与 spec §5 4 个清单一致）

执行细节（2026-05-23）：
- Grep `void \w+::Update\s*\(\s*\)\s*\{` 命中 15 个，其中 GameObject/Scene/Manager/Animator/Particle/Card(基类)/AnimatedObject/CursorManager 等 11 个非 Component 派生类。
- 反向 grep `:\s*public\s+Component\b` 列出全部 7 个 Component 派生类：CardComponent / CardDisplayComponent / CardSlotManager / ClickableComponent / TransformComponent / ColliderComponent / ShadowComponent。
- 交集 = 前 4 个完全匹配 spec §5；后 3 个（Transform/Shadow/Collider）的 .h 内连 `Update(` 字符串都不出现，证明既无 .cpp 实现也无 .h inline override，纯继承基类空 body。

---

### Task 2: Component.h 新增 NeedsUpdate + 4 个 .h override（纯增量，行为不变）

**Files:**
- Modify: `PlantVsZombies/Game/Component.h:22`（public 区加 1 个 virtual）
- Modify: `PlantVsZombies/Game/ClickableComponent.h`（加 1 行 override）
- Modify: `PlantVsZombies/Game/CardComponent.h`（同上）
- Modify: `PlantVsZombies/Game/CardDisplayComponent.h`（同上）
- Modify: `PlantVsZombies/Game/CardSlotManager.h`（同上）

- [x] **Step 1: Component.h 加 NeedsUpdate 虚函数**

`Component.h` line 28 当前为：

```cpp
    virtual void Update() {}                        // 每帧更新
```

在它**之后**插入：

```cpp
    /**
     * @brief 本 Component 是否需要每帧 Update。默认 false。
     *        type-level 静态属性（不依赖实例状态）；运行时 disable 走 mEnabled，与本方法正交。
     *        新增的 Component 派生类如果 override 了 Update() 做实事，必须 override 本方法返回 true。
     */
    virtual bool NeedsUpdate() const { return false; }
```

- [x] **Step 2: ClickableComponent.h 加 override**

`ClickableComponent.h:26` 当前为：

```cpp
    void Update() override;
```

在它**之后**插入：

```cpp
    bool NeedsUpdate() const override { return true; }
```

- [x] **Step 3: CardComponent.h 加 override**

`CardComponent.h:34` 当前为：

```cpp
    void Update() override;
```

在它**之后**插入：

```cpp
    bool NeedsUpdate() const override { return true; }
```

- [x] **Step 4: CardDisplayComponent.h 加 override**

`CardDisplayComponent.h:60` 当前为：

```cpp
    void Update() override;
```

在它**之后**插入：

```cpp
    bool NeedsUpdate() const override { return true; }
```

- [x] **Step 5: CardSlotManager.h 加 override**

`CardSlotManager.h:35` 当前为：

```cpp
    void Update() override;
```

在它**之后**插入：

```cpp
    bool NeedsUpdate() const override { return true; }
```

- [x] **Step 6: 用户构建验证**

用户在 VS F7 构建。预期：编译通过（注意 `Component.h` 改动会触发**大面积重编**，因为 Component.h 被多数 Component 派生类 include —— 这是已知 trade-off，时间成本而非正确性问题）。运行任一关卡：行为与改前完全一致（新 `NeedsUpdate` 还没人调，纯增量）。

- [x] **Step 7: 用户提交检查点（用户驱动）**

提交建议信息：`阶段三 step1：Component::NeedsUpdate virtual + 4 个派生类 override（API 未接通）`。assistant 不自动提交。

---

### Task 3: GameObject 新增 mUpdatableComponents 成员 + 3 处维护点（纯增量，Update 仍走老路径）

**Files:**
- Modify: `PlantVsZombies/Game/GameObject.h`（新增私有成员 + AddComponent / RemoveComponent template 内插入维护 + DestroyAllComponents declaration 不变）
- Modify: `PlantVsZombies/Game/GameObject.cpp`（DestroyAllComponents 内加 clear）

- [x] **Step 1: GameObject.h 新增 mUpdatableComponents 成员**

`GameObject.h:40` 当前为：

```cpp
    std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents; // 包含的组件
```

在它**之后**插入：

```cpp
    // 阶段三：仅缓存 NeedsUpdate()=true 的 Component 视图，避免每帧 iterate mComponents 全表
    // 非所有权 raw ptr 视图，所有权仍在 mComponents 的 unique_ptr
    std::vector<Component*> mUpdatableComponents;
```

- [x] **Step 2: AddComponent template 加维护点**

`GameObject.h:56-74` 是 `AddComponent<T>` template。当前 line 63-69 段是：

```cpp
        auto typeIndex = std::type_index(typeid(T));
        mComponents[typeIndex] = std::move(component);

        // ... 中间代码 line 64-68 略 ...
        if (mStarted) {
            InitializeComponent(raw);
            RegisterComponentIfNeeded(raw);
        }
```

读取 `GameObject.h:56-74` 看实际中间代码（line 64-68 可能包含 `mComponentsToInitialize.push_back(raw);` 等），然后在 `mComponents[typeIndex] = std::move(component);`（line 63）**之后**插入 1 行：

```cpp
        if (raw->NeedsUpdate()) mUpdatableComponents.push_back(raw);
```

**注意：** `raw->NeedsUpdate()` 在 `std::move(component)` 之后调安全 —— `raw` 是构造完成的对象指针（line 60 `T* raw = component.get();`），vptr 已就位指向 `T`（即派生类）的 vtable，所以虚调用 NeedsUpdate 直达 T 的 override。

- [x] **Step 3: RemoveComponent template 加维护点**

`GameObject.h:88-105` 是 `RemoveComponent<T>` template。当前 line 95-101 段是：

```cpp
            UnregisterComponentIfNeeded(it->second.get());
            it->second->OnDestroy();
            // 同步从 pending 列表中移除（防止删除后还有人对它做 InitializeComponent）
            mComponentsToInitialize.erase(
                std::remove(mComponentsToInitialize.begin(), mComponentsToInitialize.end(), it->second.get()),
                mComponentsToInitialize.end());
            mComponents.erase(it);
```

在 `mComponents.erase(it);`（line 101）**之前**插入 4 行：

```cpp
            // 阶段三：同步从 mUpdatableComponents 视图移除（vector 通常长度 0-1，扫描成本 O(0)~O(1)）
            mUpdatableComponents.erase(
                std::remove(mUpdatableComponents.begin(), mUpdatableComponents.end(), it->second.get()),
                mUpdatableComponents.end());
```

`<algorithm>` 已在 `GameObject.h:12` include，无需新增。

- [x] **Step 4: GameObject.cpp DestroyAllComponents 加 clear**

`GameObject.cpp:81-88` 当前为：

```cpp
void GameObject::DestroyAllComponents() {
    for (auto& [type, component] : mComponents) {
        UnregisterComponentIfNeeded(component.get());
        component->OnDestroy();
    }
    mComponents.clear();
    mComponentsToInitialize.clear();
}
```

在 `mComponentsToInitialize.clear();` 之后插入 1 行：

```cpp
    mUpdatableComponents.clear();
```

最终：

```cpp
void GameObject::DestroyAllComponents() {
    for (auto& [type, component] : mComponents) {
        UnregisterComponentIfNeeded(component.get());
        component->OnDestroy();
    }
    mComponents.clear();
    mComponentsToInitialize.clear();
    mUpdatableComponents.clear();
}
```

- [x] **Step 5: 用户构建验证**

用户 VS F7 构建。预期：编译通过。运行任一关卡：行为与改前完全一致 —— `mUpdatableComponents` 维护着但 `GameObject::Update()` 还没改、仍 iterate 老的 `mComponents`。这是 Task 4 切换前的最后一个安全检查点。

- [x] **Step 6: 用户提交检查点（用户驱动）**

提交建议信息：`阶段三 step2：GameObject mUpdatableComponents 视图 + 3 处维护点（Update 未切换）`。

---

### Task 4: GameObject::Update 切换为 vector iteration（**首次行为变化点**） + 5 分钟目视一致性测试

**Files:**
- Modify: `PlantVsZombies/Game/GameObject.cpp:38-46`（Update 函数体重写）

- [x] **Step 1: 重写 GameObject::Update**

`GameObject.cpp:38-46` 当前为：

```cpp
void GameObject::Update() {
    if (!mActive || !mStarted) return;

    for (auto& [type, component] : mComponents) {
        if (component->mEnabled) {
            component->Update();
        }
    }
}
```

完整替换为：

```cpp
void GameObject::Update() {
    if (!mActive || !mStarted) return;

    // 阶段三：仅 iterate mUpdatableComponents 视图（通常 size 0-1）
    // zombie/plant/bullet 上挂的 Transform/Collider/Shadow 都 NeedsUpdate()=false，视图为空，outer loop 直接退出
    for (Component* c : mUpdatableComponents) {
        if (c->mEnabled) c->Update();
    }
}
```

- [x] **Step 2: 用户构建验证**

用户 VS F7 构建。预期：编译通过。**这是首次行为变化点** —— 进 Step 3 之前不要做长时间游戏验证。

- [x] **Step 3: 用户 5 分钟卡牌/交互目视一致性测试（替代并发安全 oracle）**

进游戏关卡（不需要 11000 zombie，普通关卡即可，对象数 < 200 走的也是同一份 `GameObject::Update`，与对象数无关）。**严格按以下 4 步交互**，每步用 60 秒：

1. **CardComponent**：拿一张 PeaShooter 卡（左下角卡槽），看冷却 mask（半透明灰色覆盖层）从 100% 慢慢下降到 0%。再用一次让它进冷却，确认 mask 动画正常下降。
2. **CardDisplayComponent**：观察阳光数量变化时卡牌的可用/不可用状态切换（卡牌颜色变灰/恢复）。
3. **ClickableComponent**：鼠标悬停在卡牌、shovel、卡槽上，看 hover 高亮状态正常切换；点击响应正常。
4. **CardSlotManager**：选中一张卡 → 鼠标移到 grid cell 上看种植预览半透明 plant 是否跟随；切换到另一张卡确认预览图同步换；取消选择（点选中卡再点一次）预览消失。

**判定：**
- **全部 PASS** → 进 Task 5（性能 GATE 实测）
- **任一步骤功能不动**（卡牌不冷却 / 不变灰 / 不 hover / 预览不显示等） → **REVERT 本 Task** —— `git restore PlantVsZombies/Game/GameObject.cpp`，回 Task 3 末检查点。问题 = Task 1 audit 漏标了某个 Component 派生类。**重新做 Task 1 audit 把第 5 个 Component 加进 spec §5 清单后重启 Task 2-4**。

- [x] **Step 4: 用户提交检查点（用户驱动）**

提交建议信息：`阶段三 step3：GameObject::Update 切换为 mUpdatableComponents 视图（卡牌/交互目视 PASS）`。

---

### Task 5: 性能 GATE 实测（A/B 对比，用户实测决策点）

**Files:** 无代码改动。

注意：本 Task 复用 phase-2 留下、phase-3 拆段引入的 `2da.components` PROFILE_SCOPE（`AnimatedObject.cpp` 内的 3 段诊断，未提交工作目录改动）。**Task 5 期间这些 PROFILE_SCOPE 保留**；Task 6 KEEP 后整体清理。

- [x] **Step 1: A 基线测量（phase-2 HEAD 状态）**

用户操作：
1. `git stash push -m "phase-3 temp stash for A baseline" -- PlantVsZombies/Game/Component.h PlantVsZombies/Game/ClickableComponent.h PlantVsZombies/Game/CardComponent.h PlantVsZombies/Game/CardDisplayComponent.h PlantVsZombies/Game/CardSlotManager.h PlantVsZombies/Game/GameObject.h PlantVsZombies/Game/GameObject.cpp`
2. F7 构建（这是 phase-2 HEAD + AnimatedObject.cpp 仍含 2da/2db/2dc 诊断的状态）
3. 跑 11000 zombie 30 秒进入稳态
4. 记录整块 `FRAME PROFILE`（重点 `2.GOM_Update / 2d.PhaseB_serial / 2da.components / total/frame`）。预期 `2da` ≈ 5 ms（与 phase-3 拆段时数据吻合）。

- [x] **Step 2: B phase-3 测量**

用户操作：
1. `git stash pop`（恢复 phase-3 改动）
2. F7 构建
3. 跑 11000 zombie 30 秒进入稳态
4. 记录整块 `FRAME PROFILE`（同样字段）。

- [x] **Step 3: 把数据贴给 assistant 决策**

按 spec §7.2 决策规则（预期 `2da` ~5 ms → ~2.5 ms，削减 ~2.5 ms 即完美削干净实际工作）：

- **KEEP**：`2da` 削减 ≥ 1.5 ms（即 B 段 `2da` ≤ 3.5 ms）**且** Task 4 卡牌目视测试 PASS → 进 Task 6 清理。
- **ITERATE**：`2da` 削减 < 1.5 ms（远低于预期 ~2.5 ms） → 加诊断埋点核实哪部分仍重（可能：vector 不空 = audit 假阳性 NeedsUpdate=true，或 cache miss 在别处）后重测 Step 2；不进 Task 6。
- **REVERT**：卡牌目视有功能不动（Task 4 已经测过但 GATE 阶段又出现） → `git restore` 受影响 7 文件，回 phase-2 HEAD (`292f68e`)，记录结论到 [[pvz-perf-optimization]] 内存，结束本 plan。

---

### Task 6: GATE KEEP 后清理临时 PROFILE_SCOPE（条件性）

**前置：** 仅当 Task 5 决策 = KEEP 才执行。

**Files:**
- Modify: `PlantVsZombies/Game/AnimatedObject.cpp`（删 Profiler.h include + 3 段 PROFILE_SCOPE）

- [x] **Step 1: 删除 AnimatedObject.cpp 顶部 Profiler.h include**

`AnimatedObject.cpp:6` 当前为：

```cpp
#include "../Profiler.h"   // TEMP: phase-3 拆段诊断
```

整行**删除**。

- [x] **Step 2: 移除 AnimatedObject::Update 内 3 段 PROFILE_SCOPE**

`AnimatedObject.cpp` 内 `void AnimatedObject::Update()` 函数当前为（行号约 60-90，phase-3 拆段后版本）：

```cpp
void AnimatedObject::Update() {
	// TEMP: phase-3 拆段诊断，定位 2d.PhaseB_serial 内部分布；定方向后整段 PROFILE_SCOPE 全删
	{
		PROFILE_SCOPE("2da.components");
		GameObject::Update();
	}
	{
		PROFILE_SCOPE("2db.anim_skip+destroy");
		if (mAnimator) {
			if (mAdvancedInParallel) { mAdvancedInParallel = false; /* events 在 phase B drain 已处理；Animator 状态已就位 */ }
			else                     { mAnimator->Update(); }

			// 自动销毁逻辑（非循环动画且结束后自动销毁）
			if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
				if (mAutoDestroy) {
					GameObjectManager::GetInstance().DestroyGameObject(this);
				}
				else {
					mIsPlaying = false;
				}
			}
		}
	}
	{
		PROFILE_SCOPE("2dc.glow");
		UpdateGlowingEffect();
	}
}
```

完整替换为（去掉 3 个 `{ PROFILE_SCOPE(...); ... }` 包装与 TEMP 注释，恢复 phase-2 末态的 Update 实现）：

```cpp
void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		if (mAdvancedInParallel) { mAdvancedInParallel = false; /* events 在 phase B drain 已处理；Animator 状态已就位 */ }
		else                     { mAnimator->Update(); }

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
			if (mAutoDestroy) {
				GameObjectManager::GetInstance().DestroyGameObject(this);
			}
			else {
				mIsPlaying = false;
			}
		}
	}

	UpdateGlowingEffect();
}
```

- [x] **Step 3: 用户构建 + 性能复测（不含测量污染）**

用户 F7 构建并重跑 11000 zombie 30 秒稳态。

**期望：** `total/frame` 比 Task 5 Step 2 测的 B 值再快 ~2.5 ms（因为去掉了 PROFILE_SCOPE 自身 ~2.5 ms 测量开销），约 ~8.5-8.8 ms / ~115 FPS。这是 phase-3 真正的净收益数据。

**目视复核：** 卡牌/种植/僵尸/动画/`-Debug` 碰撞框无回归。

**全局 grep 自检：** 搜 `2da` / `2db` / `2dc` / `kVerifyParallelAdvance` / `AdvanceSnapshot` 全项目应为零（前两个本 Task 删除，后三个 phase-2 Task 7 已清）。搜 `TEMP: phase-3` 也应为零。

- [x] **Step 4: 用户提交检查点（用户驱动）**

提交建议信息：

```
阶段三完成：跳过空 Component::Update 浪费虚调（清理临时埋点）

- Component::NeedsUpdate() virtual + 4 个派生类 override (Click/Card/CardDisplay/CardSlotManager)
- GameObject::mUpdatableComponents 缓存视图 + AddComponent/RemoveComponent/DestroyAllComponents 3 处维护
- GameObject::Update 切换为 vector iteration，zombie/plant/bullet 直接退出
- 11000 zombie 实测 total/frame <填 Task 5 B 值>→<填 Task 6 Step 3 值> ms (-<差值> ms 净收益)
- 清理 phase-3 拆段诊断的 PROFILE_SCOPE (2da/2db/2dc) 及其 Profiler.h include
```

更新 [[pvz-perf-optimization]] 内存记录本阶段结果与下一阶段指向（Particles_Update 6.16ms 现在是 #1 大头，需独立 profile）。更新 [[pvz-parallel-update-phase2]] 内存的"下一阶段方向"段，标记 PhaseB_serial 已削。

---

## Self-Review

**Spec 覆盖核对（对照 2026-05-23-phase3-component-update-skipping-design.md）：**
- §1 架构（mUpdatableComponents 视图 + Update 重写）→ Task 3/4。✓
- §2 接口（Component::NeedsUpdate / GameObject::mUpdatableComponents）→ Task 2/3。✓
- §3 维护点（AddComponent / RemoveComponent / DestroyAllComponents）→ Task 3 Step 2/3/4。✓
- §4 Update 重写 → Task 4 Step 1。✓
- §5 4 个 override 清单 → Task 2 Step 2/3/4/5。✓
- §6 Audit GATE → Task 1。✓
- §7.1 一致性目视测试 → Task 4 Step 3（5 步交互覆盖 4 个组件）。✓
- §7.2 性能 GATE A/B → Task 5。✓
- §7.3 测量污染说明 → Task 6 Step 3（去 PROFILE_SCOPE 后净收益）。✓
- §8 永久 vs 临时 → Task 6 + 各 Task 永久部分说明。✓
- §9 风险（audit 漏标 / vptr 就位 / mEnabled 副道）→ Task 1 GATE + Task 4 Step 3 卡牌测试 + Task 3 Step 2 注释解释 vptr 就位。✓
- §10 范围外 → 全 plan 未触碰 Draw / mComponents 容器 / 并行 Update / SetEnabled / Register*Component。✓

**Placeholder 扫描：** 无 TBD/TODO/"类似 TaskN"/"加适当错误处理"；每个代码步给出完整代码。Task 6 Step 4 提交消息内的"<填 Task 5 B 值>"等是**实测数据占位**——执行 Task 6 时填入，不属于 plan 模糊。✓

**类型/签名一致性：**
- `NeedsUpdate() const` 全 Task 2 5 个文件签名一致（`virtual` 在 base / `override` 在 4 个派生）。✓
- `mUpdatableComponents` 是 `std::vector<Component*>` 全 Task 3/4 类型一致。✓
- `raw->NeedsUpdate()` 在 AddComponent 内调用（Task 3 Step 2），`raw` 在 GameObject.h:60 是 `T*`，T 继承 Component，所以 `raw->NeedsUpdate()` 走虚调表正确 dispatch。✓
- `it->second.get()` 在 RemoveComponent（Task 3 Step 3）取 unique_ptr 内 raw，与 mUpdatableComponents 内存的 raw ptr 类型一致。✓
- `<algorithm>` 在 GameObject.h:12 已 include（RemoveComponent 用的 std::remove），无需新增。✓
- AnimatedObject.cpp Task 6 Step 2 替换后内容与 phase-2 HEAD 末态字节一致（除继续保留 phase-2 引入的 `mAdvancedInParallel` 逻辑）。✓
