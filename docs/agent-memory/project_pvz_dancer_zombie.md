---
name: project_pvz_dancer_zombie
description: 舞王+伴舞僵尸(MJ版)全套实现——Board节拍时钟齐舞/十字召唤补位/出土裁剪/魅惑阵营判定；经验已固化为.claude/skills/adding-zombie(3f7c504)；536c424已push
metadata:
  node_type: memory
  type: project
  originSessionId: 96e2a8d6-aa5e-4f8a-bb27-cf3a98763128
---

2026-07-10(d14a526..3f7c504) 舞王(Zombie_Jackson)+伴舞(Zombie_dancer)实现完毕，7 smoke全绿；**skill已写(.claude/skills/adding-zombie, CLAUDE.md僵尸节已改指向)**。2026-07-11 536c424 起全部已push到origin/master。

架构：`Board::mBoardFrame`(入存档；536c424起按GetDeltaTime/固定步长折算推进——暂停dt=0冻结、倍速与Animator同拍，亚帧余量mBoardFrameAccum不入存档) → `GetDanceBeatFrame()`=0~22拍(12步/拍)；舞王/伴舞都按拍映射 anim_walk(0~11)/anim_armraise(12~22)，全队零通信齐舞。舞王四阶段 DANCING_IN(碰植物啃一口后转SNAPPING：EatTarget覆写首口结算后强停啃食=平衡mEaterCount/清目标ID复刻StartMindControlled规范；SNAPPING/HOLD期StartEat守卫不开吃,onTriggerStay进DANCING后自然续啃)→SNAPPING(anim_point播完定格,IsPlaying轮询收尾召唤)→HOLD(1.2s保持举手定格,=伴舞kRiseDuration须同改)→DANCING(节拍12缺人补召,x<700,加scaledTime>0守卫防暂停中瞬切举手)。关联=EntityManager整型ID(舞王mFollowerID[4]/伴舞mLeaderID)。

2026-07-11打磨(4b114ed)：①伴舞出土隐影=ShadowComponent新增SetVisible(Draw首行early-return)，SetupZombie藏/升完+读档DANCING恢复(影子组件在Zombie::Start先于SetupZombie挂上,可直接GetComponent)；②舞王举手改`PlayTrackOnce(track,"")`=原版PlayOnceAndHold(播完mFrameIndexNow=end+mIsPlaying=false定格末帧,后续任意PlayTrack自动复活,不踩定格卡死)；③**召唤触发弃36帧帧事件改SNAPPING轮询`!mAnimator->IsPlaying()`**(同C#查mLoopCount)——末帧定格clamp时帧事件不保证触发；啃食中ZombieUpdate整个不跑(Zombie.cpp mIsEating早退)故轮询无误触发。

**Foot-guns（写skill时要收录）：**
- **帧事件"末-1帧"不触发**：伴舞anim_death 65~101，主人先给100(=末-1)实测播不到→僵尸血0卡anim_death不消失(靠10s看门狗)；主人改99才触发。但Jackson死亡146(=末-1)却正常触发——**逐reanim实测，不能推公式**。
- **齐舞散拍源**：基类`Start()`给每僵尸随机动画速度1.1~1.4，舞队必须`SetAnimationSpeed`锁同值(现1.2)。
- **魅惑脱队不清领队侧槽位**：伴舞被魅惑只清自己mLeaderID，领队mFollowerID仍指活僵尸→判空补位失灵；修=槽位有效性判"活着且IsMindControlled与领队一致"。
- **出土遮挡现成方案**：`GameObject::SetClipRect`逐对象裁剪(图鉴僵尸窗同款)，底边用`Board::GetZombieSpawnY(row)`(主人指示,已挪public)而非自身坐标——换地图自适应；DANCING态读档要撤销下沉+裁剪。
- **升起期动画不定格**：定格骨架则升起中被打死卡冻结帧无法播anim_death（偏离原版，主人验收观感）。
- **`git add build/资源`被.gitignore静默挡**：gamedata.json/粒子XML须`git add -f`，首次commit后警惕"提交成功但文件没进去"。
- **枚举进哨兵前的空工厂窗口**：weight两段式(先0,注册工厂后升1200)。
- **暂停≠逻辑停**：本引擎暂停时逻辑步照跑只把dt置0(UI要消费点击)，任何不乘dt的状态源(帧计数++、轮询条件)暂停时照样演化——mBoardFrame无条件++曾致暂停中舞队随节拍翻转瞬间切轨(536c424修)。新加"每N帧做X"类逻辑必须挂dt。

主人待验收点：出土穿模观感(kGroundClipMargin=8)、齐舞节奏、断手视觉(藏小臂+手保大臂无粒子)、真机存读档(AutoTest短路测不到)。调参表在 docs/superpowers/plans/2026-07-10-dancer-zombie.md 末尾。原版C#对照=disco版(断手有_bone残肢轨道,MJ版没有)；spec/plan在 docs/superpowers/{specs,plans}/2026-07-10-*。关联 [project_pvz_charmed_zombie_feature](project_pvz_charmed_zombie_feature.md) [project_pvz_zombie_eat_walk_state_machine](project_pvz_zombie_eat_walk_state_machine.md)
