# 碰撞系统 Seeker/Target 拆分 —— 设计

**Date:** 2026-07-01
**Owner:** rgthddei67
**Status:** Design approved (brainstorming 完毕)，待落实施 plan
**范围文件:** `PlantVsZombies/Game/CollisionSystem.h`（唯一逻辑改动点）；`Profiler.h` + `CollisionSystem.h` 的诊断探针已在前置会话加入（见 §7）

---

## 1. Context

### 1.1 触发与瓶颈现状

主人把每波刷怪上限提到较高后，真实游戏可达 ~20000 僵尸。`-Profile` 实测（20000 僵尸）：

```
total / frame        :   13.55 ms  (73.8 FPS)
3.Collision_Update   :    5.07 ms      ← 单块最大，占 ~37%
2.GOM_Update(serial) :    3.45 ms
6.Draw_submit(par)   :    4.37 ms
C3.EndFrame_Present   :    0.16 ms      ← GPU 空闲，仍完全 CPU-bound
```

对照历史 11000 僵尸基线（2026-06-24）`Collision_Update = 1.33ms`：11000→20000（1.82×）时碰撞 1.33→5.07（**3.76×**），**超线性**，是唯一如此缩放的块（par-record 亚线性、GOM 线性）。

### 1.2 根因（已实测坐实，非推断）

`CollisionSystem::Update` 的每行检测用 **sweep-and-prune (SAP)**：按 `cachedBounds.x` 排序后，内层循环遍历所有 x 区间重叠的碰撞体对。SAP 复杂度取决于"x 轴上有多少区间互相重叠"，而僵尸在固定走廊里高度堆叠 → 剪枝 `bucket[j].x > rightEdge` 几乎不触发 → 单行逼近 **O(k²)**。

放大器：动态行桶里 ~99% 是僵尸，而僵尸的碰撞掩码 **不含 ZOMBIE**（`Zombie.cpp:45-46`：`layerMask=ZOMBIE`、`collisionMask=PLANT|BULLET|MOWER`）。故 `CanCollide(zombieA, zombieB)` 恒为 false —— 内层循环把僵尸×僵尸对**全部迭代一遍只为确认它们不该碰**。

前置会话加诊断探针后实测（20000 僵尸，单帧均值）：

```
sweepIter    :  9,605,974 /frame   ← 内层扫描总迭代
sweepReject  :  9,605,974 /frame   ← 被 CanCollide 拒绝（精确 100.000%）
sweepCheck   :          0 /frame   ← 真正做过 AABB 检测
sweepHit     :          0 /frame   ← 检出的碰撞对
```

结论 **CONFIRMED**：整块 5.07ms 是纯空转僵尸×僵尸迭代，零有用工作。9.6M ÷ 20000 ≈ 每僵尸每帧被无谓比较 ~480 次（=本行 x 轴重叠邻居密度）。

### 1.3 相关碰撞体的层设置（决定拆分判据）

| 碰撞体 | isStatic | layerMask | collisionMask | 角色 |
|---|---|---|---|---|
| 僵尸 (`Zombie.cpp:45`) | false | ZOMBIE | PLANT\|BULLET\|MOWER | 被动目标（多） |
| 子弹 (`Bullet.cpp:21`) | false | BULLET | ZOMBIE | seeker（少） |
| 割草机 | false | MOWER | ZOMBIE | seeker（少） |
| 植物 | true | PLANT | … | 静态目标（少） |

魅惑僵尸 `mIsMindControlled`（`Zombie.h:49`）当前**只反向右走 + 被植物无视**（`Zombie.cpp:323`/`Board.cpp:626`/`Chomper.cpp:16`/`PotatoMine.cpp:22`），其碰撞掩码从不改写 → 今天"魅惑啃普通"**并不存在**。故本次修复不破坏任何现有行为。

---

## 2. 目标 / 非目标

