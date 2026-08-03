---
name: adding-zombie
description: Use when adding or tuning any PvZ zombie, or integrating zombies into adventure/survival spawn pools — 含防具/换皮数值变体、召唤/编队、天气变异、出土/入地、断肢断头、掉装备粒子、魅惑交互与冒险出怪编排 — proven on DancerZombie, PinkFootballZombie, EliteDancerZombie and EliteDiggerZombie.
---

# 给 PvZ 新增僵尸

**最高原则：不确定的事立刻问主人，不要自行推断。** 已知必问项：①**帧事件帧号**（死亡 Die 帧、啃咬 EatTarget 帧——主人看动画预览有准确答案）②断肢/断头**隐藏哪些轨道、是否换残肢材质、粒子用哪张图**（各 reanim 差异大）③数值/行为偏离原版时。**帧号口径（主人 2026-07-14 定死）：`AddFrameEvent` 真实帧号 = 预览帧号 − 1；主人报的帧号默认已 −1 过，直接用不许再减**；只有自己读预览工具时才手动 −1。

**帧号即便主人给了也必须实测触发**——"末-1 帧"陷阱：帧事件靠播放头**越过**帧号触发，死亡动画实际停帧因 reanim 而异（伴舞 anim_death 65~101，帧 100 永不触发、99 才行；舞王 146 却正常）。**症状指纹：僵尸血 0 卡在 anim_death 不消失，10 秒后 run.log 出现 `WATCHDOG force-die`**。见到即帧号问题，回报主人调小。

## 坐标换算铁律

**C# 原版逻辑场景是 800×600，本项目是 `SCENE_WIDTH=1100`、`SCENE_HEIGHT=600`。原版任何绝对 X/Y、碰撞框偏移、绘制偏移、粒子触发点和屏幕边界都只能当语义参考，禁止直接抄入代码。**

- 场景绝对坐标改由 `SCENE_WIDTH/SCENE_HEIGHT`、Board 网格与当前背景几何重新派生；不要把 800 宽地图的右边界、出生点或阈值原样搬来。
- 原版按世界 X 分段的速度/状态阈值写成“当前地图坐标基准 + 原版相对距离”；平地可从 `CELL_INITALIZE_POS_X` 派生。屋顶连续坡面由 `Board::GetRoofSlopeEndX/GetRowCenterYAtX` 唯一拥有，当前交界 X 从 `CELL_INITALIZE_POS_X + 5 * CELL_COLLIDER_SIZE_X` 派生，禁止品种复制斜坡公式。
- 僵尸局部坐标先求“原值相对 C# 绘制原点”的差，再锚到本项目稳定视觉原点 `Transform + mVisualOffset`；物理框、攻击框和粒子通常不得跟受伤抖动等临时绘制偏移移动。
- 网格位置与画面偏移继续分离；屋顶出生、读档重建、出土裁剪及任意当前点地面线必须使用 `GetZombieCollisionY/GetZombieSpawnY(row, worldX)`。水平移动仍由品种的 `ZombieMove` 决定，基类在阵风后和 `ZombieMove` 后统一把 Transform Y 收敛到坡面，普通、飞行、地下品种不得各自维护 Y 公式。
- 验证先执行同步 `screenshot`，再用 `animatedObjectsByTag.Zombie` 的 `renderProbeReady/worldBounds/visualToRenderCenterD*Int/nearestZombie` 验证本项目最终绘制几何相对自身 collider、同排植物或攻击目标的关系；每阶段只保留一个目标僵尸以稳定数组索引。
- 修改出生 offset、受伤偏移、附件、翻转或整身变换时，默认实例化与 `-NoInstance` 各跑同一静止用例并比较整数 `worldBounds`；运动对象瞬时绝对 X/Y 只供诊断，不作稳定断言。

## 第 0 步：勘察（动手前全部做完）

