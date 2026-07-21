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

2026-07-18 舞步朝向与出土静态补全：对照 C# `UpdateReanim`，舞王入场月球步及节拍 13~15/19~21 的右举手段翻面，伴舞在相同右举手拍翻面；魅惑时整套朝向取反，啃食时回阵营默认朝向。C# `UpdateZombieWalking` 还会让未魅惑的 `DancerDancingIn` 实际向右，但主人可见 AutoTest 后明确本项目只要**视觉反向**，逻辑 X 仍向房子推进（30 帧 600→575.33）。伴舞 RISING 期间不再随拍跳舞：`PlayTrack(anim_armraise)` 后只 `Animator::Pause()`，完全出土再 `PlayTrack` 入拍；死亡轨 `PlayTrack(anim_death)` 会自动恢复 playing，避免速度层置 0 导致卡死，RISING 读档须重新 Pause。AutoTest 新增 `flipX/animPlaying` 状态抓手与通用 `damage_zombie` 正式受伤链；可见窗口跑 `smoke_dancer_summon/backupdancer/charm/death/backupdancer_rising_death` 全绿（专项确认 Pause→anim_death 自动唤醒→5 秒内 zombieNumber=0、无 WATCHDOG），clang-release 全量构建 0 warning。

2026-07-19 图鉴补全：两个 preset 的 `resources/info.txt` 均新增 `ZOMBIE_DANCER` / `ZOMBIE_BACKUP_DANCER` 名称与描述；文案采用“先讲机制、再由僵尸本人一本正经歪解”的宝开式结构。新增可见 UI 回归 `autotest/scripts/smoke_dancer_almanac.json`，从主菜单进入僵尸图鉴后依次截图网格、舞王正文与伴舞正文；clang-release 实跑 exit 0、run.log finished OK，截图确认标题居中、17号正文各5行且无溢出。

2026-07-21 新增继承型 `EliteDancerZombie`，父类 `SetupZombie()` 继续作为 Jackson 帧事件、断肢断头与月球漫步契约来源；派生类绕过普通舞王四阶段状态机，持续推进并用独立 vector 维护最多 8 只直属普通伴舞。详情与调参见 [project_pvz_elite_dancer_zombie](project_pvz_elite_dancer_zombie.md)。同日冒险出怪重排后，普通舞王只进入 2-7 的独立教学池与 2-9 综合池，避免和两种橄榄球在 2-8 同时首次出现；见 [project_pvz_night_spawnlist_pacing](project_pvz_night_spawnlist_pacing.md)。

**Foot-guns（写skill时要收录）：**
- **帧事件"末-1帧"不触发**：伴舞anim_death 65~101，主人先给100(=末-1)实测播不到→僵尸血0卡anim_death不消失(靠10s看门狗)；主人改99才触发。但Jackson死亡146(=末-1)却正常触发——**逐reanim实测，不能推公式**。
- **齐舞散拍源**：基类`Start()`给每僵尸随机动画速度1.1~1.4，舞队必须`SetAnimationSpeed`锁同值(现1.2)。
- **魅惑脱队不清领队侧槽位**：伴舞被魅惑只清自己mLeaderID，领队mFollowerID仍指活僵尸→判空补位失灵；修=槽位有效性判"活着且IsMindControlled与领队一致"。
- **出土遮挡现成方案**：`GameObject::SetClipRect`逐对象裁剪(图鉴僵尸窗同款)，底边用`Board::GetZombieSpawnY(row)`(主人指示,已挪public)而非自身坐标——换地图自适应；DANCING态读档要撤销下沉+裁剪。
- **升起期静态只能 Pause 播放头**：不要把基础/extra 速度写成 0；`Animator::Pause()` 可被后续 `PlayTrack(anim_death)` 自动唤醒，RISING 读档在 RestoreAnimState 后必须重新 Pause。
- **`git add build/资源`被.gitignore静默挡**：gamedata.json/粒子XML须`git add -f`，首次commit后警惕"提交成功但文件没进去"。
- **枚举进哨兵前的空工厂窗口**：weight两段式(先0,注册工厂后升1200)。
- **暂停≠逻辑停**：本引擎暂停时逻辑步照跑只把dt置0(UI要消费点击)，任何不乘dt的状态源(帧计数++、轮询条件)暂停时照样演化——mBoardFrame无条件++曾致暂停中舞队随节拍翻转瞬间切轨(536c424修)。新加"每N帧做X"类逻辑必须挂dt。

主人待验收点：齐舞节奏、断手视觉(藏小臂+手保大臂无粒子)、真机存读档(AutoTest短路测不到)。出土裁剪当前 `kGroundClipMargin=38`，2026-07-18 可见 AutoTest 已确认中段静态姿势。调参表在 docs/superpowers/plans/2026-07-10-dancer-zombie.md 末尾。原版C#对照=disco版(断手有_bone残肢轨道,MJ版没有)；spec/plan在 docs/superpowers/{specs,plans}/2026-07-10-*。关联 [project_pvz_charmed_zombie_feature](project_pvz_charmed_zombie_feature.md) [project_pvz_zombie_eat_walk_state_machine](project_pvz_zombie_eat_walk_state_machine.md)
