# 碰撞系统 Seeker/Target 拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 `CollisionSystem` 每行 sweep 中僵尸×僵尸的 O(k²) 空转（20000 僵尸实测 9.6M 次/帧、100% 被拒），把 `Collision_Update` 从 ~5ms 降到 <0.3ms，行为零回归。

**Architecture:** 把每行的动态碰撞体拆成 `mRowZombies`(被动目标, 多) 与 `mRowOthers`(seeker: 子弹/割草机, 少)。检测时让少数 seeker（含静态植物）二分进"排好序的僵尸数组"找碰撞，**永不扫僵尸×僵尸**。产出的 pair 集合与今天逐位相同（子弹×僵尸、割草机×僵尸、僵尸×植物）→ 回调链/存档不变。

**Tech Stack:** C++17，单文件头内联实现 `PlantVsZombies/Game/CollisionSystem.h`；`std::lower_bound` 二分；现有 `ThreadPool` 按行并行不变；`-Profile` 诊断探针（已在 `1fe76cb` 落地）作前后对比仪器。

**Spec:** `docs/superpowers/specs/2026-07-01-collision-seeker-target-partition-design.md`

---

## File Structure

只改一个文件：`PlantVsZombies/Game/CollisionSystem.h`。分三个可独立编译的增量：

- **Task 1** —— 加拆分容器 + 每行最大僵尸宽，phase 2 **双写**（新旧容器同时填），detectRow 不动 → 行为不变、纯加数据。
- **Task 2** —— detectRow 换成 seeker/target 二分扫描 + 活跃行/totalDynamic 改用新容器 + 探针计数改挂新循环 → 算法切换（旧 `mRowBuckets` 仍填着当安全网，但已不被 detectRow 使用）。
- **Task 3** —— 删除死掉的 `mRowBuckets` + 把 `mNoRowDynamic` 串行段的遍历目标换成新容器 → 纯清理。

**验证门（本项目约定，替代单元测试 TDD）：** 每个 Task 必过 `clang-release` 编译零错零警告；Task 2 额外需要行为冒烟 + 主人真实 20000 场景 `-Profile` 确认 `sweepIter` 塌缩。

**编译命令（每次验证都用）：**
```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --build --preset clang-release
```
预期：`Linking CXX executable PlantsVsZombies.exe` 成功，无 warning/error。

---

## Task 1: 加拆分容器 + 双写（行为不变）

**Files:**
- Modify: `PlantVsZombies/Game/CollisionSystem.h`（成员声明区 / `Update()` 清空块 / `Update()` 阶段2 分桶）

- [ ] **Step 1: 加三个成员容器**

在 `mStaticRowBuckets` 声明之后（紧接 `std::array<std::vector<ColliderComponent*>, MAX_ROWS> mStaticRowBuckets;` 那行）插入：

```cpp
	// seeker/target 拆分：僵尸(被动目标) 与 其余动态(seeker：子弹/割草机…) 分开存，跨帧复用。
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowZombies;
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowOthers;
	std::array<float, MAX_ROWS> mRowMaxZombieW{};   // 每行最大僵尸 AABB 宽，供二分下界
```

- [ ] **Step 2: 清空块里清新容器**

把清空块中的这两行：

```cpp
		for (auto& v : mRowBuckets)       v.clear();
		for (auto& v : mStaticRowBuckets) v.clear();
```

替换为：

```cpp
		for (auto& v : mRowBuckets)       v.clear();
		for (auto& v : mStaticRowBuckets) v.clear();
		for (auto& v : mRowZombies)       v.clear();
		for (auto& v : mRowOthers)        v.clear();
		mRowMaxZombieW.fill(0.0f);
```

- [ ] **Step 3: 阶段2 双写分桶**

把阶段2里动态分支这段：

```cpp
			else {
				if (inRange) mRowBuckets[row].push_back(col);
				else         mNoRowDynamic.push_back(col);
			}
```

替换为：

```cpp
			else {
				if (inRange) {
					mRowBuckets[row].push_back(col);   // TODO(Task3): 双写过渡，Task3 删除
					if (col->layerMask == CollisionLayer::ZOMBIE) {
						mRowZombies[row].push_back(col);
						const float w = col->cachedBounds.w;
						if (w > mRowMaxZombieW[row]) mRowMaxZombieW[row] = w;
					}
					else {
						mRowOthers[row].push_back(col);
					}
				}
				else {
					mNoRowDynamic.push_back(col);
				}
			}
```