1. **读 reanim**：`build/clang-release/resources/reanim/<Name>.reanim`，Grep `<name>` 提取全部 track，并 Grep `<i>` 列出每个实际图片键；`anim_*` 是剪辑轨（`<f>0/-1</f>` 定活跃帧区间），其余是部件轨。重点记下：头部组（`anim_head1`=头、`anim_head2`=下巴、`anim_hair`，可能有 tongue/earing 等挂件）、外臂三段（`*_outerarm_upper/lower/hand`，注意前缀可能不统一）、有无残肢轨（`_bone`/`upper2`）、`_ground` 轨（位移速度来源），以及 `rise2～6` 这类把帽子、脸和衣服烘在一起的阶段合成图。独立部件换色不会影响合成图，必须按实际 `<i>` 引用逐张盘点。
2. **读 C# 参考并主动盘点音效**：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Zombie\Zombie.cs` grep 僵尸名，读 UpdateZombieXxx 状态机 + SetupReanimForLostArm/LostHead + 数值，同时收集相关路径的全部 `PlayFoley` / `PlaySample`；不要等主人听出缺声才补。沿 `FoleyType → Sexy.TodLib/Foley/TodFoley.cs → Resources.SOUND_*` 得到精确资源键，以资源键去掉 `SOUND_` 后的小写名到 `D:\PVZ\中文年度加强版完整版\Test\sounds\` 查同名 `.ogg`。找到后复制到唯一权威 `build/clang-release/resources/sounds/` 合理子目录，并同步 `resources.xml` 与 `ResourceKeys.h`；找不到才问主人，禁止用相近声音静默替代。构建后检查 `manifest.txt` 和启动日志无 missing sound，并用可见行为路径及 `GetSoundPlayRequestCount` 投影验证触发次数（含读档不得重响）。出生/登场声若只属于“新刷新”，必须从 `Board::CreateZombie` 的正式新建路径调用品种钩子，禁止塞进 `SetupZombie()`（预览与 `CreateZombieWithID` 读档也会经过 Setup）。**警惕版本错位**：C# 是后期资源（舞王=disco 版有 `_upper_bone` 残肢轨；主人给的 MJ 版没有）——状态机照抄、断肢方案按手头 reanim 实际轨道定。
3. **盘点已就位基建**（常常提前有了，别重复加）：`ZombieType.h` 枚举（**多半在 `NUM_ZOMBIE_TYPES` 哨兵之后，要移进去才可出怪**）、`TestDriver.cpp` kZombieNames、`GameScene.cpp` kDevZombieTable、`AnimationTypes.h`、resources.xml 的 `<Reanimation name>`、粒子贴图。缺哪补哪。**reanim 文件进入 manifest 不等于已注册**：新角色必须同时把文件放入权威 `resources/reanim/` 并在 `resources.xml` 写 `<Reanimation name="...">`；漏后者会让 `AnimatedObject` 得到空 Animator，僵尸构造阶段访问轨道时直接 Access Violation。AutoTest 在首次直造前用 `ResourceManager::HasReanimation` 断言注册键。不要给 `Zombie`/`AnimatedObject` 基类加宽泛空 Animator 早退来“止崩”，`Start()`/`SetupZombie()` 仍会访问它，且坏注册会被掩盖。开发者面板会把所选僵尸的**枚举名字符串**写入 `PlayerInfo.json`；禁止改存表下标或枚举整数，因为新类型移入哨兵前会让旧数值漂移。新增表项继续用 `DEVZ(ZOMBIE_X)`，并让 `smoke_develop` 断言选择跨场景重建仍保持。
4. **若任务含冒险出怪表**：完整阅读 [references/adventure-spawnlist-pacing.md](references/adventure-spawnlist-pacing.md)，先画整大关的首次登场/复习/综合表，再改 JSON；禁止只盯被抱怨的单关局部挪怪。

## 实现清单

1. **类**：`Game/Zombie/<Name>.h/.cpp`，抄最像的现有僵尸（护盾换图=PaperZombie；状态机+覆写碰撞=Polevaulter；手臂显隐对称钩子=DoorZombie；召唤/编队=DancerZombie+BackupDancerZombie）。HP/速度硬编码在 `SetupZombie()`（不在 gamedata.json）。
2. **SetupZombie 先判定“复用父类”还是“完全接管”**：
   - **同一套 reanim/轨道/事件时序的换皮或数值变体**，优先调用最近父类的 `SetupZombie()`，再覆盖 HP、攻击和速度差额；这样直接复用已经验证过的 Die/EatTarget 事件，禁止重复 `AddFrameEvent`。父类已经乘过移速时，用“目标倍率 / 父类倍率”补差（粉色橄榄球 1.85/1.7 实证），不要再乘完整目标倍率。
   - **新 reanim、事件时序不同或状态机需要替换父类初始化**，才不调基类并自己接管三件事：帧事件注册（Die 一次性 + EatTarget `repeating=true`，帧号=全时间线绝对帧，只在所属剪辑段播放时经过）、走路起播、`mIsPreview` 分支（预览只 PlayTrack 不注册事件）。任何新增帧号仍必须先问主人。
   - **预览运动不能假定会进入品种实战更新**：`Zombie::Update()` 对 `mIsPreview` 只推进 `AnimatedObject`，不会调用 `ZombieUpdate/ZombieMove`。若上下跳、附件或阶段姿态由品种状态机而非 Animator 自身产生，覆写 `Update()`，先调 `Zombie::Update()`，再只为非 UI 大图推进无碰撞、无位移、无音效的展示状态；图鉴网格设置 `mIsUI + PauseAnimation()` 时必须继续静止。AutoTest 分别在选卡与图鉴详情导出相对高度/阶段整数投影，不断言运动对象绝对 X/Y。
   - **僵尸自身整体动画倍率只有一个出口**：覆写 `GetAbilityAnimSpeedMultiplier()`，固定品种值直接返回常量，阶段能力从已保存状态派生，出生随机值存派生类字段并由 `SaveExtraData/LoadExtraData` 持久化。状态变化后调用 `UpdateAnimSpeed()`；禁止子类直调 `Animator::SetExtraSpeedMultiplier()` 或再造一份通用基础倍率字段。`mSpeed` 只表示额外水平位移，`PlayTrack(..., clipSpeed)` 只表示当前轨道绝对速度。
   - **带 `_ground` 的根运动僵尸禁止把目标世界速度直接写进 `mSpeed`**：`GetTrackVelocity()` 已包含逐帧根位移与 `EffectiveSpeed()`，`ZombieMove()` 随后才乘 `mSpeed × delta`；因此 `mSpeed` 应承担资源 FPS 的时间基准换算，品种快慢主要用与 C# `mVelX` 对齐的 clip 速度同步改变步频和位移。直接把 42/54px/s 填给 `mSpeed` 会让身体滑行。AutoTest 至少断言 `effectiveAnimSpeed`，并从池外稳定起点检查进入泳池前仍保留可见步行时长。
   - **非魅惑状态也会反向移动的品种必须集中方向权威**：在 `Zombie` 提供 `IsMovingRight()` 虚入口，让基类位移、台风顺逆风、目标提前量和房屋失败线都查询它；子类只按当前 phase/阵营覆写一次。禁止仅在子类 `ZombieMove()` 里改正负号，否则会出现“视觉向右但仍触发进家”或阵风/索敌预测反向。AutoTest 同时断言方向、位移趋势与 `CanTriggerGameOver()`，并在房屋附近验证反向阶段不触发失败。
   - **跨轨道换态先核对视觉坐标系，不能只看逻辑位移**：从同一身体部件轨读取旧轨末帧与新轨首帧锚点，再叠加各阶段 `mVisualOffset`/视觉补偿，世界提交位移要抵消总差；海豚 `anim_dolphinjump→anim_swim/anim_ride` 当前实测分别需 104/106 px，原版 94 只复制逻辑位移会在落地时倒退。若换态同帧撤销视觉补偿，禁止继续 blend 旧姿态（海豚 `anim_ride→anim_walkdolphin` 上岸须零混合），否则旧姿态会被新坐标绘制成短暂垂挂。C# 的 `mUsesClipping` 也不等于通用水线裁剪：海豚入水只在 0.56～0.65 与 0.75～结束使用较低的局部底线，中间关闭；在本项目用派生类裁剪钩子复刻并截图校准，默认实现必须保持其他水中僵尸原样，禁止通过隐藏海豚部件轨或裁整只僵尸冒充。
   - **把父类资源选择改成虚入口时必须双向回归**：派生类换色通过后，仍要触发父类的受损帽、残肢、掉落粒子和读档终态；新增虚入口可能把父类原先未被断言的错误键暴露出来。AutoTest 分别导出父类与派生类的实际资源键加载状态，不能只证明精英路径。
