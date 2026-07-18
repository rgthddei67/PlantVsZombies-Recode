---
name: adding-zombie
description: Use when adding a new zombie (新增僵尸) to PvZ — 含防具僵尸、召唤/编队僵尸(舞王式)、出土/入地、断肢断头、掉装备粒子、与魅惑交互的僵尸 — proven on DancerZombie+BackupDancerZombie (舞王+伴舞).
---

# 给 PvZ 新增僵尸

**最高原则：不确定的事立刻问主人，不要自行推断。** 已知必问项：①**帧事件帧号**（死亡 Die 帧、啃咬 EatTarget 帧——主人看动画预览有准确答案）②断肢/断头**隐藏哪些轨道、是否换残肢材质、粒子用哪张图**（各 reanim 差异大）③数值/行为偏离原版时。**帧号口径（主人 2026-07-14 定死）：`AddFrameEvent` 真实帧号 = 预览帧号 − 1；主人报的帧号默认已 −1 过，直接用不许再减**；只有自己读预览工具时才手动 −1。

**帧号即便主人给了也必须实测触发**——"末-1 帧"陷阱：帧事件靠播放头**越过**帧号触发，死亡动画实际停帧因 reanim 而异（伴舞 anim_death 65~101，帧 100 永不触发、99 才行；舞王 146 却正常）。**症状指纹：僵尸血 0 卡在 anim_death 不消失，10 秒后 run.log 出现 `WATCHDOG force-die`**。见到即帧号问题，回报主人调小。

## 第 0 步：勘察（动手前全部做完）

1. **读 reanim**：`build/clang-release/resources/reanim/<Name>.reanim`，Grep `<name>` 提取全部 track；`anim_*` 是剪辑轨（`<f>0/-1</f>` 定活跃帧区间），其余是部件轨。重点记下：头部组（`anim_head1`=头、`anim_head2`=下巴、`anim_hair`，可能有 tongue/earing 等挂件）、外臂三段（`*_outerarm_upper/lower/hand`，注意前缀可能不统一）、有无残肢轨（`_bone`/`upper2`）、`_ground` 轨（位移速度来源）。
2. **读 C# 参考**：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Zombie\Zombie.cs` grep 僵尸名，读 UpdateZombieXxx 状态机 + SetupReanimForLostArm/LostHead + 数值。**警惕版本错位**：C# 是后期资源（舞王=disco 版有 `_upper_bone` 残肢轨；主人给的 MJ 版没有）——状态机照抄、断肢方案按手头 reanim 实际轨道定。
3. **盘点已就位基建**（常常提前有了，别重复加）：`ZombieType.h` 枚举（**多半在 `NUM_ZOMBIE_TYPES` 哨兵之后，要移进去才可出怪**）、`TestDriver.cpp` kZombieNames、`GameScene.cpp` kDevZombieTable、`AnimationTypes.h`、resources.xml 的 `<Reanimation name>`、粒子贴图。缺哪补哪。

## 实现清单

1. **类**：`Game/Zombie/<Name>.h/.cpp`，抄最像的现有僵尸（护盾换图=PaperZombie；状态机+覆写碰撞=Polevaulter；手臂显隐对称钩子=DoorZombie；召唤/编队=DancerZombie+BackupDancerZombie）。HP/速度硬编码在 `SetupZombie()`（不在 gamedata.json）。
2. **SetupZombie 覆写不调基类** → 基类的三件事自己接管：帧事件注册（Die 一次性 + EatTarget `repeating=true`，帧号=全时间线绝对帧，只在所属剪辑段播放时经过）、走路起播、`mIsPreview` 分支（预览只 PlayTrack 不注册事件）。
3. **枚举移动 + 空工厂窗口**：把枚举移到哨兵前的**同一提交**必须补齐两份 gamedata.json 条目（缺字段拒启动 exit -6）；若工厂注册在后续提交，**weight 先填 0**（哨兵前+非零权重+无工厂=生存随机抽中即空指针），注册后再解封。
4. **注册**：`GameDataManager.cpp` `#include` + `RegisterZombie(type, "ZOMBIE_X", ANIM_X, "ReanimName", &MakeZombie<T>)`——animName 必须与 resources.xml 的 `<Reanimation name>` 一致。
5. **gamedata.json ×2 preset**：`{weight, appearWave, survivalRound, offset, scale}` 五字段缺一不可；只能被召唤的僵尸 `weight: 0`（永不被抽中，AutoTest spawn_zombie 仍可直造）。注意 weight 一物两用=抽中权重+生存点数成本。
6. **粒子**：照抄 `ZombieHeadOff.xml` 改 `<Name>`+`<Image>`（图键=贴图文件名的标准派生键，如 `ZombieDancerHead.png`→`PARTICLE_ZOMBIEDANCERHEAD`），放 `particles/config/` 整目录自动加载，×2 preset。XML 标签全参考/foot-guns 见 **adding-particle skill**（勿再读 ParticleSystem 源码）。
7. **⚠️ build/ 下资源提交必须 `git add -f`**——被 .gitignore 静默挡下，`git commit` 照样"成功"但文件没进去。提交后 `git show --stat` 核对文件数。

## 断肢 / 断头（每个 reanim 单独定案，先问主人）

- 覆写 `ArmDrop()/HeadDrop()`（不调基类——基类隐藏的是普通僵尸轨道名）+ 基类阈值自动触发（臂≤2/3、头≤1/3，由 `mNeedDropArm/Head` 门控）。
- 无残肢轨道的 reanim：藏 `outerarm_lower`+`outerarm_hand`、保 `outerarm_upper` 当残端、**不换材质**（disco 伴舞式）；有 `_bone`/`UPPER2` 的才做显示残肢/换图。
- 断头隐藏头部组全体**含挂件**（hair/earing/tongue…逐 reanim 数）。
- **`ZombieItemUpdate()` 与 Arm/HeadDrop 的轨道操作严格一致**（读档重建残肢的唯一路径，漏一轨=读档后幽灵部件）。