- [ ] **Step 4: 编译验证**

Run: 上文 File Structure 的编译命令
Expected: 链接成功，0 warning / 0 error。（detectRow 未改，行为与改前完全一致。）

- [ ] **Step 5: 提交**

```bash
git add PlantVsZombies/Game/CollisionSystem.h
git commit -m "refactor(collision): 加 mRowZombies/mRowOthers 拆分容器并双写（行为不变）

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: detectRow 换 seeker/target 二分扫描（核心）

**Files:**
- Modify: `PlantVsZombies/Game/CollisionSystem.h`（`Update()` 阶段3 活跃行统计 / `detectRow` lambda）

- [ ] **Step 1: 活跃行与 totalDynamic 改用新容器**

把阶段3开头这段：

```cpp
		int totalDynamic = 0;
		for (int r = 0; r < MAX_ROWS; r++) {
			if (!mRowBuckets[r].empty()) {
				mActiveRowIndices.push_back(r);
				totalDynamic += (int)mRowBuckets[r].size();
			}
		}
```

替换为：

```cpp
		int totalDynamic = 0;
		for (int r = 0; r < MAX_ROWS; r++) {
			if (!mRowZombies[r].empty() || !mRowOthers[r].empty()) {
				mActiveRowIndices.push_back(r);
				totalDynamic += (int)(mRowZombies[r].size() + mRowOthers[r].size());
			}
		}
```

- [ ] **Step 2: 重写 detectRow lambda**

把整个 `auto detectRow = [this](int row) { ... };`（从 `auto detectRow = [this](int row) {` 到对应的 `			};`，即当前包含旧 SAP 双循环 + 两段静态交叉检查 + 探针写回的整块）替换为：

```cpp
		auto detectRow = [this](int row) {
			auto& zb      = mRowZombies[row];   // 被动目标（僵尸），多
			auto& other   = mRowOthers[row];    // seeker（子弹/割草机…），少
			auto& results = mRowResults[row];
			const bool prof = g_ProfileEnabled;
			uint64_t nIter = 0, nReject = 0, nCheck = 0, nHit = 0;

			// 僵尸按 x 升序，供二分
			std::sort(zb.begin(), zb.end(),
				[](const ColliderComponent* a, const ColliderComponent* b) {
					return a->cachedBounds.x < b->cachedBounds.x;
				});
			const float maxW = mRowMaxZombieW[row];

			// 一个 seeker（other 或 静态植物）二分僵尸 x 窗口，测重叠并产 pair。
			// pair 的 (a,b) 顺序无所谓：MakePairKey 按 id 排序、HandleCollisionEnter/Exit 对 a、b 双向触发。
			auto sweepAgainstZombies = [&](ColliderComponent* s) {
				const float left  = s->cachedBounds.x;
				const float right = s->cachedBounds.x + s->cachedBounds.w;
				// 下界：第一个 x >= left - maxW 的僵尸（maxW 保证不漏"起点更早但延伸进来"的宽僵尸）
				size_t i = static_cast<size_t>(
					std::lower_bound(zb.begin(), zb.end(), left - maxW,
						[](const ColliderComponent* z, float v) { return z->cachedBounds.x < v; })
					- zb.begin());
				for (; i < zb.size() && zb[i]->cachedBounds.x <= right; ++i) {
					if (prof) ++nIter;
					if (!CanCollide(s, zb[i])) { if (prof) ++nReject; continue; }
					if (prof) ++nCheck;
					if (CheckCollision(s, zb[i])) {
						uint64_t key = MakePairKey(s->colliderID, zb[i]->colliderID);
						results.push_back({ s, zb[i], key });
						if (prof) ++nHit;
					}
				}
			};

			// (1) other × zb（子弹/割草机 命中僵尸）
			for (auto* o : other) sweepAgainstZombies(o);

			// (2) 静态目标 × zb（僵尸啃植物）：本行植物 + 无行静态，都少，同样二分
			for (auto* s : mStaticRowBuckets[row]) sweepAgainstZombies(s);
			for (auto* s : mNoRowStatic)           sweepAgainstZombies(s);

			// (2b) other × 静态目标：朴素双循环（两侧都少）。保留以对齐旧 dynamic×static 全集，
			//      免去"子弹/硬币是否撞植物"的隐含假设——CanCollide 照常过滤。
			for (auto* o : other) {
				for (auto* s : mStaticRowBuckets[row]) {
					if (prof) ++nIter;
					if (!CanCollide(o, s)) { if (prof) ++nReject; continue; }
					if (prof) ++nCheck;
					if (CheckCollision(o, s)) {
						results.push_back({ o, s, MakePairKey(o->colliderID, s->colliderID) });
						if (prof) ++nHit;
					}
				}
				for (auto* s : mNoRowStatic) {
					if (prof) ++nIter;
					if (!CanCollide(o, s)) { if (prof) ++nReject; continue; }
					if (prof) ++nCheck;
					if (CheckCollision(o, s)) {
						results.push_back({ o, s, MakePairKey(o->colliderID, s->colliderID) });
						if (prof) ++nHit;
					}
				}
			}

			// (3) other × other（|other| 极小）
			for (size_t a = 0; a < other.size(); ++a) {
				for (size_t b = a + 1; b < other.size(); ++b) {
					if (prof) ++nIter;
					if (!CanCollide(other[a], other[b])) { if (prof) ++nReject; continue; }
					if (prof) ++nCheck;
					if (CheckCollision(other[a], other[b])) {
						results.push_back({ other[a], other[b],
							MakePairKey(other[a]->colliderID, other[b]->colliderID) });
						if (prof) ++nHit;
					}
				}
			}

			// (4) zb × zb：彻底不扫 —— 干掉旧 SAP 的 9.6M 空转

			if (prof) {
				mRowIters[row]   = nIter;
				mRowRejects[row] = nReject;
				mRowChecks[row]  = nCheck;
				mRowHits[row]    = nHit;
			}
			};