3. **枚举移动 + 空工厂窗口**：新类型必须**追加在全部既有已实现类型之后、`NUM_ZOMBIE_TYPES` 哨兵之前**；禁止插进旧类型中间，否则存档里的整数僵尸 ID 会错位。把枚举移到哨兵前的**同一提交**必须补齐权威 `gamedata.json` 条目（缺字段拒启动 exit -6）；若工厂注册在后续提交，**weight 先填 0**（哨兵前+非零权重+无工厂=生存随机抽中即空指针），注册后再解封。
4. **注册**：`GameDataManager.cpp` `#include` + `RegisterZombie(type, "ZOMBIE_X", ANIM_X, "ReanimName", &MakeZombie<T>)`——animName 必须与 resources.xml 的 `<Reanimation name>` 一致。
5. **gamedata.json**：只改 `build/clang-release/resources/gamedata.json`，`{weight, appearWave, survivalRound, offset, scale}` 五字段缺一不可；只能被召唤的僵尸 `weight: 0`（永不被抽中，AutoTest spawn_zombie 仍可直造）。注意 weight 一物两用=抽中权重+生存点数成本。
6. **粒子**：照抄 `ZombieHeadOff.xml` 改 `<Name>`+`<Image>`（图键=贴图文件名的标准派生键，如 `ZombieDancerHead.png`→`PARTICLE_ZOMBIEDANCERHEAD`），放权威 `build/clang-release/resources/particles/config/`，其他 preset 自动共享。新粒子专用 PNG 还必须登记进 `resources.xml` 的 `<ParticleTextures>`；只有文件和 manifest 不会加载出 `PARTICLE_*` 键。XML 标签全参考/foot-guns 见 **adding-particle skill**（勿再读 ParticleSystem 源码）。
7. **换色变体资源**：优先用仓库内 PowerShell + `System.Drawing` 脚本按 HSV/亮度映射目标材质，保留原 Alpha、阴影、高光、描边和非目标部件；不要对整张图平涂或只靠 overlay。脚本是可复现源，只向 clang-release 权威资源生成一次。先把 reanim 全部 `<i>` 引用建立源→目标表，尤其逐张处理阶段合成图；空间/低饱和度遮罩必须在原分辨率与原图并排检查，防止把眼白、灯泡、牙齿等一起染色，**视觉检查通过后**才把最终 SHA-256 写回脚本并复跑锁定。换色后还要沿死亡/受击入口检查粒子 XML 的每个车辆或身体部件 `<Image>`：本体 reanim 换色不会自动替换粒子里写死的普通资源键。**仅存在于 `image/reanim/` 但未被 reanim XML 引用的受损帽、残肢等运行时换图，manifest 驱动的启动扫描会生成文件标准键 `IMAGE_<UPPERCASE_STEM>`，不是 `IMAGE_REANIM_*`；后者只由 reanim loader 为时间线实际引用的图片建立。** 用 `GetTexture(key, false)` 导出加载断言，同时覆盖正常换图和读档重建。派生换色品种的断肢应由父类虚入口同时选择“本体残留材质”和“飞出粒子效果”，并让 `ZombieItemUpdate()` 复用同一材质入口，避免受伤或读档时短暂变回普通配色。
8. **⚠️ build/ 下资源提交必须 `git add -f`**——被 .gitignore 静默挡下，`git commit` 照样"成功"但文件没进去。提交后 `git show --stat` 核对文件数。
9. **图鉴**：在权威 `build/clang-release/resources/info.txt` 同时添加 `[ZOMBIE_X]` 与 `[ZOMBIE_X_DESCRIPTION]`。`ZombieAlmanacScene` 按 `mAdventureLevel - 1` 之前已通关关卡的 `spawnlists.json` 并集解锁条目，并按首次遭遇顺序排列；当前正在玩的关卡不得提前泄露。召唤型 `weight: 0` 子单位（如伴舞）不能为了图鉴解锁写进随机池，而应由图鉴的“必然派生遭遇”映射随其召唤者解锁。概率变异不能由 spawnlist 推断；若要求实际遇见后永久解锁，把独立遭遇标记存入 `PlayerInfo.json`，且只在正式波次的实际类型成功创建后记录，不能在 roll 命中、通用 `CreateZombie()`、读档或预览路径记录。缺 info key 不会构建失败，只会留下有图无标题/正文的空白条目；因此静态检查每个可解锁枚举名的两枚 key 均存在且唯一。AutoTest 用 `set_adventure_level` 配合 UI 场景状态字段 `zombieAlmanacEntries` / `zombieAlmanacSelected`，同时断言当前关排除、下一关解锁并截图；AutoTest 会短路真实 PlayerInfo 磁盘写入，遭遇持久化须用内存字段断言加保存/加载源码审查。

