---
name: project_pvz_charmed_zombie_collision_hook
description: 以后做「魅惑僵尸啃普通僵尸」可直接用碰撞系统预留的 seeker/target 钩子，不必重写碰撞、不会引入 O(k²)
metadata:
  node_type: memory
  type: project
  originSessionId: 5d1440f7-e821-4df9-88a8-b37f2be4e916
---

**将来实现「魅惑僵尸啃普通僵尸」时，碰撞侧已备好钩子，直接用即可——不用改碰撞算法、不会退回 O(k²)。**

背景：2026-07-01 的碰撞 seeker/target 拆分([project_pvz_collision_seeker_target_fix](project_pvz_collision_seeker_target_fix.md)，已合 master)把每行动态碰撞体按 `layerMask==ZOMBIE` 分成 `mRowZombies`(被动目标)与 `mRowOthers`(seeker：子弹/割草机)，seeker 二分进排序好的僵尸数组、僵尸×僵尸整段不扫。当时**故意为魅惑留了扩展点**(spec §4)。

**接线配方(碰撞侧，`CollisionSystem.h` 一行不用改)**：
1. `ColliderComponent.h` 的 `CollisionLayer` 加一位 `CHARMED = 1 << 5`(uint16_t 还有空位)。
2. 僵尸被魅惑时把它的碰撞体 `layerMask` 改成 `CHARMED`、`collisionMask` 含 `ZOMBIE`(以及要不要含 BULLET 看下方决策)。
3. 于是它在 phase-2 分桶时**自动落进 `mRowOthers`**(因为 `layerMask!=ZOMBIE`)，`detectRow` 的 `sweepAgainstZombies` 会让它走同一条二分路径去搜 `mRowZombies`(普通僵尸)→ 命中即产 pair → 触发其"啃僵尸"回调。普通僵尸仍是纯目标。**魅惑数量少 → 廉价 seeker，不引入 O(k²)。**

**天然性质**：魅惑×魅惑不互啃(都在 `mRowOthers`，走 other×other 朴素循环，且掩码互不含 CHARMED → CanCollide false)；普通×普通照常不扫。

**注意(已被碰撞侧处理，但接魅惑回调时要记得)**：`sweepAgainstZombies` 产 pair 时**僵尸恒放 a**(`{ zb[i], s }`)，因 `HandleCollisionEnter(a,b)` 先触发 a 后触发 b，回调有先后(详见 [project_pvz_collision_seeker_target_fix](project_pvz_collision_seeker_target_fix.md) 的 foot-gun)。所以魅惑作为 seeker(`s`/b)、被啃的普通僵尸是 a：a 的回调先跑(普通僵尸 `StartEat` 对非植物 no-op)、再跑魅惑的回调。**魅惑的"啃普通僵尸"逻辑要注册在魅惑僵尸自己的碰撞体回调上**(类似植物被啃是植物侧承受)，并确认这个先后次序对新逻辑无害。

**接线时要先定的行为决策(碰撞钩子不替你决定)**：① 自家豌豆/子弹要不要打到魅惑僵尸(要则子弹 collisionMask 加 CHARMED、且魅惑得进能被 seeker 搜到的目标集)；② 魅惑僵尸会不会误啃植物(当前它 collisionMask 若保留 PLANT 就会)；③ 魅惑走到最右边界怎么处理。这些是玩法设计，非碰撞性能问题。

现状：当前 `mIsMindControlled` 只让僵尸反向右走、植物/土豆雷/大嘴花无视它(`Zombie.cpp:323`/`Board.cpp:626`/`Chomper.cpp:16`/`PotatoMine.cpp:22`)，"啃普通"尚不存在。
