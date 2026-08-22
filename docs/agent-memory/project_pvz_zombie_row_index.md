---
name: project-pvz-zombie-row-index
description: EntityRegistry 按行索引 ForEachZombieInRow 替代 GetAllZombieIDs 全表扫+双lock；惰性每帧重建；大蒜/子弹换行免维护
metadata:
  node_type: memory
  type: project
  originSessionId: 9e81cc04-5ff9-4abd-a71e-4383e3968e61
  updated_at: 2026-08-22
---

2026-06-06 完成（用户实跑验证功能一致后 commit/push）：给当时名为 `EntityManager`、现名为 `EntityRegistry` 的注册表加僵尸**按行空间索引**，治掉射手类索敌的热路径反模式。2026-08-22 只做类型、文件与成员名的语义重命名，下面的索引与生命周期契约没有改变。

**根因**：`Shooter::HasZombieInRow` / `PuffShroom::HasZombieInRow` / `Chomper::FindTargetZombieID` 三处同款——每个植物每 0.6s 调 `GetAllZombieIDs()`（堆分配 `vector<int>` + 遍历整张僵尸 `unordered_map` + 对每个 `weak_ptr.lock()` 判活），再逐 ID `GetZombie()`（二次哈希查找 + 二次 lock），最后用 `z->mRow == mRow` 过滤。O(射手数 × 全场僵尸) + 双重 lock + 每次分配。**这是 [project-pvz-clickable-optimization](project_pvz_clickable_optimization.md) 同族 foot-gun**：本仓库 stress-test 下"取全集再逐个过滤"必须警觉。

**方案（关键设计抉择）**：选**惰性、每帧重建**而非"增量维护钩子"。
- 存储 `std::array<std::vector<Zombie*>, kMaxRows> mZombiesByRow`，`kMaxRows=8` 与 `CollisionSystem::MAX_ROWS` 取齐；瞬态裸指针（删除在 GameObjectManager 延迟到帧末，同帧内无悬垂——和 CollisionSystem 行桶存 `ColliderComponent*` 同理）。
- `CleanupExpired()` 顶部每帧 `mRowIndexDirty = true`；首个 `ForEachZombieInRow` 查询触发 `EnsureZombieRowIndex()`（清桶保 capacity → 扫 mZombies → lock 一次 → 按当前 `mRow` 入桶）。重建只在真有查询的帧发生。
- 公开模板 `ForEachZombieInRow(row, fn)`（头文件，`Zombie` 仅前向声明即可，因模板体不解引用，解引用发生在调用方 .cpp）。

**Why 选重建不选增量**：增量 O(1) 维护但要在 add/death/换行三处接线，换行钩子漏调 → silent desync。重建把正确性的唯一真相源收敛到 `z->mRow`。**直接消解用户两个担心**：(1) 后期大蒜让僵尸换行只写 `z->mRow=新行`，下一帧自愈，零通知；(2) 子弹换行根本不进此索引，其唯一行敏感消费者=碰撞，`CollisionSystem` 本就每帧按当前行重建，免费处理。

**复杂度**：旧 = 每射手每 0.6s O(Z) 扫+分配+双lock；新 = 每帧≤1 次 O(Z) 重建（单 lock 无分配）+ 射手查询降为 O(Z/行数)。

**未动**：`GetAllZombieIDs()` 保留（`Board` / `GameInfoSaver` 存档路径仍用）。`FindGameObjectsWithTag` 全表扫实测**零调用**，未处理。

**How to apply**：本仓库再见到"`GetAll*IDs()` / `GetAllGameObjects()` 取全集 → 循环按 row/tag 过滤"立即警觉；行敏感查询优先复用 CollisionSystem 的"每帧从 mRow 重建按行桶"惯用法，别引增量钩子。

## 2026-07-27 同帧新增失效契约

火豌豆范围伤害 AutoTest 暴露了另一条边界：若本帧较早的查询已经构建过 `mZombiesByRow`，随后 `AddZombie` / `AddZombieWithID` 新增的僵尸在本帧后续查询中会缺席。两个新增入口现在都会把 `mRowIndexDirty` 置为 `true`；以后增加任何绕过这两个入口的实体恢复/生成路径，也必须同步保证行索引失效。同帧专项期望值从漏算时的 500 恢复为正确的 487。

## 2026-07-27 稀有来源查询脱钩

黄色冰道层数曾由每只僵尸逐帧调用 `ForEachZombieInRow()`，因而即使没有鎏金冰车也会触发
本帧首次 O(Z) 行桶重建。该查询现改用 `EntityRegistry` 中只登记鎏金冰车的 ID→弱引用索引，
每帧首查生成活跃来源强引用快照，目标再复核当前行、生命周期和精确覆盖。
通用行索引仍保留“每帧标脏、首查重建”以可靠承接未集中通知的换行；不要仅为这项优化把它
改成容易漏钩子的全局增量索引。以后若热查询只关心极少数特殊品种，可采用同类专用候选索引，
但移动区间、多来源叠层等最终几何结果仍须即时计算。

## 2026-08-12 每帧稀有品种查询准则

屋脊督军血条曾每个渲染帧调用通用 `GetFirstActiveZombieOfType()`：虽然没有分配 ID 数组，但仍遍历
`mZombies` 并对全体 `weak_ptr` 执行 `lock()`。20,000 僵尸、关闭血量显示的 Vulkan 压测中，该不可见
血条查询单独消耗约 1.49ms，使总帧率从历史约 130 FPS 降到约 95 FPS。无分配不等于低复杂度；
`Draw/Update` 中“全表按类型过滤”与 `GetAll*IDs()` 后过滤属于同一类压力场景 foot-gun。

修复沿用黄色冰道的专用品种索引思路，但因血条要稳定选择最小实体 ID，使用
`std::map<int, weak_ptr<RoofMarshalZombie>>`：`AddZombie` 和读档入口 `AddZombieWithID` 同步登记，
同 ID 被非督军覆盖时撤销，既有 30 帧清理周期删除过期项；查询只遍历督军候选，并即时复核预览、
活动、垂死和生命状态，返回 `shared_ptr` 钉住读取期间生命周期。正常场景只有 0～1 个候选，查询成本
不再随全场普通僵尸数增长；开发模式多实例仍保持最小 ID 语义。

以后设计逐帧查询先按集合形态选结构：极少数固定品种用专用弱索引；需要同帧批量遍历时从弱索引
构建强引用快照；Board 真正唯一拥有的实体可存 ID/弱引用并在恢复路径重建；只有集合覆盖大量普通
僵尸或成员会频繁换行时才用每帧惰性重建桶。实现与交付都要显式列出复杂度、普通生成/读档恢复/
同 ID 覆盖/过期清理这些维护入口。

本次 `clang-release` 与 Vulkan `smoke_roof_marshal_boss_bar` 已验证索引的正常生成、快照恢复和死亡
失活语义；主人提供的修复前 20,000 僵尸数据为血条查询约 1.49ms，修复后的同场景数值仍以主人下次
`-Profile` 结果为准，不把复杂度分析冒充实测 FPS。