## 冒险出怪编排

修改 `spawnlists.json` 时完整遵循 [references/adventure-spawnlist-pacing.md](references/adventure-spawnlist-pacing.md)。核心不是“把新僵尸塞进一个可生成关卡”，而是围绕玩家在**关卡开始时**已有的植物，给重点敌人安排独立教学、无同场复习和最终综合，并用精简池保证重点敌人的实际抽中率与选卡预览可读性。

## 断肢 / 断头（每个 reanim 单独定案，先问主人）

- 覆写 `ArmDrop()/HeadDrop()`（不调基类——基类隐藏的是普通僵尸轨道名）+ 基类阈值自动触发（臂≤2/3、头≤1/3，由 `mNeedDropArm/Head` 门控）。
- 无残肢轨道的 reanim：藏 `outerarm_lower`+`outerarm_hand`、保 `outerarm_upper` 当残端、**不换材质**（disco 伴舞式）；有 `_bone`/`UPPER2` 的才做显示残肢/换图。
- 断头隐藏头部组全体**含挂件**（hair/earing/tongue…逐 reanim 数）。
- **`ZombieItemUpdate()` 与 Arm/HeadDrop 的轨道操作严格一致**（读档重建残肢的唯一路径，漏一轨=读档后幽灵部件）。

## 僵尸专属心智清单

