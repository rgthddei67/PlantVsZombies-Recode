---
name: project_pvz_doorzombie_arm_bugs
description: "铁门僵尸(DoorZombie)手臂显隐状态机——两套手臂/所有入口/已修4个bug(c0cb799,52bd86d,8f9ada6,+断手再吃复活臂)/dump抓手armVisible+doorArmVisible；主人说\"铁门bug贼多\"故建此专档"
metadata:
  node_type: memory
  type: project
  originSessionId: 6bfd61e6-5020-4394-b221-cf3b0dca5360
---

**铁门僵尸手臂 bug 反复出现（主人 2026-07-02 连报 3 个，均已修，master 未 push）。** 根因都是"手臂显隐分散在很多入口、某条路径漏同步"。此档汇总状态机与抓手，便于将来审计/排查。

## 两套手臂（互斥，同一条胳膊的两种表现）
- **常规臂**：`Zombie_outerarm_hand`/`_lower`/`_upper` + `anim_innerarm1/2/3`，由 `DoorZombie::ShowArm(bool)` 整体控制。平时藏门后（SetupZombie `ShowArm(false)`），**啃食时露出来啃**。
- **持门臂**：`Zombie_innerarm_screendoor`/`Zombie_outerarm_screendoor`/`Zombie_innerarm_screendoor_hand`，抱着门。门在时显示；只在 `ShieldDrop`（掉门）里被隐藏。
- **门本体**：`anim_screendoor`（独立于持门臂）。
- 不变式：门在→持门臂显示、常规臂藏（啃食例外：常规臂也露，两臂并存=正确）；门掉→持门臂隐藏、常规臂常显；**断臂(ShowBrokenArm)→常规臂变残端 且 持门臂必须隐藏**（断臂抓不住门）。

## 碰手臂可见性的所有入口（排查必须逐个看）
`SetupZombie`(ShowArm false) / `StartEat` 植物分支(ShowArm true) / `StartEat` 僵尸分支(魅惑后啃敌方僵尸,52bd86d 补 ShowArm true) / `StopEat` 植物分支(ShowArm false) / `ValidateEatingState`(ShowArm false) / `ResumeWalkAfterEat`(c0cb799 覆写,ShowArm false,接魅惑/啃僵尸收尾) / `ShieldDrop`(隐藏持门臂三轨+ShowArm true) / `TakeBodyDamage`(门掉后 body≤2/3 ArmDrop、≤1/3 HeadDrop) / `HeadDrop`(mHasArm 时 ShowBrokenArm) / `ShowBrokenArm`(断臂残端,8f9ada6 补隐藏持门臂三轨) / `ZombieItemUpdate`(读档一次定状态：!mHasArm||mIsDying→ShowBrokenArm；shieldGone||mIsEating→ShowArm true)。