```

- [ ] **Step 3: 编译验证**

Run: 上文 File Structure 的编译命令
Expected: 链接成功，0 warning / 0 error。
（注意：`mRowBuckets` 此时仍被填充 + 被 `mNoRowDynamic` 段引用，但 detectRow 不再用它，Task 3 才删。）

- [ ] **Step 4: 行为冒烟（主人可选：AutoTest 或实玩）**

产出 pair 集合与旧代码同集，理论上行为零变。冒烟确认三条主路径不回归：
- 豌豆射手打死僵尸（子弹×僵尸 → 僵尸掉血/死亡）
- 僵尸啃植物（僵尸×植物 → 植物掉血）
- 割草机碾压（割草机×僵尸 → 触发）

自动路径（若采用）：
```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
$LASTEXITCODE   # 期望 0
Pop-Location
```
查看 `build\clang-release\autotest\out\demo_peashooter\state.json` 与截图，确认僵尸受击/死亡表现正常。

- [ ] **Step 5: 性能验证（主人真实 20000 场景 `-Profile`）**

主人在真实游戏跑到 ~20000 僵尸、带 `-Profile`，对比 `FRAME PROFILE`：
- `sweepIter`：~9.6M → 几千（塌缩）
- `sweepReject`：~9.6M → ~0
- `Collision_Update`：~5ms → <0.3ms
- `total/frame` 相应下降。

**这一步是本方案成败的判据；主人确认塌缩后再进 Task 3。**

- [ ] **Step 6: 提交**

```bash
git add PlantVsZombies/Game/CollisionSystem.h
git commit -m "perf(collision): detectRow 改 seeker/target 二分扫描，跳过僵尸×僵尸空转

O(k²)→O(少数·log k)：仅让 seeker(子弹/割草机)+静态植物 二分进排序僵尸数组，
永不扫僵尸×僵尸。pair 集合与旧代码逐位相同（子弹×僵尸/割草机×僵尸/僵尸×植物）。
探针计数改挂新扫描循环。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: 删除死掉的 mRowBuckets + 适配 noRowDynamic 段

**Files:**
- Modify: `PlantVsZombies/Game/CollisionSystem.h`（成员声明 / 清空块 / 阶段2 / `mNoRowDynamic` 串行段）

- [ ] **Step 1: 删成员声明**

删除这一行：

```cpp
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowBuckets;
```

- [ ] **Step 2: 删清空块里的清 mRowBuckets**

删除这一行：

```cpp
		for (auto& v : mRowBuckets)       v.clear();
```

- [ ] **Step 3: 删阶段2 的双写行**

把 Task 1 Step 3 加的这段：