- **走路权威**：reanim 无 `anim_walk2` 必须覆写 `PlayWalkAnimation`（啃完回走/读档全经它）；啃食视觉残留用 `OnStartEating/OnStopEating` 对称钩子；永不覆写 `ResumeWalkAfterEat`。
- **入水/出水反馈只绑定真实介质边沿**：通用僵尸以决定 `mInPool` 的同一对探针中点和水面裁剪底线作为水花世界锚点；C# `PoolSplash` 是一次性 `Splash.reanim` + `PlantingPool` 水滴两层，必须分别创建并按入水/出水选择 Foley。首次生成在水中、预览与读档恢复只静默同步介质，禁止伪造跨界声画；海豚等自管入水动作在 C# 进度节点调用共用水花视觉，动作结束提交 `mInPool` 时显式静默，避免末端重复一次。AutoTest 一负一正断言节点前计数 0、节点后两层各 1，并在快照重载后确认计数和声音请求都不再增加。
- **飞行/落地是命中层状态机，不只是视觉偏移**：由僵尸虚接口按当前 phase 声明空中层、地面层或过渡期不可命中，植物索敌与子弹碰撞都查询该接口；高低弹丸的层标记必须随对象池 `Reset()` 归零并入存档。飞行期同时禁用啃食、地面水池状态、冻结/黄油和断肢阈值，落地后再统一补结算；水道气球被击破按原版直接 `Die()`，不能先进入落地动画。飞行额外生命应在基类发光反馈后、普通防具/本体前由虚钩子消费，并参与全局生命倍率。专项至少覆盖空/地弹互斥、爆裂过渡、陆地落地、水道直接消失与存读档。
- **受击白光必须按实际承伤层驱动**：禁止在 `TakeDamage` 分层前统一整身 `SetGlowingTimer`。原版本体/头盔/飞行额外生命共用本体计时器，报纸/铁门/梯子等二类护盾使用独立计时器和盾轨高亮；普通正面弹只伤盾就只闪盾，穿透或破盾溢出确实伤到后层才同时闪。所有 `Bullet` 的直接伤害统一走 `Zombie::TakeProjectileDamage`，按命中时 `velocityX` 与 `IsMovingRight()` 判断命中面：子弹与面向同向即从背后追上，完全绕过二类护盾并只闪后层；`velocityX=0` 保持正面口径。禁止按子弹类型或运动模式建**方向性**绕盾白名单，追踪弹须持续写回真实速度。弹丸自身能力若主动请求绕盾，必须通过同一入口的显式参数表达，并由目标侧 `BlocksProjectileShieldBypass` 虚接口按当前防具状态否决；方向背击优先保持原物理语义，目标否决不得顺带封死背击。`penetrateShield` 会让盾与后层同时承伤，不能用来冒充完全绕盾。围绕 `TakeExtraProtectionDamage`、`TakeShieldDamage`、`TakeHelmDamage`、`TakeBodyDamage` 的虚调用比较扣血前后值，避免派生覆写漏报；盾轨覆盖必须同时接入实例化与 `-NoInstance` 绘制路径。AutoTest 用 `hitFlashMask` 锁逻辑层、`renderedHitGlowMask` 锁实际轨道层，并同步截图检查铁门/报纸的正向、反向、主动绕盾、目标否决与静止弹。
- **灰烬致死若要求直接消失，不能只改 `CanBeCharred()`**：覆写 `TakePlantAshDamage()`，按词条缩放后的最终伤害与“本体 + 当前额外生命层”判断是否确实致死；致死直接 `Die()`，非致死继续 `TakeDamage(..., PLANT_ASH)`。同时覆写 `Charred()` 收口其他兼容调用，避免落地态仍生成烧焦残影。带额外生命层时不要简单放开飞行态 `CanBeCharred()`，因为通用调用方多只比较 `mBodyHealth`，会把本应剩血的目标误删。AutoTest 至少分别覆盖额外层仍在与落地后两态，并断言实体、`charredZombieCount` 和特殊爆裂音效。
- **同 reanim 独立附件**：螺旋桨等需要与主轨同时循环的部件可实例化第二个 Animator、只播放附件 clip，再挂到主 Animator 的稳定锚点；附件 clip 必须清除主轨下发的 clip 速度覆写，但保留冻结/减速 extra 倍率。父锚点隐藏不会自动阻止附件绘制，掉头、死亡和读档终态都要显式隐藏/暂停子 Animator；附件帧、播放态和宿主 phase 一起入档。
- **不移动阶段**：覆写 `ZombieMove` 按状态早退（PaperZombie gasp / 舞王 SNAPPING+HOLD 同款）。
- **共享循环音效要有物种级所有权**：`AudioSystem` 的循环 key 是全局共享资源，一只实例直接 `StopLoopingSound` 会误停仍存活的同类。为品种维护静态引用计数，每实例用布尔值保证只申领/释放一次；只在正式出生钩子和 RUNNING 读档恢复时申领，在开盒/换态、掉头、死亡和析构时释放，计数归零才真正停止。Load 不得重播 boing/surprise/explosion 等一次性声音。AutoTest 至少断言循环实际播放状态与一次性请求计数；允许同品种并存时再造两只，验证一只退态后循环仍由另一只保持。
- **编队齐舞/同步动作**：动画速度必须 `SetAnimationSpeed(固定值)` 锁死——基类 `Start()` 给每僵尸随机 1.1~1.4，不锁必散拍。全队同步时钟用现成的 `Board::mBoardFrame`+`GetDanceBeatFrame()`（0~22 拍，12 逻辑步/拍，入存档），按拍映射轨道、缓存上次段位防每帧重播。
- **召唤僵尸**：`mBoard->CreateZombie(type, row, x)`（y 恒由 row 派生）；关联用 EntityManager 整型 ID（死亡自动失效）；**槽位有效性 = `GetZombie(id)` 非空 且 `IsMindControlled()` 与本体一致**——只判空则被魅惑的随从永远占位、补召失灵；行越界/永久不可用的槽要豁免，否则无限重触发召唤动作。持续补召必须把“最多维持数量”和“补召间隔”集中为匿名 namespace 可调常量，并明确计时使用哪种 delta：精英舞王实证为冻结暂停、减速按 scaled delta 拖慢，天气动画倍率不得重复加速召唤逻辑。
- **跳跃阻拦要拆分职责、动画时序和受伤对象**：阻拦植物通过虚接口声明“能挡哪类跳跃”并拥有 Bonk/粒子反馈；接触回调只锁定当前格顶层目标并起播跳跃，必须到 C#/规格给出的动画进度节点才查询一次，禁止一碰植物就提前阻拦。仅按进度检查不需要新增 `AddFrameEvent`；目标 ID、是否已检查和额外根位移须入档，读档恢复原动画帧继续判定，不能直接落地绕过阻拦。跳跃计时必须明确使用真实 delta 还是寒冰后的 scaled delta；若某半程按独立倍率加速，额外根位移必须消费同一时间基准和倍率，且不得把已缩放的 delta 再乘 Animator extra 倍率造成重复减速。被挡时先撤回品种已补的额外位移，再恢复碰撞/阴影、弃杆或落地、切稳态并开始啃食，最后把阻拦植物传给派生钩子结算召唤或撞击。**规格写“给 A N 点伤害”先确认 A 是植物还是跳跃者，禁止把碰撞伤害凭感觉记到自身**；常规僵尸对植物伤害走 `Plant::TakeDamage(N, DamageSource::ZOMBIE)` 以统一消费僵尸增伤与植物韧性，只有主人明确要求最终固定扣血才另设不缩放入口。若要求被挡后仍召唤，先创建召唤物再伤害目标。AutoTest 必须同时断言节点前 `JUMPING/anim_jump`、无 Bonk/粒子/扣血，节点后才出现阻拦终态和派生效果。
- **手动开吃不能依赖碰撞退出收尾**：跳跃受阻等状态机可能把僵尸停在植物碰撞箱外的小间隙后直接调用 `StartEat`，这对对象从未进入 `CollisionSystem::currentCollisions`，外力吹离时不会产生 `onTriggerExit`。基类必须逐帧复核已保存的植物目标仍存活、仍是同格顶层且碰撞箱间距仍在允许咬合范围；失效时原子清目标 ID、平衡 `mEaterCount`、调用 `OnStopEating` 并经 `PlayWalkAnimation` 恢复。进入死亡轨道前也要先清啃食状态，`EatTarget` 对垂死者硬早退。AutoTest 同时断言吹离、目标死亡、啃食者死亡三条路径的 `isEating=false`、目标 ID 为空和 `eaterCount=0`。
- **魅惑交互**：`StartMindControlled` 非虚——子类反应放 `ZombieUpdate` 里的边沿检测（`mIsMindControlled && !mCharmHandled`）；魅惑领队后新召唤单位补调 `StartMindControlled()` 继承阵营；魅惑者互啃敌方是引擎既有行为，编队混战减员属正常。
- **魅惑范围伤害按阵营对称过滤**：僵尸自身发起的爆炸或范围伤害若应攻击敌方，统一用 `target->IsMindControlled() != source->IsMindControlled()` 判敌，禁止只在来源已魅惑时跳过魅惑目标，否则未魅惑来源会误伤普通僵尸。AutoTest 的未魅惑与魅惑两侧都同时放普通僵尸、魅惑僵尸和植物，先证明敌方目标实际受击，再断言同阵营与植物是否按设计保留。
- **离体但仍由僵尸持有的延迟攻击要按投出瞬间锁定**：保存飞行起终点、已飞时间、目标行、下一次倒计时和投出时阵营；飞行更新必须放在冻结、掉头和死亡动画的新攻击门禁之前，使已经离手的攻击继续结算。投出时隐藏手持轨、落地后恢复，`LoadExtraData()` 与 `ZombieItemUpdate()` 都按飞行状态重建显隐；若设计要求投掷者被立即回收后仍保留攻击，则不能继续把投射物只存在僵尸对象内，必须改成独立实体。
- **范围攻击的贪心落点要按实际结算集合评分**：先从合法行/格或实体建立候选落点，再用与最终伤害完全相同的活动状态、阵营过滤、圆/碰撞框相交和组合植物拦截规则累计每个爆点覆盖目标的价值；位置倍率、经济产出预期等权重施加到每个受影响目标，而不是只看被瞄准单体或硬切目标优先级。新增南瓜等外壳后，需先以原几何找出命中植物，再按逻辑格归并：有活动外壳的格只结算外壳一次及其规格倍率，无外壳格才保持原逐层伤害；不要只遍历全植物直接扣血，也不要让普通版本误接精英专属拦截。蒙特卡洛/轻量推演须保存层类型并复用同一承伤集合。无合法目标时不空投且用有界短间隔重试，最高分并列可随机择一。专项覆盖陆地壳内植物、水路 under+normal+壳、无壳旧行为、倍率伤害和明确豁免的普通版本。
- **可复用的短视未来选点由 `Board` 采集、纯数值模块推演**：`Board` 是唯一读取 GameObject/当前卡槽/正式 `CanPlantAt` 的边界，快照记录当前敌方僵尸的真实行、X、移速、本体/头盔/护盾生命与攻击力，以及玩家实际已选卡、冷却、费用和 gamedata 植物画像；已有交互不能只存 `isEating` 等布尔量，必须用实体 ID 带入当前啃食目标等精确关系，否则简化几何会把正式状态误判成尚未接触。推演器不得按僵尸类型写 `switch`，只用能力维度固定预算推进；需要让时域外的未完成交互参与决策时，可在终点用剩余生命、合并后的交互速率和有界权重结算一次廉价终局分，而不是延长完整逐步模拟。候选与无攻击基线共用 rollout seed，并使用局部 RNG，禁止消费 `GameRandom`；诊断状态应单独导出终局分差，便于确认决策来自协同而非噪声。总开关放 `GameAPP` 且旧档默认开启，关闭或推演失败必须保留原确定性/贪心策略；AutoTest 同时锁定开启时的 rollout/候选/僵尸/卡槽数量、关闭时确实走回退，并用一例“协同改变目标”和一例“更高即时收益仍胜出”约束终局权重。
- **寒冰免疫**：`CanBeChilled()` 是减速状态、蓝色覆色和减速音效的共同前置总闸；寒冰子弹必须先检查它，再调用 `SetCooldown` 或播放 `SOUND_COOLDOWNZOMBIE`。只在免疫僵尸里把 `SetCooldown` 写成 no-op 不够，调用方若提前播音仍会产生“没减速却有减速声”的假反馈。AutoTest 同时断言状态未变化和音效请求计数未增加，并用普通僵尸对照证明计数抓手有效。
- **车辆/精英变体的特殊受击**：让车辆基类拥有虚事件（如 `HandleCaltropHit`），植物只命中并派发；普通车辆在默认实现里处理植物消耗、音画和死亡，精英车辆覆写生存规则，避免植物侧 `dynamic_cast` 到每个精英类型。若特殊死亡播放的是 wheelie/bounce 等非通用 `anim_death` 轨道，不要设置会被基类死亡轨看门狗消费的通用 dying 标志；用独立状态+计时器守卫 `ZombieMove`、碾压和继续承伤，存档保存状态/剩余时间，Load 和 `ZombieItemUpdate` 重新应用停驶、碰撞禁用及材质终态。
- **特定植物攻击抗性**：由 `Zombie` 基类提供语义窄的虚修正点，特殊品种按当前防具/状态覆写；子弹只向目标查询，禁止在子弹侧堆类型表或 `dynamic_cast`。若攻击先按倍速累计“每帧总基础伤害”、再拆成多个 `TakeDamage(1)`，目标修正必须发生在累计之前，普通 `AdjustIncomingDamage` 单击上限无法把整帧压低。防具限定抗性在防具掉落后必须返回输入原值，专项同时锁定有/无防具两态。
- **可被植物剥离的装备由僵尸拥有契约**：在 `Zombie` 提供成对的资格查询与原子剥离虚接口，返回当前损伤阶段贴图、装备轨道世界起点和目标偏移，再由品种切到正式无装备 phase/显隐/速度/循环声终态；植物侧禁止识别具体僵尸类型。剥离不得调用会喷掉落粒子或触发范围伤害的派生 `HelmDrop/ShieldDrop`，应只复用无副作用的基类层清理并显式重建品种终态。精英或核心能力变体用资格覆写免疫，不在植物侧加黑名单。C# `MagnetItem.mDestOffset` 是未缩放贴图左上角；若植物侧保存的是绘制中心，必须用每张贴图的缩放后半宽/半高换算，不能让所有装备共用固定补偿。AutoTest 覆盖普通/免疫对照、损伤贴图、装备显隐、能力取消、循环声、离体物与双方存档往返。
- **车辆的场地范围与攻击范围必须分离**：多行铺路、速度场或其他地面覆盖由独立入口决定，植物碾压只通过 `CanCrushRow` 等攻击入口决定；禁止从铺路范围推导碾压范围。AutoTest 在同一列的本行和相邻行同时放植物，分别断言压扁状态，再独立断言各行场地状态，防止调整一项时误改另一项。
- **手动位移车辆的速度合成**：车辆若不用基类走路位移，仍必须让动画与真实 X 位移消费同一组能力、寒冰、雨势、台风倍率；禁止只改 `Animator` 速度。场地增益必须逐因子变换再相乘，不能放大最终乘积；当前鎏金冰道契约为每层把加速倍率乘二（`1.4→2.8`）、减速倍率除二（`0.6→0.3`），中性 `1.0` 不变。重叠场地按仍存活且实际覆盖目标的独立来源逐层变换；进入、离开、来源死亡和范围缩短都要刷新，不能只缓存一个布尔值。若品种能力有最终上限，先保留原始能力因子供统一变换，再在品种覆写的有效能力入口钳位，避免为逆变换叠层而产生无法表达的边界。
- **稀有品种来源的热路径查询**：若每个目标每帧只需查询少量特殊僵尸来源，不要借通用行索引扫描整行后对每个候选 `dynamic_cast`。在 `EntityManager` 的所有新增/恢复入口维护按实体 ID 的品种专用弱索引，每帧首查生成强引用快照并在 `CleanupExpired` 释放；快照只缩小候选集，回调仍须复核同帧死亡/失活、当前行与精确几何覆盖。移动区间或多独立来源的最终层数仍应即时计算，不能缓存成布尔值或静态计数。
- **天气条件变异**：同时使用 `adding-rain-weather`。`spawnlists.json` 与波次点数预算只放基础类型，正式生成路径在扣点前经唯一 resolver 决定实际变异类型；`CreateZombie`/`spawn_zombie` 保持直造确定性。每波上限只在推进新波时清零，不能被天气切换重置；已生成数必须入存档，`weight: 0` 的变异体不得独立进入随机池。
- **地形限定刷新必须放正式选行入口**：只允许陆地或水路刷新的品种统一登记到 `Board::IsSpawnRowCompatible` / `CanZombieTypeSpawnInPool`，让 `SelectSpawnRow` 在加权前把非法行权重清零；禁止在僵尸构造或出生后再换行。`CreateZombie` / AutoTest `spawn_zombie` 继续保留直造确定性，正式波次测试用 `assert_zombie_spawn_row` 在泳池陆路与水路逐项锁定普通/精英家族的兼容性。
- **连续坡面由 Board 统一拥有，品种移动只提交水平语义**：屋顶出生、预览、正式行走、阵风横移和读档恢复都以保存的 `row + x` 经 `Board::GetZombieSpawnY`/基类坡面同步重建 Y；气球、矿工等品种不得复制坡度或各自累计 Y。需要地面裁剪、影子或附件落点时查询当前 X 的行地面线。不要给通用 `Transform` 自动套坡面，否则飞行物、UI、独立动画与其他地图都会被误改；专项同时覆盖坡段/平台、预览、读档和至少一个非普通移动品种。
- **击杀掉落资源先在正式出生时预分配并按波封顶**：死亡、灰烬群杀和其他 `Die()` 路径只领取出生时锁定的奖励，禁止按本次死亡数量临时 roll，否则范围灰烬会把经济成倍放大。若设计要求关内前宽后紧，用当前波/总波数建立独立归一化曲线，同时收紧携带概率、单团价值和单波预算；跨波累计器只补偿未命中，预算命中硬闸后不再累加。AutoTest 用正式 `SummonNextWave()` 和至少两种总波数断言首尾参数及最终波实际上限。
- **小推车特殊交互**：若能力只应吞掉“其他行”小推车，当前碰撞车必须先走原版 `Trigger()`，Board 副作用按当前 mower ID 排除它，最后仍对碰撞僵尸结算 `INT32_MAX`；不要在全场清理分支提前 return，否则精英会绕过本行最后防线直接进家。AutoTest 把吞车验证放在新场景，前序编队/动画长等待可能让移动僵尸提前撞到别行 mower，造成断言对象漂移。
- **出土/升起**：垂直位移用 `mVisualOffset.y`（存基准值，按计时线性还原）；地面遮挡用现成 `SetClipRect(0,0,SCENE_WIDTH, groundY+margin)`，**底边取 `Board::GetZombieSpawnY(row, currentX)` 当前 X 的行地面线**（换地图与连续坡面自适应），完成后 `ClearClipRect`；升起期不移动不啃食（覆写 StartEat 早退）。出土未完成前同时隐藏 `ShadowComponent` 和 reanim 自带 `_ground` 轨道，完整站起后才恢复组件影子；`ZombieItemUpdate()`/Load 必须重建两层显隐，防 Animator 恢复后 `_ground` 黑影提前出现。AutoTest 导出两者的可见状态并在出土中段、完成节点和读档后分别断言。默认让动画继续播放；若主人明确要求静态出土，**只能 `Animator::Pause()` 播放头，不得把 base/extra 速度层写成 0**——后续 `PlayTrack(anim_death)` 会自动恢复 playing，RISING 读档在 `RestoreAnimState` 后须重新 Pause，并必须专项实测升起中死亡不会卡帧。

