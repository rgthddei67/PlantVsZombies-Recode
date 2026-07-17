---
name: project-pvz-zombie-row-index
description: EntityManager 按行索引 ForEachZombieInRow 替代 GetAllZombieIDs 全表扫+双lock；惰性每帧重建；大蒜/子弹换行免维护
metadata:
  node_type: memory
  type: project
  originSessionId: 9e81cc04-5ff9-4abd-a71e-4383e3968e61
---

2026-06-06 完成（用户实跑验证功能一致后 commit/push）：给 `EntityManager` 加僵尸**按行空间索引**，治掉射手类索敌的热路径反模式。

**根因**：`Shooter::HasZombieInRow` / `PuffShroom::HasZombieInRow` / `Chomper::FindTargetZombieID` 三处同款——每个植物每 0.6s 调 `GetAllZombieIDs()`（堆分配 `vector<int>` + 遍历整张僵尸 `unordered_map` + 对每个 `weak_ptr.lock()` 判活），再逐 ID `GetZombie()`（二次哈希查找 + 二次 lock），最后用 `z->mRow == mRow` 过滤。O(射手数 × 全场僵尸) + 双重 lock + 每次分配。**这是 [project-pvz-clickable-optimization](project_pvz_clickable_optimization.md) 同族 foot-gun**：本仓库 stress-test 下"取全集再逐个过滤"必须警觉。

**方案（关键设计抉择）**：选**惰性、每帧重建**而非"增量维护钩子"。
- 存储 `std::array<std::vector<Zombie*>, kMaxRows> mZombiesByRow`，`kMaxRows=8` 与 `CollisionSystem::MAX_ROWS` 取齐；瞬态裸指针（删除在 GameObjectManager 延迟到帧末，同帧内无悬垂——和 CollisionSystem 行桶存 `ColliderComponent*` 同理）。
- `CleanupExpired()` 顶部每帧 `mRowIndexDirty = true`；首个 `ForEachZombieInRow` 查询触发 `EnsureZombieRowIndex()`（清桶保 capacity → 扫 mZombies → lock 一次 → 按当前 `mRow` 入桶）。重建只在真有查询的帧发生。
- 公开模板 `ForEachZombieInRow(row, fn)`（头文件，`Zombie` 仅前向声明即可，因模板体不解引用，解引用发生在调用方 .cpp）。

**Why 选重建不选增量**：增量 O(1) 维护但要在 add/death/换行三处接线，换行钩子漏调 → silent desync。重建把正确性的唯一真相源收敛到 `z->mRow`。**直接消解用户两个担心**：(1) 后期大蒜让僵尸换行只写 `z->mRow=新行`，下一帧自愈，零通知；(2) 子弹换行根本不进此索引，其唯一行敏感消费者=碰撞，`CollisionSystem` 本就每帧按当前行重建，免费处理。

**复杂度**：旧 = 每射手每 0.6s O(Z) 扫+分配+双lock；新 = 每帧≤1 次 O(Z) 重建（单 lock 无分配）+ 射手查询降为 O(Z/行数)。

**未动**：`GetAllZombieIDs()` 保留（`Board` / `GameInfoSaver` 存档路径仍用）。`FindGameObjectsWithTag` 全表扫实测**零调用**，未处理。

**How to apply**：本仓库再见到"`GetAll*IDs()` / `GetAllGameObjects()` 取全集 → 循环按 row/tag 过滤"立即警觉；行敏感查询优先复用 CollisionSystem 的"每帧从 mRow 重建按行桶"惯用法，别引增量钩子。