```cpp
				if (inRange) {
					mRowBuckets[row].push_back(col);   // TODO(Task3): 双写过渡，Task3 删除
					if (col->layerMask == CollisionLayer::ZOMBIE) {
```

改回为（仅删中间那行双写 + 其注释）：

```cpp
				if (inRange) {
					if (col->layerMask == CollisionLayer::ZOMBIE) {
```

- [ ] **Step 4: noRowDynamic 段改遍历新容器**

把 `mNoRowDynamic` 串行段里这段对 `mRowBuckets` 的遍历：

```cpp
				for (auto& bucket : mRowBuckets) {
					for (auto* b : bucket) {
						if (!CanCollide(a, b)) continue;
						if (CheckCollision(a, b)) {
							uint64_t key = MakePairKey(a->colliderID, b->colliderID);
							mNoRowResults.push_back({ a, b, key });
						}
					}
				}
```

替换为（对两个新容器各遍历一次，行为等价）：

```cpp
				for (auto& bucket : mRowZombies) {
					for (auto* b : bucket) {
						if (!CanCollide(a, b)) continue;
						if (CheckCollision(a, b)) {
							uint64_t key = MakePairKey(a->colliderID, b->colliderID);
							mNoRowResults.push_back({ a, b, key });
						}
					}
				}
				for (auto& bucket : mRowOthers) {
					for (auto* b : bucket) {
						if (!CanCollide(a, b)) continue;
						if (CheckCollision(a, b)) {
							uint64_t key = MakePairKey(a->colliderID, b->colliderID);
							mNoRowResults.push_back({ a, b, key });
						}
					}
				}
```

- [ ] **Step 5: 编译验证（含"无残留 mRowBuckets 引用"检查）**

Run: 上文 File Structure 的编译命令
Expected: 链接成功，0 warning / 0 error。
额外确认全文件已无 `mRowBuckets` 字样（可 grep）：期望 0 处。

- [ ] **Step 6: 行为冒烟复核 + 提交**

再快速过一遍 Task 2 Step 4 的三条主路径（或主人实玩确认），无回归后提交：

```bash
git add PlantVsZombies/Game/CollisionSystem.h
git commit -m "refactor(collision): 移除过渡用 mRowBuckets，noRowDynamic 段改遍历拆分容器

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## 验证与交接

- **自动门（执行者负责）：** 三个 Task 均 `clang-release` 0 warning / 0 error；Task 3 后全文件无 `mRowBuckets`。
- **行为门（主人或 AutoTest）：** 豌豆打死僵尸 / 僵尸啃植物掉血 / 割草机碾压，三条不回归。
- **性能门（主人 `-Profile`，20000 僵尸）：** `sweepIter` ~9.6M→几千、`Collision_Update` ~5ms→<0.3ms。
- **探针去留（后续决定）：** `1fe76cb` 的 `CountSweep` + per-row 计数是验证仪器。性能门通过后由主人定夺"删除"或"留在 `-Profile` 之后"当常驻探针（关闭时仅一个高度可预测分支，近零开销）。此决定不在本 plan 范围，作单独跟进。
- **潜伏项（不在本 plan）：** `mNoRowDynamic` 段仍是 O(noRow×n) 串行陷阱（常空），继续记录不修。
- **魅惑钩子（不在本 plan）：** 未来"魅惑啃普通"按 spec §4 加 `CHARMED` 层即可复用本结构，本 plan 一行不写。

---

## Self-Review 结论

- **Spec 覆盖：** §3.1 分桶→Task1；§3.2 (1)(2)(2b)(3)(4) 检测→Task2 Step2；§3.3 pair 对齐→Task2（pair 集合论证）；§5 不变量→各 Task 编译+行为门；§7 验证→"验证与交接"；§4 魅惑钩子/§2 非目标 mNoRowDynamic→显式留空并记录。无遗漏。
- **占位符扫描：** 无 TBD/TODO 型占位（Task1 的 `TODO(Task3)` 是"过渡标记"，Task3 Step3 显式删除，非计划占位）。
- **类型/命名一致：** `mRowZombies`/`mRowOthers`/`mRowMaxZombieW`、`sweepAgainstZombies`、`CanCollide`/`CheckCollision`/`MakePairKey`、`mRowIters/Rejects/Checks/Hits` 全程一致；`std::lower_bound` 比较器方向与 `std::sort` 升序一致。