## 存读档心智清单

普通 AutoTest 仍会短路玩家 `saves/`，但可用 `save_level_snapshot` → 主动改局面 →
`reload_level_snapshot` 在脚本输出目录内验证“正式序列化 → 销毁旧 `GameScene` →
新场景正式反序列化”。这能覆盖实体与 Animator 的进程内往返；中央存档路径、跨进程
退出重进和迁移行为仍需按任务风险另行验证。禁止临时关闭 `GameAPP::mAutoTestMode`
绕过保护。

- 状态机枚举/计时器/关联 ID → `SaveExtraData/LoadExtraData`；Load 首行 `if (mIsEating) return;` 再动动画。
- 新字段能用中性默认值表示旧档时保持兼容；结构或语义变化无法只靠默认值表达时，提升 `SaveSchema::kCurrentLevelVersion`，增加连续迁移和 `SaveSchemaTests`。JSON 必须先升级成功，再恢复 `Board` 与实体。
- **SetupZombie 先于 LoadExtraData 跑**：Setup 里的出生预设（下沉、裁剪、初相位）在 Load 里按存档相位**显式撤销**，否则读档僵尸带着出生态复活。
- 关联僵尸 ID 经 `CreateZombieWithID` 保值可交叉引用；引用已死自然返回 null，无需清理回调。
- 轨道/帧位由 `RestoreAnimState` 统一恢复不用自己存；节拍驱动的僵尸在 Load 里把"上次段位缓存"置 -1 重新入拍即可。