**目标**
- 消除动态-动态 sweep 里的僵尸×僵尸空转，把 `Collision_Update` 从 ~5ms 降到 <0.3ms（`sweepIter` 从 ~9.6M 塌到几千）。
- 保持产出的碰撞对集合与今天**逐位相同**（行为零回归）。
- 为未来"魅惑僵尸啃普通僵尸"留一个不引入 O(k²) 的廉价钩子（**只设计，不实现**）。
- 运行速度为最高优先（主人明示：实现成本不重要，运行时最快）。

**非目标**
- 不实现魅惑啃普通功能本身。
- 不**优化** `mNoRowDynamic` 那段串行逻辑（既有潜伏 O(n²) 陷阱，罕用/常空）；仅随容器拆分**机械跟随**（其遍历目标 `mRowBuckets` → `mRowZombies ∪ mRowOthers`），行为等价。
- 不改存档、不改回调链、不改 Debug 命中框。
- 不换用空间哈希网格（Approach B）或泛层分桶（Approach C）—— 见 §6 取舍。

---

## 3. 方案（Approach A：Seeker/Target 拆分）

### 3.1 分桶阶段（`Update` phase 2）

把"每行一个动态桶 `mRowBuckets[row]`"替换为两个跨帧复用容器（沿用现有 `clear()` 保 capacity 的惯例）：

- `mRowZombies[row]` —— `layerMask == CollisionLayer::ZOMBIE` 的被动目标。
- `mRowOthers[row]` —— 其余动态碰撞体（子弹/割草机/硬币…）。

判据仅一句：`col->layerMask == CollisionLayer::ZOMBIE` → `zb`，否则 → `other`。
build 时顺手求每行最大僵尸宽 `mRowMaxZombieW[row] = max(cachedBounds.w)`（供 §3.2 二分下界）。

**核心不变量:** 凡进 `mRowZombies` 的成员，都不需要与另一个 `mRowZombies` 成员碰撞。（这正是"僵尸专用层"要留给未来魅惑的原因——魅惑必须换独立层，见 §4。）

静态桶 `mStaticRowBuckets` / `mNoRowStatic` / `mNoRowDynamic` 不变。

### 3.2 检测阶段（`detectRow`）—— 一切扫进"排好序的僵尸数组"

```
zb    = mRowZombies[row]      // 已在本行
other = mRowOthers[row]
sort zb by cachedBounds.x     // 唯一一次排序，供二分

// (1) other × zb：每个 seeker 二分僵尸 x 窗口
for o in other:
    lo = lower_bound(zb, o.x - mRowMaxZombieW[row])      // 下界，保证不漏宽僵尸
    for i = lo; i < zb.size() && zb[i].x < o.x + o.w; ++i:
        if CanCollide(o, zb[i]) && CheckCollision(o, zb[i]): pair(o, zb[i])

// (2) 静态目标 × zb（僵尸啃植物）：植物少，同样二分
for p in mStaticRowBuckets[row] + mNoRowStatic:
    同 (1) 的二分窗口 → pair(zb[i], p)

// (2b) other × 静态目标：朴素双循环（两侧都少）。保留以对齐今天 dynamic×static 的全集，
//      免去"子弹/硬币是否会撞植物"的隐含假设——CanCollide 照常过滤，成本可忽略。
for o in other, for s in (mStaticRowBuckets[row] + mNoRowStatic):
    if CanCollide && CheckCollision: pair(o, s)

// (3) other × other：朴素双循环（|other| 极小，可忽略）
for i in other, for j>i: if CanCollide && CheckCollision: pair

// (4) zb × zb：彻底不扫   ← 干掉 9.6M 空转
```

**二分窗口正确性:** `zb` 按 x 升序；僵尸 z 与 seeker o 在 x 上重叠 ⇔ `z.x < o.x+o.w && z.x+z.w > o.x`。因 `z.w ≤ mRowMaxZombieW`，下界取 `o.x - mRowMaxZombieW` 二分即不会跳过任何"起点更早但延伸到 o.x 内"的宽僵尸；上界线性扫到 `z.x ≥ o.x+o.w` 为止；精确重叠仍由 `CheckCollision` 判（含 BOX/CIRCLE）。

