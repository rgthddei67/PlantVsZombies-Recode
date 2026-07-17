---
name: project_pvz_zombie_eat_walk_state_machine
description: 僵尸「啃食→走路」状态机重构 ✅ 合 master 未 push（PlayWalkAnimation 单一权威 + OnStart/OnStopEating 对称钩子 + 非虚 ResumeWalkAfterEat 模板方法）
metadata:
  node_type: memory
  type: project
  originSessionId: 5ddfb17d-9859-4cb3-8688-2e6b27d6e5b4
---

2026-07-03 完成，7 提交合 master 未 push（0259dd0 spec / ef6f170 plan / 0701deb 基类 / 05e3189 Door / 8eb6951 Paper / c86dbe2 Polevaulter / 703a747 收口 / 0db1acb 文档补正）。起因=主人嫌 Zombie 设计臃肿：`ValidateEatingState`/`WalkTrackAfterEat`/`ResumeWalkAfterEat` 看不懂，撑杆要整段继承重写。

**根因诊断**：基类 `Zombie.cpp` 六条「回走路」路径里，`StopEat` 植物分支(682)和 `ValidateEatingState` 植物没了(742)**硬编码 `PlayTrack("anim_walk2")`** 而没走虚函数→reanim 无 anim_walk2 的僵尸(Paper/Polevaulter，`PlayTrack` 打不存在轨道是**静默失败**)被迫整段复制 `StopEat`+`ValidateEatingState`。安全性靠一条**反向隐性约定**撑着（要么有 walk2 要么整段覆写），两级继承 `FastPaperZombie` 只因「恰好没覆写」而安全。

**最终设计**（写在 `Zombie.h` 带★注释+示例）：
- `virtual void PlayWalkAnimation(float=0.0f)` = 稳态走路唯一权威（轨道+clip，确定性）。基类硬编码 anim_walk2；Paper 覆写(walk/walk_nopaper+clip)、Polevaulter 覆写(anim_walk)。**5 个走路站点全收敛**（啃完回走路/EndJump/LoadExtraData/狂暴脱困；SetupZombie 随机起步保留不动=有意初始变化）。
- `virtual void OnStartEating()`/`OnStopEating()` = 啃食视觉残留**对称钩子**，默认空。仅 DoorZombie 覆写(ShowArm true/false)。`OnStartEating` 在基类 `StartEat` 两分支 `!wasEating&&mIsEating` 触发一次。
- `void ResumeWalkAfterEat(float)` = **非虚 inline 模板方法** `{ OnStopEating(); PlayWalkAnimation(blend); }`，锁死「先收尾后走路」，子类永不覆写。
- **删 `WalkTrackAfterEat`**。三子类共卸 ~6 个整段覆写（Door 4、Paper 2+改名、Polevaulter 3）。

**成果**：门臂 3 个历史 bug（见 [project_pvz_doorzombie_arm_bugs](project_pvz_doorzombie_arm_bugs.md) c0cb799/52bd86d）从「靠人补 4 入口、终审才抓」变「结构上不可能漏」，dump 实测印证；FastPaper 两级继承「地雷」dump 验安全。

**foot-gun（实现期真机抓到，文档看不出）**：
1. **C++ 默认实参不随虚函数 override 继承**——`Polevaulter::PlayWalkAnimation(float)` 覆写没重写默认值，`EndJump` 里 `PlayWalkAnimation()` 无参调用**编译失败**。修=显式传 `PlayWalkAnimation(0.0f)`（不在 override 加默认值，那是 footgun）。
2. **AutoTest 覆盖盲区**：`smoke_charm_polevaulter` 场景没种植物→撑杆一路 anim_run 从不起跳→`EndJump`→`PlayWalkAnimation` 根本没触发。「脚本 exit=0」≠「验到目标路径」。补 `smoke_polevaulter_vault_walk`（真让撑杆翻越墙果）才验到。dump 抓 `track` 字段。
3. **`ValidateEatingState` 读档路径 AutoTest 测不到**（AutoTest 短路存档），靠编译+等价推理。
4. **blend 时间 0.2→0.3 统一（code-review 补正）**：基类 `ValidateEatingState` 本用 0.3f；被删的 Paper/Polevaulter 覆写在植物已死+啃僵尸共 4 条读档子路径原用 0.2f，收敛后统一 0.3f。纯视觉/仅读档/0.1s 淡入/肉眼不可辨，裁决保 0.3f（与其余僵尸一致，最小改动）。原 spec 只记撑杆一条=不准，已如实补。

**流程**：brainstorm→spec→plan→executing-plans 内联（选内联非 subagent 的理由=串行耦合链+重量级串行构建+机械小改+靠读真实 dump 验证）；两轮 code-review(workflow high)：Task2 后审基座无 finding；全量审出上述 blend 文档不准 1 条。新增回归脚本 `smoke_charm_fastpaper_eating`/`smoke_polevaulter_vault_walk`。dump_state 已有 `track`/`armVisible`/`doorArmVisible` 断言字段（TestDriver.cpp:387/391/393）。

**新增僵尸规则**（写进 Zombie.h）：走路不同→覆写 `PlayWalkAnimation`；啃食露部件→覆写 `OnStartEating`/`OnStopEating`；永不碰 `ResumeWalkAfterEat`/`StopEat`/`ValidateEatingState`。关联 [project_pvz_charmed_zombie_feature](project_pvz_charmed_zombie_feature.md)（d83bab0 抽 ResumeWalkAfterEat 的未竟收口）、[project_pvz_animator_playstate_save_fix](project_pvz_animator_playstate_save_fix.md)（save/load reconciliation 同族）。