## 验证（缺一不可）

1. 默认按仓库契约配置并构建 `clang-release`，保持 0 warning；新 .cpp 未被编译先 `cmake --preset clang-release` reconfigure。只有主人要求快速迭代/PDB/无 LTO，或 Release 崩溃确需符号栈时才用 `clang-playtest`；诊断完成后仍须回到 `clang-release` 做最终验证。
2. **AutoTest 冒烟**：`autotest/scripts/smoke_<name>.json`。默认按 `PROJECT_GUIDE.md` 的“当前桌面可见启动”方案运行：从 `build/<preset>/` 工作目录，用提升权限的 `Start-Process -WindowStyle Normal -PassThru` 启动并等待退出；普通沙箱 shell 即使写了 `WindowStyle Normal` 也可能落在隔离会话，主人桌面完全看不到。首次直造前断言 `HasReanimation`，运行时帽子/残肢/粒子贴图用 `GetTexture(key,false)` 导出加载状态；Release WARN 不保证写进 `run.log`，manifest 也不能替代这些断言。状态断言用 `zombies.N.type/hasArm/armVisible/hasHead/track/mindControlled`；几何断言用 `animatedObjectsByTag.Zombie.N` 的最终世界包围盒及相对 collider 投影，禁止把 C# 绝对坐标写成期望值。**exit 0 ≠ 通过**：逐张 Read 同步截图（断肢前后、编队站位、出土中段——换色变体必须截取真正使用 `rise*` 合成图的中段；注意升起初期整体在地面线下被裁掉是正确的，截图要卡升起 60% 时点）。
3. **死亡消失必须专门测**（末-1 帧陷阱专项）：豌豆打死→dump 确认该 type 消失+run.log 无 WATCHDOG。炸弹类走 Die() 直杀路径，**测不到**死亡帧事件。
4. 时序：`wait_seconds` 是游戏秒；关卡 20 秒起第一波普通僵尸会混入 dump，别断言"场上为空"。
5. 站位/影子不对 → 本体调 gamedata offset（免编译）、影子调代码 `ShadowComponent`。
6. 父类测试钩子或状态投影使用 `dynamic_cast` 时会同时命中派生精英；先判断具体派生类，或以 `mZombieType` 排除变体，避免普通品种计数和命令误操作精英。
7. Release Fatal Error / Access Violation 先保留崩溃报告与最小脚本；需要堆栈时用同脚本在 `clang-playtest` 复现，优先检查资源注册/键和首个空对象来源。修复后重跑 Release 的新类型脚本和父类回归，禁止只交付 playtest 结果。