**复杂度:** O(k²) → O((|other|+|plants|)·(log k + 窗口))。seeker 数是几十量级，窗口很窄。

### 3.3 pair 产出对齐

(1)(2)(2b)(3) 产出的 pair 与今天完全同集：子弹×僵尸、割草机×僵尸、僵尸×植物。`MakePairKey`、`mRowResults[row].push_back`、phase 4 的 `HandleNewCollision` / `DetectEndedCollisions` 全部不改。方向对称性：detection 与回调对 a/b 双向触发（`HandleCollisionEnter/Exit` 已对 a、b 都调），故每对只需发现一次。

---

## 4. 魅惑钩子（只设计，不实现）

未来要"魅惑啃普通"时的最小改法（本次一行都不写）：

1. 新增碰撞层 `CollisionLayer::CHARMED`（`ColliderComponent.h`，uint16_t 尚有空位）。
2. 僵尸被魅惑时把 `layerMask` 改为 `CHARMED`、`collisionMask` 含 `ZOMBIE`。
3. 于是魅惑僵尸 build 时自然落进 `mRowOthers`，走 §3.2 (1) 的二分路径去搜 `mRowZombies`（普通僵尸）→ 魅惑啃普通。

天然性质：普通僵尸仍是纯目标（`zb`）；魅惑数量少 → 廉价 seeker，不引入 O(k²)；魅惑×魅惑走 (3) 且掩码互不含 CHARMED → 不互啃。子弹是否打魅惑、魅惑是否误啃植物等行为细节留到那时再定。

---

## 5. 不变量与正确性

- **碰撞对集合**与今天逐位相同 → enter/stay/exit 回调、`StartEat`/`StopEat`、子弹伤害、割草机触发 全部行为不变。
- `currentCollisions` / `MakePairKey` / `DetectEndedCollisions` 不变。
- **存档不变**（碰撞是每帧瞬态，不入档）。
- **线程安全不变**：每行并行派发照旧，各行只写自己的 `mRowResults[row]` 与诊断计数槽位。
- **Debug 命中框**：碰撞体注册/绘制不变。
- `mNoRowDynamic` 串行段保留原样（其内部对"所有行桶"的遍历需改为遍历 `mRowZombies ∪ mRowOthers`，但该表常空 → 保持朴素，不优化）。

---

## 6. 备选方案取舍

- **B 空间哈希网格**：更通用，但密集行仍在单元格内 zombie×zombie 迭代（只被单元格容量封顶），代码多、风险大，且不利用"少 seeker"结构。否决。
- **C 泛层分桶** `(row, layer)`：是 A 的多层泛化；当前只有 ZOMBIE(多) vs BULLET/MOWER(少) 两类相关层，属 YAGNI。否决。
- **A** 是针对实测数据的最省运行时、最小爆炸半径（单文件）、且免费带出魅惑钩子的方案。**已选。**

---

## 7. 验证

- **性能（主人在真实 20000 场景 `-Profile` 对比）：** `sweepIter` ~9.6M → 几千；`sweepReject` → ~0；`Collision_Update` ~5ms → <0.3ms；`total/frame` 相应下降。
- **行为冒烟（AutoTest 或实玩）：** 豌豆射手打死僵尸 / 僵尸啃植物掉血 / 割草机碾压触发 各验一遍不回归。
- **诊断探针去留：** `Profiler.h` 的 `CountSweep` + `CollisionSystem.h` 的 per-row 计数是本次验证仪器；验证通过后由主人定夺"删除"还是"留在 `-Profile` 之后"当常驻探针（关闭时仅一个高度可预测分支，近零开销）。

---

## 8. 影响文件

- `PlantVsZombies/Game/CollisionSystem.h` —— 分桶容器拆分（`mRowBuckets` → `mRowZombies`/`mRowOthers` + `mRowMaxZombieW`）、`detectRow` 检测算法重写、phase 2/3/4 相应接线；诊断计数改挂到新容器。
- （已完成，前置会话）`Profiler.h` + `CollisionSystem.h` 诊断探针 —— 保留，作验证仪器。