## 僵尸专属心智清单

- **走路权威**：reanim 无 `anim_walk2` 必须覆写 `PlayWalkAnimation`（啃完回走/读档全经它）；啃食视觉残留用 `OnStartEating/OnStopEating` 对称钩子；永不覆写 `ResumeWalkAfterEat`。
- **不移动阶段**：覆写 `ZombieMove` 按状态早退（PaperZombie gasp / 舞王 SNAPPING+HOLD 同款）。
- **编队齐舞/同步动作**：动画速度必须 `SetAnimationSpeed(固定值)` 锁死——基类 `Start()` 给每僵尸随机 1.1~1.4，不锁必散拍。全队同步时钟用现成的 `Board::mBoardFrame`+`GetDanceBeatFrame()`（0~22 拍，12 逻辑步/拍，入存档），按拍映射轨道、缓存上次段位防每帧重播。
- **召唤僵尸**：`mBoard->CreateZombie(type, row, x)`（y 恒由 row 派生）；关联用 EntityManager 整型 ID（死亡自动失效）；**槽位有效性 = `GetZombie(id)` 非空 且 `IsMindControlled()` 与本体一致**——只判空则被魅惑的随从永远占位、补召失灵；行越界/永久不可用的槽要豁免，否则无限重触发召唤动作。
- **魅惑交互**：`StartMindControlled` 非虚——子类反应放 `ZombieUpdate` 里的边沿检测（`mIsMindControlled && !mCharmHandled`）；魅惑领队后新召唤单位补调 `StartMindControlled()` 继承阵营；魅惑者互啃敌方是引擎既有行为，编队混战减员属正常。
- **出土/升起**：垂直位移用 `mVisualOffset.y`（存基准值，按计时线性还原）；地面遮挡用现成 `SetClipRect(0,0,SCENE_WIDTH, groundY+margin)`，**底边取 `Board::GetZombieSpawnY(row)` 行地面线**（换地图自适应，主人指示），完成后 `ClearClipRect`；升起期不移动不啃食（覆写 StartEat 早退）。默认让动画继续播放；若主人明确要求静态出土，**只能 `Animator::Pause()` 播放头，不得把 base/extra 速度层写成 0**——后续 `PlayTrack(anim_death)` 会自动恢复 playing，RISING 读档在 `RestoreAnimState` 后须重新 Pause，并必须专项实测升起中死亡不会卡帧。

## 存读档心智清单（AutoTest 短路存档=盲区，只能脑内过+主人真机验）

- 状态机枚举/计时器/关联 ID → `SaveExtraData/LoadExtraData`；Load 首行 `if (mIsEating) return;` 再动动画。
- **SetupZombie 先于 LoadExtraData 跑**：Setup 里的出生预设（下沉、裁剪、初相位）在 Load 里按存档相位**显式撤销**，否则读档僵尸带着出生态复活。
- 关联僵尸 ID 经 `CreateZombieWithID` 保值可交叉引用；引用已死自然返回 null，无需清理回调。
- 轨道/帧位由 `RestoreAnimState` 统一恢复不用自己存；节拍驱动的僵尸在 Load 里把"上次段位缓存"置 -1 重新入拍即可。

## 验证（缺一不可）

1. `clang-release` 构建 0 warning；新 .cpp 未被编译先 `cmake --preset clang-release` reconfigure。
2. **AutoTest 冒烟**：`autotest/scripts/smoke_<name>.json`。断言抓手：`zombies.N.type/hasArm/armVisible/hasHead/track/mindControlled`（type 是字符串枚举名）。**exit 0 ≠ 通过**：逐张 Read 截图（断肢前后、编队站位、出土中段——注意升起初期整体在地面线下被裁掉是正确的，截图要卡升起 60% 时点）。
3. **死亡消失必须专门测**（末-1 帧陷阱专项）：豌豆打死→dump 确认该 type 消失+run.log 无 WATCHDOG。炸弹类走 Die() 直杀路径，**测不到**死亡帧事件。
4. 时序：`wait_seconds` 是游戏秒；关卡 20 秒起第一波普通僵尸会混入 dump，别断言"场上为空"。
5. 站位/影子不对 → 本体调 gamedata offset（免编译）、影子调代码 `ShadowComponent`。

## 完工交付：调参量清单交主人（必做环节，主人指定保留）

数值常量集中在 .cpp 顶部匿名 namespace（`constexpr float kXxx`，注释写原版出处）。完工汇报时**列全部可调量的表**：量名｜位置｜现值，跨文件须同改的（如编队两侧的动画速度/段速）显式标注"**须同改**"，gamedata 侧（offset/scale/weight）与代码侧分开列。主人会自己改数值，表就是他的操作面板。

## 流程

复杂僵尸（状态机/召唤/新机制）走完整 brainstorm（关键决策逐项问主人：帧号、断肢方案、召唤细节、魅惑交互）→spec→writing-plans；换皮/纯防具类可简短 spec 直实现。模板：`docs/superpowers/specs+plans/2026-07-10-dancer-zombie*.md`。commit 由 Codex 做、push 等主人发话。

## 关联记忆

`[[project_pvz_dancer_zombie]]`（本 skill 起源+全部 foot-gun 现场）、`[[project_pvz_zombie_eat_walk_state_machine]]`（走路权威/啃食钩子）、`[[project_pvz_charmed_zombie_feature]]`（魅惑契约）、`[[project_pvz_gamedata_json]]`（双 preset）。
