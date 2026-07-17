---
name: project-pvz-ghost-zombie-shooter-fix
description: "修\"僵尸数0但射手持续开火\"幽灵僵尸 — 行索引过滤IsActive/IsDying + Die()防重入 + DestroyGameObject(raw)静默失败留痕"
metadata:
  node_type: memory
  type: project
  originSessionId: 701254d0-91d0-4068-abea-64c8cee6d691
---

2026-07-06(b1cec54, master未push) 修主人报告的"僵尸数量=0、本行无僵尸、Shooter却一直发射，重进关卡就好"。

**根因分析**：`Shooter::HasZombieInRow` 每0.6s新鲜扫描不缓存，持续开火⇒`EntityManager`行桶里真有活对象。僵尸存亡有4份真相（mZombieNumber计数/mActive渲染/EntityManager索敌/GOM生命周期），`Die()`失败或重入即产生"计数已扣+隐身+对象仍在"的幽灵。两条具体路径：
1. `GameObjectManager::DestroyGameObject(GameObject* raw)` 在 mGameObjects/mObjectsToAdd 都找不到时**静默no-op**→shared_ptr泄漏；
2. 同帧双Die（大嘴花OnBiteKillFrame按ID直接z->Die() + 僵尸自身anim_death第216帧事件，此刻weak_ptr未过期）→mZombieNumber双扣提前归零。

**修法（三处守卫，1+3已上；探针方案2未做）**：
- `EnsureZombieRowIndex` 入桶排除 `!IsActive() || IsDying()`（全部索敌方受益：Shooter/PuffShroom/FumeShroom/Chomper；原版也不索敌垂死僵尸）
- `Zombie::Die()` 顶部 `if(mIsDead)return; mIsDead=true;`（Die无子类覆写，守卫放基类即全覆盖）
- `DestroyGameObject(raw)` 找不到时 LOG_WARN 留痕（Release保留WARN）——若主人日后再见此症状，先查run.log/stdout有无该WARN即可定位泄漏现场

**foot-gun**：mIsDying 是 protected 无访问器，本次新加 `IsDying()`（照HasHead()风格）；行桶过滤后勿再有人假设桶里含垂死僵尸。关联 [project_pvz_zombie_row_index](project_pvz_zombie_row_index.md)、[project_pvz_zombie_eat_walk_state_machine](project_pvz_zombie_eat_walk_state_machine.md)（Zombie.cpp:238 mDbgAnomalyLogged+10s看门狗是先前"垂死卡住"的插桩，泄漏对象不再Update所以看门狗救不了它）。