## 完工交付：调参量清单交主人（必做环节，主人指定保留）

数值常量集中在 .cpp 顶部匿名 namespace（`constexpr float kXxx`，注释写原版出处）。完工汇报时**列全部可调量的表**：量名｜位置｜现值，跨文件须同改的（如编队两侧的动画速度/段速）显式标注"**须同改**"，gamedata 侧（offset/scale/weight）与代码侧分开列。主人会自己改数值，表就是他的操作面板。

## 流程

复杂僵尸（状态机/召唤/新机制）走完整 brainstorm（关键决策逐项问主人：帧号、断肢方案、召唤细节、魅惑交互）→spec→writing-plans；换皮/纯防具类可简短 spec 直实现。模板：`docs/superpowers/specs+plans/2026-07-10-dancer-zombie*.md`。完成且验证通过后由 Codex 提交；是否 push 服从当前 `AGENTS.md` 与主人本次指令，不在 skill 内写死。

**每次完成并验证任何僵尸新增或实质修改后，必须在提交前完善本 skill**：把本次实际暴露的新坐标换算、生命周期契约、foot-gun 或验证手法浓缩进现有章节；已有规则则合并强化，不堆一次性日志。任务同时修改粒子、植物或天气时，也同步完善本次实际使用的对应 skill。更新后运行 skill-creator 的 `quick_validate.py` 校验全部改动过的 skill，不再等待主人确认查收才复盘。

## 关联记忆

`[[project_pvz_dancer_zombie]]`（本 skill 起源+全部 foot-gun 现场）、`[[project_pvz_pink_football_zombie]]`（同构父类初始化复用+可见桌面测试）、`[[project_pvz_zombie_eat_walk_state_machine]]`（走路权威/啃食钩子）、`[[project_pvz_charmed_zombie_feature]]`（魅惑契约）、`[[project_pvz_gamedata_json]]`（权威单份资源）。