## 已修的 3 个 bug（都是某入口漏同步）
1. **c0cb799 魅惑后多一条手臂**：啃植物时 ShowArm(true)，被魅惑走基类 `StartMindControlled→ResumeWalkAfterEat` 绕过 DoorZombie::StopEat 没藏臂→常规臂+持门臂并存。修=DoorZombie 覆写 `ResumeWalkAfterEat`：Zombie::ResumeWalkAfterEat + `if(mShieldType!=NONE) ShowArm(false)`。详见 [project_pvz_charmed_zombie_feature](project_pvz_charmed_zombie_feature.md) foot-gun#5。
2. **52bd86d 魅惑后啃敌方僵尸无手臂**：`StartEat` 僵尸分支(仅魅惑啃僵尸走)只委托 Zombie::StartEat、从不 ShowArm(true)；被 #1 未修前的多手臂 bug 掩盖，#1 修好才现形("现在手臂没了")。修=僵尸分支补 `if(!wasEating && mIsEating && mShieldType!=NONE) ShowArm(true)`。详见 foot-gun#6。
3.5. **5c38bb0 断手后再啃食手臂复活（主人手测发现并验证，autotest未跑）**：穿透在门未掉时掉头(HeadDrop→ShowBrokenArm，注意 mHasArm 仍 true 只有 mHasHead=false)，无头流血期间 `StartEat` 无 mHasHead 守卫（守卫只在 EatTarget）照样开吃→`OnStartEating` 只查门在→`ShowArm(true)` 把 hand/lower 点亮=断臂复活；对称地 OnStopEating 会把残端全藏成无臂。修=OnStart/OnStopEating 头部加 `if(!mHasArm||!mHasHead) return;`（残端优先）；同 commit 把 `ZombieItemUpdate` 读档判据补上 `!mHasHead`（原来只判 !mHasArm，抓不到"有臂无头"穿透态，读档后残端会藏回门后），三入口优先级对齐。正是本档"尚未审计"预言的断手×穿透格。
4. **8f9ada6 大喷菇穿透打死残留持门臂**：`penetrateShield=true` 在门未掉时把本体打到 ≤1/3→`HeadDrop→ShowBrokenArm`(常规臂变残端)，但 ShowBrokenArm 从不隐藏持门臂→断臂残端+完好持门臂并存。正常死亡门先掉(ShieldDrop 已隐藏持门臂)故只有穿透暴露。修=`ShowBrokenArm` 补隐藏持门臂三轨（门已掉时无害 no-op）。

## 死亡模型（排查死亡相关手臂 bug 必知）
只有**掉头后**(`!mHasHead`)本体才流血(10%/s，`Zombie.cpp:268-306`)→≤35 触发 `anim_death`+mIsDying→帧216 Die()销毁。`FumeAttack` 跳过无头僵尸(HasHead 守卫)，故穿透把本体打到 1/3→掉头→之后靠无头流血推进死亡。buggy 手臂状态从掉头就开始、持续到死亡。

## 验证抓手（AutoTest dump_state，TestDriver）
- `armVisible` = GetTrackVisible("Zombie_outerarm_hand")（常规臂）
- `doorArmVisible` = OR("Zombie_outerarm_screendoor","Zombie_innerarm_screendoor","Zombie_innerarm_screendoor_hand")（持门臂）
- 无此轨道的僵尸安全返回 false。脚本：smoke_charm_door_eating（魅惑啃植物）、smoke_charm_door_eat_zombie（魅惑啃僵尸）、smoke_door_fume_death（夜10关大喷菇喷死啃坚果的铁门,d04/d05 抓掉头+死亡）。
- 复现大喷菇穿透死亡的关键：夜间关(10)大喷菇醒；kFumeReach=400(col0 覆盖 x∈[242,642])；坚果放射程内(col3)让门僵尸啃着被喷；门僵尸本体≈270，掉头 body≤90 后 FumeAttack 停(无头)、改无头流血(实时~2-3s)到死。

## 尚未审计（主人已同意方向，未展开）
建议出"入口×状态(门在/掉·有头/断头·魅惑/非魅惑·啃植物/啃僵尸/走路/死亡)"矩阵逐格用 armVisible/doorArmVisible 验一遍找剩余缺口。可疑未验点：断手 ArmDrop 与魅惑/穿透组合、读档(ZombieItemUpdate)各状态(AutoTest 短路存档测不了)、FastBucket/其它二类护盾僵尸是否有类似"持armor臂"结构。

## 2026-07-22 换色派生钩子

加固铁门复用完整 DoorZombie 手臂/进食/掉门/死亡状态机。DoorZombie 将原先散落的三阶段硬编码贴图收口为 `GetDoorImageKey(stage)` + `ApplyDoorImage()`，掉落粒子收口为 `GetDoorDropParticleName()`；派生类只覆写资源键，不复制任何手臂逻辑。`smoke_door_fume_death` 可见回归通过，确认原版铁门生命周期未受影响。
