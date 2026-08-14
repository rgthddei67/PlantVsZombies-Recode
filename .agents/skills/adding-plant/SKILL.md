---
name: adding-plant
description: Use when adding ANY new plant (新增植物) to PvZ — 射手/生产/即时消耗/全场结算，包括效果侵入僵尸或其他系统的（冻结/减速/魅惑/穿透/新子弹/格子占用）— proven on ScaredyShroom (胆小菇), IceShroom (寒冰菇), DoomShroom (毁灭菇+弹坑), Starfruit (杨桃+五向星弹).
---

# 给 PvZ 新增植物

**最高原则：不确定的事立刻问主人，不要自行推断。** 已知必问项：①**帧事件的帧号**（主人会看动画预览，他有准确答案；从 reanim 活跃区间推断会错——胆小菇实测 25，区间末段推法给 28）②数值/行为想偏离原版时。**帧号口径（主人 2026-07-14 定死）：`AddFrameEvent` 真实帧号 = 动画预览帧号 − 1；主人报给你的帧号默认已经 −1 过，代码直接用、不许再减**。只有自己从预览工具读数时才需要手动 −1。

## 原版参考边界

- C# 参考实现是玩家可感知功能的证据：先提取状态、触发顺序、时长、数值、目标规则、音效和资源表现；未获主人批准时，这些行为必须与原版一致。
- C# 不是本项目的架构模板。动手前逐项核对当前植物类型体系、Board/实体所有权、更新与 Animator 时序、占格/碰撞、绘制路径、资源键以及存读档入口，再接入现有最窄扩展点；禁止为了贴近 C# 类结构复制平行状态或旁路系统。
- 坐标和资源按下述当前项目契约换算。工程实现可以不同，但必须用 AutoTest 状态、音效请求、默认与 `-NoInstance` 截图证明功能等价；验证失败时先修适配，不能用“原版就是这样写的”合理化当前项目中的错误表现。

## 坐标换算铁律

**C# 原版逻辑场景是 800×600，本项目是 `SCENE_WIDTH=1100`、`SCENE_HEIGHT=600`。原版任何绝对 X/Y、范围端点、绘制偏移、碰撞框和粒子触发点都只能当语义参考，禁止直接抄入代码。**

- 场景范围由 `SCENE_WIDTH/SCENE_HEIGHT` 和 Board 当前背景几何重算；格子范围由 `GetCellCenterPosition`、`GetCellHeight` 与 `CELL_COLLIDER_SIZE_X` 派生。
- 屋顶植物只使用 `GetCellCenterPosition(row, col)` 的离散坡面格中心；不要复用僵尸的连续 `GetRowCenterYAtX`。花盆占 `under`、上层植物占 `normal`、南瓜占独立壳层，咖啡豆这类原版 flying 叠种体可占短时 `overlay`；普通植物/南瓜必须有花盆，地刺系仍拒绝屋顶。新局在选卡前且确认未进入读档生命周期后，按大关内编号生成 5-1 五列、5-2 四列、后续三列的初始花盆；C# 原关卡号分段与本项目九关制不同，必须按显示关卡语义映射，并保留外层列、内层行的创建顺序。
- 植物局部点位先换算到本项目以格子中心为 `GetPosition()` 的口径，再叠加当前 gamedata 视觉偏移；逻辑格位置与 `mVisualOffset` 永远分开。
- 发射点、范围边界和附加 Animator 基点优先表达成“相对稳定视觉原点/父轨基准姿态”的差值，不把 C# 的世界坐标塞进局部偏移。
- AutoTest 先执行同步 `screenshot`，再用 `animatedObjectsByTag.Plant` 的 `renderProbeReady/worldBounds/visualToRenderCenterD*Int/nearestPlant` 验证本项目最终绘制几何相对格子与植物 collider 的关系；每阶段只保留一株目标植物以稳定数组索引。
- 修改 gamedata offset、附件、整株变换或 `SetRenderScale` 时，默认实例化与 `-NoInstance` 各跑同一静止用例并比较整数 `worldBounds`；截图负责肉眼基线，运动对象瞬时绝对 X/Y 只供诊断、不作稳定断言。
- 战场主体按 `row N 植物 → row N 僵尸/扶梯 → row N+1 植物` 交错绘制；同排僵尸仍在植物之上，下一行植物遮挡上一行越界身体。植物运行期换行/搬格若改变 `mRow`，必须同步调用 `GameObjectManager` 的排序键刷新入口；小推车与子弹层不得顺带改动。专项同时断言语义 `renderLayer` 未变、实际 `renderOrder` 行带正确，并以默认/`-NoInstance` 屋顶跨行截图验收。

## 第 0 步：勘察（动手前全部做完）

1. **读 reanim**：`build/clang-release/resources/reanim/<Name>.reanim`，用 Grep `<name>` 提取全部 track 名，`anim_xxx` 即可用动画（具体可以询问主人，有些anim_xxx并不是可用动画，而是一个track）；`<f>-1/0</f>` 对定位 anim 轨活跃帧区间。
2. **读 C# 参考并主动盘点音效**：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Plant\Plant.cs`，grep 植物名，读专属 Update 函数 + 发射物类型 + mShootingCounter/state 分支，先记录必须忠实的行为与数值，再按本项目现有所有权、坐标、更新、绘制和存档契约实现；同时收集相关路径的全部 `PlayFoley` / `PlaySample`，不要等主人听出缺声才补。受啃、受击等由外部对象触发的反馈还必须搜索消费方（例如 `Zombie::AnimateChewSound` 会按植物类型选择 `ChompSoft`），不能只读 `Plant.cs`。沿 `FoleyType → Sexy.TodLib/Foley/TodFoley.cs → Resources.SOUND_*` 得到精确资源键，以资源键去掉 `SOUND_` 后的小写名到 `D:\PVZ\中文年度加强版完整版\Test\sounds\` 查同名 `.ogg`。找到后复制到唯一权威 `build/clang-release/resources/sounds/` 合理子目录，并同步 `resources.xml` 与 `ResourceKeys.h`；找不到才问主人，禁止用相近声音静默替代。构建后检查 `manifest.txt` 和启动日志无 missing sound，并用可见行为路径及 `GetSoundPlayRequestCount` 投影验证触发次数（含读档不得重响）。
3. **盘点已就位的基建**（常常提前有了，别重复加）：`PlantType.h` 枚举、`TestDriver.cpp` kPlantNames、`ResourceKeys.h` RKEY、`AnimationTypes.h`、卡片图 `PlantImage/<Name>.png`、reanim 部件图。缺哪补哪。
   如果植物枚举、冒险解锁位或 AutoTest 名称表已经预置但尚未注册，保留现有位置与整数 ID，只补缺失接线；动画枚举仍追加在末尾，禁止为追求排列整齐移动旧值或再加重复项。
   `image/reanim/` 全目录预加载生成 `IMAGE_<文件名大写>`；只有被 reanim XML 的 `<i>` 直接引用的部件才会额外获得 `IMAGE_REANIM_*` 别名。运行时动态换入、但不在 XML 时间线出现的受伤材质必须用前者。更新派生阶段时先确认 `GetTexture` 非空，再提交阶段缓存，避免“状态断言通过、画面仍是旧图”的假绿。
   派生换色必须逐个核对目标 reanim 的实际 `<i>` 资源键，不能从 track 名或文件名猜部件归属：名字像 `backleaf` 的轨道可能属于地面叶座，头后小叶反而可能引用共享 `ANIM_SPROUT`。只给真正需要变色的共享部件派生独立纹理并替换新 reanim 的键，原植物仍保留共享资源；AutoTest 为该独立键增加 `GetTexture(key,false)` 断言并截图。

## 实现清单

1. **类**：`Game/Plant/<Name>.h/.cpp`。选基类：蘑菇→`Shroom`（白天睡觉自动处理）；豌豆系带独立头部动画→`Shooter`；其余→`Plant`。抄最像的现有植物结构（喷射蘑菇抄 `PuffShroom`）。
2. **注册**：`GameDataManager.cpp` 加 `#include` + `RegisterPlant(type, "PLANT_X", IMAGE_X, ANIM_X, "ReanimName", &MakePlant<T>)`。卡片由注册表数据驱动，**无需单独加卡**。
3. **gamedata.json**：在 `build/clang-release/resources/gamedata.json` 加 `{cost, cooldown, offset, scale, simulation}` 条目（前四项缺任一字段拒启动 exit -6）；其他 preset 自动共享。`simulation` 是 Board 级轻量防线推演画像：所有植物填写 `baseHealth`；普通射手按稳定等效值填写 `attackDps`，跨行攻击另填 `attackRowRadius`；向日葵/阳光菇类填写 `sunPerSecond` 与 `firstSunDelay`；一次性或无法可靠简化的复杂能力明确填 `persistent:false`，不得为了让它参与推演而在 `Board` 写植物类型特判。普通花盆/睡莲这类只承载、没有额外推演能力的 under 层填写 `supportOnly:true`，由 Board 压缩进独立的每格支撑数组并从未来种植卡画像排除；避雷花盆等有特殊战略能力的支撑植物保持 false，继续占详细植物画像。新增植物必须在专项中断言对应 `plantDefinitions.<TYPE>.simulation*` 投影（含适用时的 `simulationSupportOnly`），防止画像漏配或未加载。
4. **info.txt（图鉴文案）**：在 `build/clang-release/resources/info.txt` 加两段——`[PLANT_X]` 下一行图鉴名字、`[PLANT_X_DESCRIPTION]` 下一行介绍（enum 名与注册的 "PLANT_X" 严格一致；解析器只认 `[key]`+正文，多行正文允许）。缺条目图鉴显示空白不报错，极易漏。
5. **资源入库**：reanim、贴图、声音、resources.xml 都只改 `build/clang-release/resources/` 这一份权威资源；严禁为 playtest/debug 建副本或 Copy-Item 同步。新增文件仍因 build/ 被忽略而需要 `git add -f`。
6. **射击类惯例**：帧事件发弹（帧号问主人）+ `mShootTimer` 乘词条攻速 `GetAttackSpeedMultiplier()` + 索敌用 `ForEachZombieInRow`（严禁全表扫）。跨行/范围判定注意坐标换算：**C# 的 mX/mY 是格子左上角，本项目 `GetPosition()` 是 80×100 格子中心**，差 (40,50)。若攻击开始时先随机决定手持弹型，待发类型必须成为该轮权威状态：起手同步手持轨显隐，发射帧消费后恢复默认弹型；攻击计时与待发类型一起入档，`LoadExtraData` 只重建显隐、不重抽、不播声。专项在发射帧前后断言两条轨道互斥，并在手持特殊弹时做快照往返。双向弹优先复用同一 BulletType 并用带符号的基础 X 速度表达方向，让碰撞、火炬、台风和对象池继续走通用链；速度符号还必须传播到溅射区间、命中特效局部 X 偏移、贴图翻转和二类护盾命中面，不能只把本体速度改负。所有 `Bullet` 对僵尸的直接伤害统一走 `Zombie::TakeProjectileDamage`：它以命中时 `velocityX` 与目标 `IsMovingRight()` 判断正面/背面，同向追上即绕过铁门/报纸/梯子直接攻击后层，禁止按 `BulletType` 或 `Backwards/Homing` 建方向白名单；追踪弹必须持续更新真实速度。弹丸能力若主动无视二类护盾，必须通过该入口的显式请求参数表达，并让目标用 `BlocksProjectileShieldBypass` 虚接口否决不可绕过的防具；禁止在子弹侧维护僵尸类型表，也不要误用 `penetrateShield`——后者会同时伤盾与后层。速度为 0 的测试弹保持历史正向口径；AutoTest 分别断言正反向伤害、主动绕盾、目标侧否决、护盾/本体生命、`hitFlashMask` 和命中特效相对目标的最终几何，不依赖 `bullets.N` 顺序。
7. **动画状态机**：一次性→循环用 `PlayTrackOnce(track, returnTrack, speed, blendTime, returnSpeed, returnTrackBlendTime)` 自动接轨，完成信号 = `GetCurrentTrackName()` 变成 returnTrack（天然兼容存读档，勿自造 loop 计数）。`blendTime` 只管进入一次性轨，`returnTrackBlendTime` 独立控制返回；原版若用 `SetFramesForLayer` 在重合/相邻包装轨边界硬切，最后一个参数显式传 `0.0f`，否则历史默认 0.5 秒可能插值出资源中不存在的部件姿态。包装循环轨的首帧必须与 body/face 等部件真正切换到该状态贴图的帧一致：若包装轨提前一帧开始，`PLAY_REPEAT` 每次回绕都会闪出上一姿态；逐帧核对 `<f>` 标记与 `<i>` 切图，并让专项在初始及跨多轮循环后都断言 Animator 不会进入切图前一帧。状态贴图内的细线发光元素还必须在最终战场缩放和实际昼夜/天气叠色下验色，不能只看高分辨率源图；描边占比过高会在缩小后吞掉内芯颜色，应让最终截图仍保留明确色相。返回速度与返回混合都属于待执行状态，主 Animator、Shooter 头和其他自管 Animator 必须一起保存；额外 Shooter 头统一调用 `Shooter::SaveHeadAnimatorState/LoadHeadAnimatorState`，禁止只存轨道/帧或复制一套私有序列化；旧档缺返回混合字段时默认 0.5 秒。
8. **帧事件是全局帧号、跨轨道通用**（`Animator::mFrameEvents` 只按 int 帧号，不分轨道）：定下触发帧后必须核对**其他 anim 轨的活跃窗口扫不到它**（毁灭菇 51=explode(19..51) 末帧，sleep(52..76)/idle(0..19) 都够不着才安全；末帧触发安全——普通前进与循环回绕分支都覆盖）。原版动画速率≠reanim 基础 fps 时，用 `PlayTrack(track, 原版fps/reanim fps)` 折算（毁灭菇 23/12≈1.92）。
9. **整株世界变换必须覆盖复合 Animator 的两条 A/B 路径**：默认路径会把根 Animator 与任意深度附件按轨道顺序递归写入 GPU `InstanceRecord`，`-NoInstance` 才整棵走矩阵慢路径；外层 `Graphics` 变换栈不会覆盖默认实例路径。应在 `Animator` 最终矩阵/`InstanceRecord` 两处统一实现世界变换，并递归同步现有与以后附加的子 Animator；AutoTest 同时断言根/子变换，默认与 `-NoInstance` 都要逐张检查截图。
10. **C# 复合头附件要补 `inverse(basePose)`**：C# `AttachToAnotherReanimation` 的附件矩阵是父轨道当前姿态乘基准姿态逆矩阵，而本项目 `AttachAnimator` 目前只直接乘父轨道当前姿态。子 reanim 仍使用整株绝对坐标时，必须从根返回/待机轨首帧读取每条附件轨各自的基准姿态并在子 Animator 局部变换中抵消；基准旋转/缩放为单位时就是 `SetLocalPosition(-baseX, -baseY)`。不能给多个头套同一个 `gamedata` 偏移，否则某个头会与茎错位；默认和射击轨都要可见截图校对。
11. **不可啃食植物覆写 `CanBeEaten()`，不要在僵尸索敌处堆类型表**：植物自己声明契约，普通啃食路径会统一跳过；具有临时离地状态的植物只在 C# `NotOnGround()` 对应阶段返回 false，不能把仍在地面的观察/预备阶段一并排除。组合格里无效上层不应遮挡可啃支撑层：专项要让离地植物与花盆/睡莲叠放，逐帧断言僵尸持续啃支撑层且动画前进、X 不发生基类行走位移，防止每帧停吃再开吃把动画重置到同一帧。
12. **只有完整时间轴、没有 `anim_*` 包装轨的循环 reanim**（如 `FirePea.reanim`）用 `SetFrameRangeToDefault()` + `Play(PLAY_REPEAT)`，不要捏造轨道名或帧事件。非等比 `SetRenderScale` 的 pivot 是**世界坐标**；命中特效应传自身绘制基点，传 `(0,0)` 会把整个特效按比例拉向屏幕左上角。
13. **跳跃阻拦植物只声明能力和反馈，不决定僵尸动画时序**：`BlocksZombieJump`/`OnZombieJumpBlocked` 由跳跃者在原版动画进度节点调用，接触植物时不得提前 Bonk、喷粒子或扣血。组合植物的起跳与啃食目标仍按战斗顶层解析，但**阻拦查询必须独立按格内层级逐层询问能力**；南瓜等不会阻拦的外壳不得遮蔽内层高坚果，睡莲/花盆碰撞也不得让上层阻拦体漏判。特殊僵尸若撞伤阻拦植物，应把实际阻拦者引用传入品种钩子并走带正确 `DamageSource` 的正式承伤链；先确认规格中的受伤者，不能把“植物损失 N 血”误实现成僵尸自身扣血。
14. **台风锚定植物也只声明格位能力和直接撞击反馈**：用类似 `AnchorsPlantCellAgainstTyphoon` / `OnTyphoonPlantImpact` 的虚接口让天气唯一结算点派发，禁止在 `Board` 堆植物类型表。逐格位移中先让锚定源格保持不动，再只对直接进入锚定目标格的植物组合结算；后方被普通占格挡住时不传导压力。伤害必须逐格立即生效，使锚定植物中途死亡后剩余步数能重读格位；同阵风重复撞击可按锚定植物 ID 合并音画反馈，但不能合并伤害。组合植物按一个移动格计数，专项覆盖双向、紧邻多步、间隔移动、连续链、水路上下层和中途死亡放行。
15. **直接落水植物必须同时声明地形、层级与水面表现**：在 `Board::CanPlantAt` 的水生集中分支要求“水格且 `Cell::IsEmpty()`”，从而允许空水直种并同时拒绝陆地、已有睡莲和其他植物；除睡莲继续占 `under` 外，水草/海蘑菇这类能力植物占 `normal`，不要为了直种改 `CreatePlant` 的通用层级。原版不画陆地影子的品种移除 `ShadowComponent`；发射点和附属视觉从 `GetVisualAnchorPosition()` 派生以跟随水面浮动。专项用 `assert_can_plant` 覆盖空水 true、陆地 false、睡莲水格 false，再断言 `cells.*.normal/under`、无阴影和同步截图。
16. **套壳或其他新增占格层必须闭合 Cell 生命周期与绘制夹层**：新增槽位时同步检查 `CanPlantAt`、`CreatePlant/CreatePlantWithID`、`GetTopPlantAt`、`ReleaseGridSlot/CleanPlantFromCells`、铲子/啃食、render order、存读档重建、台风整组搬运和外部范围伤害，AutoTest 导出每层类型及生命与 top，分别覆盖正反种植顺序、移除外层和水路多层。先明确新层是否参与 top：咖啡豆的 flying `overlay` 需要画在普通层上方，却故意不参与啃食/铲除 top，并通过无 collider、`CanBeEaten=false` 与承伤覆写排除地面攻击。**铲子选择与战斗 top 必须分开建模**：僵尸可继续按外壳优先，铲子则按可见区域在壳与内层间选择；命中环、中心或边界从当前 Cell 宽高/稳定视觉原点派生，悬停高亮与最终铲除共用同一目标，并用真实 `click` 分别断言只移除内层、只移除外壳。若范围技能规定外壳或邻格保护，先沿原几何找出实际命中层，再通过唯一 Board 解析入口按逻辑格选择保护者；范围重叠须使用稳定最近者/并列规则，命中的外壳只选自身以阻断保护链，最后按保护者实体 ID 归并为每次攻击一次承伤。没有保护者的命中层保持技能原行为，不能直接全表逐株扣血，也不能只用爆点所在格的 `GetTopPlantAt` 漏掉范围边缘；生产结算、选点评分和纯数值推演必须消费同一保护范围、选择与归并语义，普通版本若明确豁免则不得误接。专项覆盖陆地、水路 under+normal+壳、多命中归并、重叠保护、倍率、无保护与明确豁免攻击。若美术要求“后片→内层植物→前片”，不要在 `Draw` 中临时切换共享 Animator 轨道可见性；默认实例化可能并行提交。由套壳持有同步帧/alpha/scale 的独立后片 Animator，根 Animator 只画前片，并在默认与 `-NoInstance` 下逐张核对同一截图。开启植物血量显示时，同格各层会分别绘制数字；让外壳覆写血量文字偏移并错开至少一行，再用同屏截图确认不会覆盖内层数字或主体。
17. **唤醒蘑菇要把倒计时、睡眠指示和品种激活分开**：目标蘑菇基类持有 sleeping/wake timer、原版 `EaseSinWave` 纵向弹性与 `SOUND_WAKEUP` 边界，咖啡豆只负责等待、碎裂并对同格普通层调用单一 `BeginWakeUp`。品种通过 `OnWakeUp` 恢复自身能力轨；毁灭菇等特殊蘑菇必须进入与夜间种下相同的充能入口，不能统一硬切 `anim_idle`。睡眠 `Z` 是由 sleeping 派生的独立 Animator：睡着时懒创建，醒来或压扁时移除，植物失活后随宿主销毁；`RestoreSleepState` 只静默重建，不保存指针或随机相位。`Z.reanim` 没有 `anim_*` 包装轨时使用完整默认时间线循环，并按原版 6～8fps 与随机起相播放；默认更新和并行更新都要推进，常规植物与蹦极携带绘制都在本体之后提交。位置从 `GetVisualAnchorPosition()`、当前搬运视觉偏移和按品种映射后的局部偏移派生，禁止照抄 C# 左上角世界坐标。咖啡豆阶段/等待计时与目标 wake timer 都入档；加载使用无音效、无能力重触发的恢复入口，再由统一 Animator 恢复当前轨，避免重播咖啡声或充能声。专项覆盖 `Z` reanim/贴图资源闭环、睡眠显示与相对锚点、等待和碎裂中快照、醒后移除、音效请求不增加、普通蘑菇恢复 idle、特殊蘑菇正式结算，以及默认/`-NoInstance` 同步截图。
18. **升级紫卡必须集中声明基础株并原子替换普通层**：用 `PlantUpgradeRules` 的单一映射同时驱动 `Board::CanPlantAt/CreatePlant`、卡槽可用性和紫卡绘制，禁止各处复制类型表。正式种植只接受同格活动、存活、未压扁且未被蹦极锁定的目标基础株；先保存睡眠/唤醒剩余时间并把 Cell 普通层切到新 ID，再让旧株 `Die()`，使其 `ReleaseGridSlot` 不会清掉新株，同时保留 `under` 承载层与南瓜层。读档的 `CreatePlantWithID` 继续直接恢复已升级实体，不要求场上先有基础株。无可升级基础株时 `CardSlotManager` 必须把卡置灰；`CardDisplayComponent` 只把紫卡底板作为独立资源区域绘制，植物立绘照常叠加，不能把完整卡槽截图或生成图当卡底。生产型升级优先让基础株持有计时、发光与存档状态，只暴露间隔/单轮数量等品种变体点；升级实体按自身首轮进度初始化，加载再由同一基类状态覆盖。专项同时锁定基础株与升级株的生产参数、发光中途快照只结算一次，以及错误基础株/蹦极拒绝、昼夜与咖啡豆唤醒继承、花盆或睡莲+普通层+南瓜叠层、资源键和普通卡底不受影响。
19. **立即死亡必须先失活再登记延迟销毁**：`GameObjectManager::DestroyGameObject` 到下一次 `Update` 才真正移除对象，而 `StopAnimation()` 会把 Animator 重置到轨道起点。`Plant::Die()` 必须先防重入并 `SetActive(false)`，再停动画、释放格位和入销毁队列，否则死亡当帧仍会闪回起始姿态一次。需要保留残影的压扁态继续走 `Squish()`，不能套用立即死亡契约。
20. **环境完全暂停要同时封住并行事件与串行动画**：只跳过 `PlantUpdate()` 不够，Animator 可能已在 `UpdateParallel` 产生帧事件并由串行阶段消费。离散技能统一调用 `Plant::ApplyShutdown`，重复施加取更长剩余时间并保存 `shutdownTimer`；连续区域场继续由 Board 声明式查询拥有，不能每帧刷新一个短计时冒充。`Plant::IsShutdown` 合并两类来源，植物在并行阶段不推进/不排事件，串行回退也把 `mAdvancedInParallel` 置位让公共更新跳过本帧 Animator，并让行动倍率返回 0。禁止用临时 `Pause/Play` 代替：一次性轨道会被公共自动结束检查误判，原本已暂停的花盆轨也可能被唤醒。专项同时断言计时状态重复施加、连续场选中/未选、活动阶段读档、结束恢复和新放置边界。
21. **短展开防御要让植物声明能力与阶段、威胁负责结算时序**：由植物虚接口回答保护格、启动动作并返回 `INACTIVE/ACTIVATING/REFLECTING`，Board 只按逻辑格和稳定实体 ID 选择唯一保护者，禁止在篮球、蹦极等威胁侧维护植物类型表。需要等待展开的持续碰撞必须同时接 `onTriggerEnter + onTriggerStay`：`ACTIVATING` 不伤目标也不回收威胁，保持重叠宽限；`REFLECTING` 才原子消费威胁。无需等待的落地威胁可在其原版落地节点立即弹回。阶段与剩余时间入档，加载只按已恢复 Animator 修正终态，不重播声音或重触发威胁；专项覆盖保护范围内外、对角格、重叠选择、展开等待、存档无反馈重放和原威胁回归。

## 存读档心智清单

普通 AutoTest 仍会短路玩家 `saves/`，但可用 `save_level_snapshot` → 主动改局面 → `reload_level_snapshot` 在脚本输出目录内验证“正式序列化 → 销毁旧 GameScene → 新场景正式反序列化”。这能覆盖实体与 Animator 的进程内往返；中央存档路径、跨进程退出重进和迁移行为仍需按任务风险另行验证。禁止临时关闭 `GameAPP::mAutoTestMode` 绕过保护。

- 自定义状态（状态机枚举、计时器）→ `SaveExtraData/LoadExtraData`。
- 新字段能用中性默认值表示旧档时保持兼容；结构或语义变化无法只靠默认值表达时，提升 `SaveSchema::kCurrentLevelVersion`，增加连续迁移和 `SaveSchemaTests`。JSON 必须先升级成功，再恢复 `Board` 与实体。
- 动画轨道/PlayTrackOnce 进行态：`GameInfoSaver::RestoreAnimState` 已统一恢复，**不用自己存**；`LoadExtraData` 在其后运行可放心覆盖。
- **由生命值等已保存数据派生的材质/阶段要在 `LoadExtraData` 主动重建，但必须走显式“只恢复终态、不播放反馈”路径**；不要复用会喷粒子、播音效或再次结算阈值奖励的实战更新入口。快照测试同时断言派生材质/阶段恢复且对应反馈计数为 0。
- **凡是被状态机消费的节流缓存，读档第一帧不得吃初值**——缓存初值=给读档后的世界注入捏造状态（胆小菇实测：`mScaredCached=false` 初值让 SCARED 态读档后误判"僵尸走了"先伸头再缩回）。修法=计时器初始即到期，首帧强制真算。

## 验证（缺一不可）

1. 默认用 `clang-release` 完成带 LTO 的 0 warning 构建；只有主人特殊要求快速迭代、PDB 或无 LTO 时才改用 `clang-playtest`，特殊要求 Debug CRT/Debug 语义时才用 `msvc-debug`。
2. **站位+影子截图校对**（写完必做，别等主人指出）：临时脚本把新植物与小喷菇/向日葵种同一行，截图比脚底基线。先从原版实现/画面确认该品种是否有影子；原版不画影子的品种即使是陆生植物也要在 `SetupPlant()` 显式 `RemoveComponent<ShadowComponent>()`，并断言 `hasShadow=false`，不能因落在草地就沿用基类默认影子。需要影子时再校对两套独立坐标：**本体 = gamedata.json 的 offset**（改无需重编译）；**影子 = 代码里 `ShadowComponent::SetOffset`**（改要重编译）。C# `DrawShadow` 的 `num2/num3` 是最终局部落点，不是相对本项目组件默认值的增量：例如花盆为 `Y=51-5=46` 且 1:1，不能误写成 `28-5` 或继续套纵向 0.75 压缩；除状态探针外必须以截图确认肉眼可见。抄同类植物的值大概率不准。

   花盆这类承载植物还要让上层本体与落点预览共同消费一个 5px 视觉抬升常量，逻辑位置/碰撞箱不变；上层存在时暂停花盆 idle，移除或台风换格/读档后按当前 Cell 派生状态恢复。台风必须遍历同格全部活跃层（当前含 `under + normal + pumpkin + overlay`）作为一个组合移动、丢失或受阻，不能只搬顶层。
   植物若有阵风插值、水面浮动等动态位移，收口成**不含品种静态 offset 的公共视觉锚点**：本体=`锚点+gamedata offset`，影子=`锚点+shadow offset`，禁止影子退回裸 `Transform` 而漏掉动态量。专项在同步截图后导出 `ShadowComponent` 最近实际提交的中心，并用**同一绘制帧**的 Animator render base 减 gamedata offset 得到锚点再断言；不要在截图后的下一逻辑帧重算正弦锚点，否则会产生亚像素相位差假失败。最后跨两个动画相位读图确认共同移动。
   屋顶等非水平网格中，阵风换格或其他格间动画必须从源/目标 `Board::GetCellCenterPosition` 插值完整二维视觉锚点，不能只改 X 后继续沿用源格 Y；逻辑 `row/column` 与 `mVisualOffset` 仍分离。格内植物保持离散格中心，横跨多列的火焰/范围视觉则按每个分段 X 单独查询坡面。禁止给通用 `Transform` 自动消费坡面，以免污染飞行物、UI 和其他地图。
   卡槽/选卡卡图由 `CardDisplayComponent::DrawPlantImage` 独立绘制，不能为缩卡图去改 gamedata `scale`（那会改草坪本体）。需要品种特例时在通用卡图倍率上追加独立倍率，并从既有卡图矩形中心缩放，避免向左上角漂移；用实际卡槽截图验收。
   卡牌状态与槽下菜单若用比例字体显示可变短标签（如 `0/I/II/III`），必须用 `Graphics::MeasureTextWidth` 按当前字号和 letterbox 口径真实测宽后在各自布局矩形内居中；禁止按字符数分组手调 X，否则同组内不同字形宽度仍会肉眼错位。
   若卡槽本身承载“下一株”的玩法设置（方向、模式等），权威状态放在该 `CardComponent`，卡面图标/箭头等提示和预览都实时消费同一状态，种植成功时再复制到实体；之后改卡不能追溯已落地实例。卡片和实体分别保存各自状态，旧档给中性默认。右键切换必须先于通用右键取消选择消费，并允许冷却中的卡修改。AutoTest 连续合成点击时，`click` 完成只代表松开事件已入队；移动鼠标或断言选中前至少留一个真实输入处理帧，否则松开会在新坐标结算而丢失点击。
   图鉴/选卡卡片用 `GetIsInChooseCardUI()` 表示非实战上下文，这些场景可以没有 `CardSlotManager`。`CardDisplayComponent` 只有在具体实战卡面确实需要 `Board`（当前为活动路灯花状态）且该标志为 false 时才查询 manager；禁止为通用卡面绘制每帧全表重找，否则植物图鉴会按“每张卡×每帧”刷 `CardSlotManager host invalid`。专项从主菜单走真实点击进入植物图鉴并检查捕获 stdout 中目标警告为 0，再切 GameScene 截图确认活动路灯花卡仍显示挡位与燃料。
   修改卡槽悬停或点击落格时，预览与种植必须共用同一个“世界坐标 → 唯一 `Cell`”解析入口和相同的边界归属规则。`ColliderComponent::ContainsPoint` 的矩形四边都是闭区间，相邻格边缘会同时命中；禁止预览按行列扫描、点击却依赖 `ClickableComponent` 渲染顺序，否则边缘和四格交点会显示一格却种到另一格。
   新增“上次选卡/预设卡组”时只在玩家正式提交后记录，PlayerInfo 保存稳定植物枚举名及点击顺序并随结构变化升级 schema；恢复必须从当前选卡面板已有卡中解析、去重、过滤未知/未注册/未拥有项并遵守槽位上限。按钮若属于 `ChooseCardUI`，位置必须相对面板并随其过场移动；一键恢复复用 `Card::SetTargetPosition` 的现有飞行动画，禁止直接写最终坐标。AutoTest 用真实按钮点击分别断言运动中、终态和下一局再次恢复，默认隔离模式只布置内存状态、不写真实 PlayerInfo。
3. **AutoTest 冒烟**：`autotest/scripts/smoke_<name>.json`，每阶段**只种一棵**（plants dump 顺序来自 unordered_map，多棵时下标不可靠），断言 `plants.0.track`；`plantDefinitions.<TYPE>.sunCost/cooldownMs` 可直接锁定基础 gamedata 数值，`simulationBaseHealth/simulationAttackDpsOn100/simulationAttackRowRadius/simulationSunPerSecondOn100/simulationFirstSunDelayMs/simulationPersistent/simulationSupportOnly` 用来锁定轻量推演画像。几何验收用 `animatedObjectsByTag.Plant.0` 的最终世界包围盒与相对 collider 投影，禁止把 C# 绝对坐标写成期望值。时序估算用僵尸判定矩形 `[x±25]×[y-65,y+35]`、步速 23~45px/s；验证帧事件时，等待值必须越过理论触发时刻至少一个逻辑步，不能把断言卡在“刚好到帧”的浮点边界。**exit 0 ≠ 通过**：必须逐张 Read 同步截图 + dump 数值核对（防假绿）。
4. 蘑菇夜测用 level 10-18（九关制的 2-1..2-9）；白天睡觉同时断言 `anim_sleep`、睡眠 `Z` 的资源/显示/相对锚点，醒后断言移除，并逐张检查同步截图；魅惑僵尸清场用 `charm_zombie`（不触发输局）。
   只能种水路的蘑菇改用夜间泳池 level 28+ 验活跃态，并另在日间泳池 level 19+ 验 `anim_sleep`。

紫卡升级同帧内，旧基础株已失活但可能尚未从实体表移除；所有按格布置状态的 AutoTest 夹具必须过滤 `IsActive()`，断言优先用 `normalPlantsByCell/topPlantsByCell.<row>_<col>`，不能让 `plants.N` 或实体枚举顺序选中旧株。

## 特性侵入其他系统时（寒冰菇冻结、魅惑、穿透这类）

纯植物侧的清单不够用了，先按效果落点归类，逐类有先例可抄：

| 落点 | 先例 | 关键点 |
|---|---|---|
| **僵尸新状态效果**（冻结/减速/魅惑…） | 减速=SnowPea→`Zombie::SetCooldown`；冻结=IceShroom→`StartFrozen`；魅惑=HypnoShroom→`StartMindControlled` | 见下方专属清单 |
| **全场即时结算**（寒冰菇式） | `IceShroom`（帧事件→音效+白闪+逐行结算→`Die()`，仿 CherryBomb 骨架） | 植物先检查 `mBoard->GetPresentation()`，再用 `ShowScreenFlash(...)` 请求全屏效果；由 `GameScene` 实现 `BoardPresentation` 并注册绘制，禁止植物或 `Board` 恢复具体 `GameScene` 依赖；**isPreview 特判否则图鉴也结算**（主人叮嘱） |
| **格子占用系统**（弹坑/墓碑类） | `Crater`（毁灭菇弹坑，阻种 180s） | 轻量 GameObject（Trophy 先例，LAYER_GAME_OBJECT=背景上植物下）+Board weak_ptr 簿记+存档数组（旧档兼容=无字段则空）；**占格判定有两处独立口径都要改**：`CanPlaceInCell`（阻种）+`UpdatePlantPreviewPosition`（落点预览隐藏——漏改=悬停占用格仍显示预览，主人验收抓出）。AutoTest `plant` op 直连 CreatePlant **绕过闸门**：基础规则用 `assert_can_plant` 同时断言占用格 false 与旁格 true；若改了卡槽/悬停 UI，再补 `click` 真实路径与同步截图 |
| **外部附着格对象生命周期**（扶梯等） | `Ladder` → `Plant::Die/Squish` | 对象由 Board 提供唯一格接口与可选存档数组，植物只在正式死亡/压扁入口通知 Board，不持对象指针；悬浮 overlay 等原版豁免必须显式列出，行级清除能力在自身结算点调用 Board 同行接口。专项分别覆盖普通死亡、压扁、豁免层、同行/异行保留与快照往返，避免只测部署者而漏掉被附着植物的生命周期。 |
| **清除同格组合植物** | DoomShroom→`KillOtherPlantsInCell` | 按 EntityManager 的全部植物 ID 快照遍历，以逻辑 `row/column` 过滤并排除施法者，再逐株 `Die()`；不要只写死当前 under/normal 两层，否则以后南瓜等额外层会留在结算后的弹坑里 |
| **引爆倒计时无敌** | CherryBomb / DoomShroom 的 `TakeDamage` 覆写 | 只 `SetGlowingTimer(0.1f)` 不掉血；**有睡觉态的要放行睡觉分支**（白天=普通蘑菇照常被啃，毁灭菇实证） |
| **新子弹/运行时变种** | `PuffBullet`；Torchwood→FirePea | `BulletType` + BulletPool；动画子弹可在 `Bullet` 组合独立 Animator，但命中语义必须按“当前类型”分派，不能依赖分配时的 C++ 子类 |
| **新粒子特效/染色变种** | IceFumeCloud（寒冰大喷菇） | XML 标签全参考在 **adding-particle skill**，勿再读 ParticleSystem 源码 |
| **TakeDamage 类钩子** | FumeShroom 的 `penetrateShield` 参数 | 穿透只对二类护盾（门/报纸），不穿头盔；改签名先看全部调用点 |
| **即时/范围结算** | CherryBomb/大喷菇锥形 | 帧事件触发结算帧（帧号问主人），范围判定用行桶不全扫 |
| **目标类型拥有特殊受击语义** | Caltrop → `ZamboniZombie::HandleCaltropHit` | 植物只负责命中与派发，目标基类拥有虚事件并处理消耗植物/动画/存活；精英变体覆写目标方法，禁止在植物攻击函数里继续堆具体精英类型分支 |
| **能力剥离目标装备** | MagnetShroom → `Zombie::HasMagneticItem/ExtractMagneticItem` | 目标自己声明当前状态是否可剥离，并原子返回当前损伤贴图、轨道世界起点和植物侧落点后进入无装备终态；植物只做范围、优先级、飞行和充能，禁止按僵尸类型 `dynamic_cast`。剥离与受击破甲是两条路径，不得误触发掉落粒子、范围伤害等破甲副作用；高压变体可覆写资格为 false。离体物视觉状态与植物计时入档，目标新 phase 也须走自身存档。若装备返回默认 0 之外的 `extractorSelfDamage`，必须先进入吸取/充能态并接管离体物，再直接扣植物本体；即使反噬致死也不得回滚目标装备。规格要求绕过南瓜和防御词条时不可调用通用 `TakeDamage`。C# `MagnetItem.mDestOffset` 是未缩放贴图左上角，而本项目飞行状态使用绘制中心；终点必须加 `texture.width/height * drawScale / 2` 转换，禁止用统一 X/Y 补偿掩盖锚点口径错误。屋顶关自带花盆时 AutoTest 用 `topPlantsByCell.<row>_<col>` 定位磁力菇，不得误把 `plants.0` 当作新种植物。 |
| **区域天气保护与每次事件反噬** | GroundingShroom → `Plant::CanGroundNightRoofChargeFor/AbsorbGroundedNightRoofCharge` | 由 Board 在天气事件唯一边沿先按稳定实体 ID 冻结全部目标与提供者分配，让所有受该区域影响的实体消费同一批范围后，再按提供者 ID 归并为每次事件一次反噬；这样提供者被反噬击杀也不会让同一次事件的后续目标漏保护或漏压制。重叠范围按规格的距离与稳定 ID 决胜。品种接口只声明资格、范围和直接本体反噬，不在 Board 堆植物类型表；免除离散天气状态不得顺带清除仍有效的连续区域暂停。若能力同时压制某种僵尸，植物继续声明空间语义，僵尸自己的能力入口查询 Board 聚合结果，保留目标所有者对承接、过载和存档的权威。专项覆盖多植物层归并、重叠、提供者致死、连续暂停不被免疫及目标僵尸能力仍保留其他抗性。 |
| **承载层紫卡原位升级并提供事件边沿保护** | LightningRodPot → `PlantUpgradeLayer::UNDER` + `Plant::IsRoofSupportPlant` | 升级规则必须声明替换 `under` 而不是默认 `normal`：先创建新 under 并切换 Cell ID，再让旧 under 正式死亡，原格 `normal/pumpkin/overlay` 全部保留；读档创建、渲染层、卡槽预览、屋顶放置、径流与台风等支持语义都改查能力接口，禁止继续写死 `PLANT_FLOWERPOT`。同格保护用 Cell 的 O(1) 分层查询，只在劫持/雷击结算边沿调用；同行非叠加光环在已锁定行的固定列上扫描一次并取最大倍率，空承载返回中性值，不保存可由当前层组合派生的活跃标志。专项覆盖原子升级、组合层保留、空盆 no-op、多盆不叠加、外伤毁盆留上层、存读档和默认实例/`-NoInstance` 同步截图。 |
| **场上唯一、死亡后可重种并复用卡槽作控制台** | Plantern → `Board::mActivePlanternID` + `CardSlotManager` | 唯一性从当前活动实体 ID 派生，创建/读档重建、死亡/压扁释放，不能复用“累计种植次数”计数；卡槽菜单只持 UI 瞬态，玩法状态留在实体/Board。卡片和本体只请求展开，同一按钮输入走真实 `click` + 截图验收。需要覆盖场景自定义面板时，不能只依赖 CardUI 的对象层级：`GameObjects` 绘制命令会早于后注册的天气面板，应把菜单拆成更晚的场景 UI 绘制命令；重叠区输入仍须由菜单优先消费。若资源经济要求前宽后紧，按每关当前波/总波数独立归一化，不把补给挂在后期天气压力上；至少用两种总波数验证首尾端点。每波“出生分配上限”不能防止跨波携带者被范围伤害同时兑现：另在 `ReserveFuel` 限制同批在途吸收量，并对高收益挡按同一波次进度提高消耗；专项须同帧结算超过上限的多份奖励 |
| **飞行资源抵达实体后才到账** | Plantern `ReserveFuel` → `MistFuel` → `DeliverReservedFuel` | 生成飞行物时只预留容量，抵达目标才增加玩家可见数值；多团在途必须共同占用容量，目标死亡则丢弃。飞行对象若不进入通用存档，实体须保存预留量，并在读档时结算或重建飞行，禁止奖励丢失或永久占仓 |
| **消耗型实体资源的低量警报** | Plantern `previousFuel >= T && fuel < T` | 只在正式消耗前后比较阈值下降沿，禁止保存或初始化 `warned` 标志，避免持续刷屏和低量读档伪触发；补回阈值后自然重置。瞬时反馈用既有音效 + `BoardPresentation` 的短时中央警报，高倍速下用未缩放时间；警报结束后由卡牌保留持续低量指示。专项断言音效请求次数、提示内容、5 倍速不提前消失和同步截图，测试 setter 只负责布置阈值上方状态，不直接伪造下降沿。 |
| **逐格可见性限制远程索敌** | Plantern/Fog → `Board::CanPlantAcquireZombie` | 继续以目标行平滑后的逐格 alpha 为唯一权威；若允许看入第一格薄雾，目标格超阈值后只检查它朝植物方向的相邻格是否已可见。不要按地图固定雾线或复制照明形状，否则开关灯过渡与画面不同步。AutoTest 锁定高 alpha 第一格 true、第二格 false，并覆盖照明边缘 |
| **空中/地面分层索敌** | Cactus → BalloonZombie | `Board::CanPlantAcquireZombie` 先调用植物虚接口决定能否索敌，再叠加雾可见性；普通植物默认只认地面，特殊植物分别缓存空中/地面目标并让空中优先驱动姿态。发射时把当前姿态写入子弹的命中层，子弹碰撞再向僵尸查询当前 phase；层标记须入档且在对象池复位。伸缩和射击的一次性动画回调必须同时核对 phase 与当前轨道，低/高姿态分别使用经截图验证的稳定发射偏移。AutoTest 用聚合计数断言空/地弹互斥，不依赖 `bullets.N` 顺序 |

**僵尸新状态效果专属清单**（血泪教训浓缩）：

1. **字段**加 `Zombie.h`，默认值=中性（无效果）；状态入口收敛成一个 `StartXxx()` 方法。
2. **行为守卫放虚函数、不放 lambda 回调**——`onTriggerStay` 这类 lambda 会被别的路径绕过（魅惑撑杆实测教训）。
3. **入口/出口清单**：新状态改变的每个视觉/行为量（动画轨、翻转、手臂显隐…），把"进入状态、退出状态、死亡、断头、被魅惑、读档还原"每个入口逐一过——铁门僵尸 3 个 bug 全是入口漏同步。
4. **三条创建路径**初始化各自正确：波次 `Board::CreateZombie`（出生初始化）、读档 `CreateZombieWithID`（**绝不**在此初始化，由 Load 还原）、预览 `InstantiateZombieFree`（跳过效果）。
5. **存档**进 `Zombie::Save/LoadProtectedData`（基类无条件调用，不怕子类覆盖 SaveExtraData）；计时器类状态照"存读档心智清单"过一遍。
6. **交互矩阵**先和主人对齐规格：与魅惑、啃食状态机（走路权威=`PlayWalkAnimation`）、减速是否叠加、哪些僵尸免疫、持续时间、**效果期间新刷出的僵尸是否受影响**。
7. 动画冻结/变速类表现走 **Animator 三层速度模型**（`EffectiveSpeed=(clip?clip:base)*extra`，clip=0 回落 base，见 [[project_pvz_animator_clip_speed]]）；状态变色用 `OverrideColor`、特效用 `EmitEffect`。"全场"结算 = 对 0..mRows-1 行逐行 `ForEachZombieInRow`，没有整场 API。
8. **extra 速度层必须单点收敛**：所有改速度状态的路径统一走 `Zombie::UpdateAnimSpeed()`（冻结0 > 减速×因子 > 常速；因子差异用 `GetSlowAnimFactor()` 覆写）。直调 `SetExtraSpeedMultiplier` 的旁路（如报纸狂暴）会把停格顶掉——寒冰菇实施时已把存量调用点收编，加新状态先 grep 这个函数。
9. **视觉反馈别耦合在别的效果上**：蓝色 overlay 原先绑在减速里，持盾僵尸免减速→冻结了却不变蓝（主人一眼抓出）。新状态的视觉在自己的入口/出口开关，别搭别的状态的便车。
10. **豁免语义连伤害一起豁免**：原版 HitIceTrap 的 20 伤害在免疫判定**之后**——魅惑/跳跃中撑杆连血都不掉。把"伤害+状态"整体放进 StartXxx()，别在植物侧拆开无差别结算。
11. **dump_state 加字段 + assert**（仿 `slowCooldown`/`frozen`/`armVisible`），否则 AutoTest 对新状态是瞎的。浮点计时器另配一个 bool 投影字段供 equals。
12. **共享 overlay 必须单点合成优先级**：魅惑、冻结/减速、中毒等共用 Animator overlay 时，让 `UpdateStatusOverlay()` 从全部权威状态派生最终颜色；每个进入、到期、清除、魅惑与读档路径都调用它。禁止某状态结束时直接 `EnableOverlayEffect(false)`，否则会抹掉仍有效的较低优先级状态。
13. **目标级持续伤害要保存完整时间状态**：共享层数上限放在目标实体，独立层保存各自剩余时间；跨帧小数余量按未减速的游戏 `deltaTime` 累积并与计时器一起入档，旧档默认中性。规格若要求魅惑清除延迟伤害，唯一魅惑入口和读档归一化都要清层；不绕盾的毒伤继续走 `TakeProjectileDamage(..., velocityX=0)`。专项覆盖满层后的刷新、存读档、魅惑、护盾以及不同 `timeScale` 下相同游戏时间等伤。
14. **啃食触发的跨行反应必须脱离植物生命周期**：第一口正式承伤后在 `Zombie` 建立独立 phase/timer，停止啃食时走统一目标清理以归零植物 `eaterCount`，后续碰撞不得重新开吃。换行直接更新权威 `mRow`，让通用行桶与碰撞桶下一帧从它重建；视觉 Y 再按 Board 地形追赶，并与 phase/计时/是否已换行一起保存中途 Y。目标行须按边界、介质和品种可生成条件筛选；死亡、掉装备等强制演出要原子取消，魅惑是否取消按规格单独决定。专项覆盖停顿、换行中快照、边界行、泳池、屋顶、装备破损和魅惑。
15. **临时替换动画轨贴图时先比较有效像素框**：同宽贴图也可能因画布高度或顶部透明留白不同而看似整体下沉。量取原图/替换图 alpha bounds 后用轨道局部 offset 补偿，禁止挪整只僵尸或改共享 reanim；所有退出、掉头、装备切换和读档恢复入口都要恢复原图、offset 与附属轨显隐，并用同步截图逐品种核对颈部基准。

**动画子弹与对象池附加清单**（Torchwood/FirePea 实证）：

1. 对象池槽位保留不可变的“分配类型”，另存可变的“当前类型”；`Reset()` 必须恢复类型、基础伤害、转换防重标记、纹理/Animator、速度和阴影。否则点燃过的 Pea 槽位会以 Fireball 身份污染下一发。
2. Animator 组合要镜像 `AnimatedObject` 的并行推进握手：`UpdateParallelDeferred` 成功后串行 `Update` 只清标记，没走并行时才回退 `Animator::Update()`；Draw 直接走 Animator，默认实例化与 `-NoInstance` 都要截图。
3. 存档同时保存不可变 `poolType`、可变“当前类型”和会影响后续转换的防重字段；读档按 `poolType` 走 `BulletPool::AcquireShared`，再恢复当前表现，**不得把 `mFromPool` 改回 false**。专项快照断言 bullet 的 `fromPool=true`、`poolType` 和转换列，防止动画变种读回错误池槽。
4. 同帧 AutoTest 可能先构建行索引再 `spawn_zombie`；`EntityManager::AddZombie/AddZombieWithID` 必须置 `mRowIndexDirty=true`，否则随后同帧的范围弹只看见旧桶。
5. `dump_state` 为类型数量、动画表现和防重状态加聚合整数抓手；不要依赖 `unordered_map` 导出的 `bullets.N` 顺序。运动弹仍断言相对量，不断言绝对 X/Y。
6. `onTriggerStay` 的逐碰撞逻辑帧伤害不受渲染 FPS 波动影响，但固定逻辑步的回调次数不会随 `timeScale` 改变；若直接每次扣固定整数，移动弹在 0.5x/2.0x 下会因重叠帧数反向变化而失衡。默认按目标累计 `damage × GetDeltaTime()/GetFixedStep()` 的小数额度再取整结算，并把每个整数额度分别走一次 1 点正式承伤链，禁止合并成 `TakeDamage(N)` 而改变免伤次数/逐击取整。若特定目标要修改“每帧总基础伤害”，必须先由目标侧虚钩子修正 `damage` 再累计；不能依赖后续普通单击上限，因为拆开的每次输入已经只有 1 点。专项断言 0.5x 60 帧、1x 30 帧、2.0x 15 帧的同目标总伤害完全一致，并覆盖特殊目标的有/无防具两态。
7. 持续命中且限制穿透数的子弹要按稳定实体 ID 记录不同目标：首次接触才登记并播放反馈，`stay` 只继续伤害；达到上限时先让最后目标承伤再回收。目标名单和逐目标小数伤害余额必须一起随存档恢复，并在对象池 `Reset()` 清空；专项至少覆盖“上限减一”目标后的存读档、达到上限消弹、倍速等伤和复用槽位归零，禁止把测试写死在某个穿透数。
8. 纹理子弹若新增旋转、纵向速度或其他影响命中与表现的运动状态，必须在 `BulletPool::Reset()` 归零、在存档中往返，并让读档恢复值覆盖表现初始化时的随机值。跨行直线弹按移动后的 Board 网格更新 `mRow`，再走统一碰撞链；场景上下边界用 `SCENE_HEIGHT`，横向仍沿用通用回收边界。专项用方向聚合断言同帧齐发数量，用直接 `spawn_bullet` 覆盖跨行、命中、对象池和存读档，不依赖运动弹的绝对坐标或数组顺序。
9. 运行时转换产物若拥有独立伤害、状态或表现语义，追加独立的当前 `BulletType`，不要再从 `mPoolType` 猜玩法身份；`mPoolType` 只负责对象池回收归属，`mBulletType` 统一驱动命中分派、表现和存档。新类型在枚举末尾追加，并同步穷举风力表、默认伤害、碰撞入口、表现/阴影、对象池直接创建分支及 AutoTest 名称/聚合计数；专项同时断言转换后当前类型、原池类型、存读档、回收复位和普通变种未串型。
10. 派生溅射弹若以缩小范围换取次要目标状态，范围和状态分派都按当前 `BulletType` 决定，基础弹保留原范围；只给实际进入溅射集合且仍有效的目标施加状态。专项在同一行放置直击、范围内和范围外目标，逐只断言状态与生命，并回归基础弹的原溅射宽度，禁止只用总数或总伤害掩盖边界选错目标。
11. 换色子弹还要审计命中粒子 XML 的每个固定 `<Image>` 键；本体贴图换色不会自动改变飞溅/碎屑。需要独立配色时按当前 `BulletType` 分派独立效果名，并按 adding-particle 的资源闭环注册同布局图集；专项同步截图后同时断言派生效果存在、基础效果计数为 0，再跑基础子弹回归防止串色。
12. 屋顶平射遮挡按子弹类型的视觉离地高度与 `Board::GetRowCenterYAtX(row, bulletX)` 比较；豌豆、孢子、尖刺、星星等阈值可不同，投掷物不进入这条判定。遮挡时先走对应命中反馈再回收，不能把子弹本体强制贴坡冒充遮挡；阴影若存在则可独立采样当前 X 的地面。专项在坡段与平台各放正/负例，并回归草地地图不受影响。
13. 固定飞行时间的投掷物优先用“预测落点 + 解析抛物线”：发射时通过目标公开的水平速度入口预测 X，Y 取碰撞箱稳定部位；带 `_ground` 的根运动目标必须以**当前活动动画片段的平均逐帧位移**估算未来速度，禁止把发射瞬间单帧 `GetTrackVelocity` 外推到整段飞行——步态停顿帧会严重少预判，跨步帧会严重多预判，动画倍率还会同步放大两类误差。以 `p=elapsed/duration` 计算 `lerp(start,target,p) + (0,-4*apex*p*(1-p))`，不要在植物或子弹里按僵尸类型堆落点补丁。起点、落点、elapsed、duration、apex 全部入档并在对象池复位；飞行高段关闭碰撞器，只在下降接近落点时开启，到点后留一个碰撞帧宽限再按落空反馈回收。阴影独立采样当前 X 地形并随离地高度缩放，屋顶不套平射遮挡。专项除移动目标命中、拱顶碰撞关闭、在途快照、落空、池槽归零和屋顶外，还要在同一变速根运动目标的多个步态相位断言相对提前量稳定。
14. 跨行溅射先用弹丸权威 `mRow±radius` 查行桶，每行只以当前弹心的水平命中窗口与目标 collider 求交；不得把 C# 800×600 中含绝对 Y 的 projectile rect 直接搬入 Board 网格。先快照稳定实体 ID，再分开结算直击与次要伤害，避免死亡/回收让遍历失效；规格必须明确二类护盾是“绕过不伤盾”还是 `penetrateShield` 的“盾与后层同伤”，不得互换。专项同 X 放命中行、相邻行与范围外行，逐只断言生命；另用二类护盾断言盾/本体分层，并覆盖在途快照、落空反馈与池槽复用。

15. 只更换弹种、音效或命中语义且共享射击周期、动画帧和存档状态的升级变体，让基础射手持有全部状态并暴露窄虚入口返回 `BulletType`，派生类只覆写品种差异；禁止复制一套计时/存档状态机。若穿透伤害已实际进入持盾目标后层，而 C# 状态语义要求同次命中影响该目标，状态入口须显式采用与该伤害一致的护盾口径，并分别断言盾、本体与状态，不能让仍存在的盾再次把状态挡掉。

这类跨系统植物**不走"简短 spec 直实现"捷径**：回到完整 brainstorm（交互矩阵逐项问主人）→spec→必要时 writing-plans。

## 流程与模板

纯植物侧的小套路：brainstorm 问清关键项→简短 spec 存 `docs/superpowers/specs/`→直接实现（不必单独 writing-plans）。模板：`2026-07-08-scaredyshroom-design.md`。完成并验证后由 Codex 提交，再按仓库风险、工作区状态和上游是否明确决定是否常规 push。

**每次完成并验证任何植物新增或实质修改后，必须在提交前完善本 skill**：把本次实际暴露的新坐标换算、交互契约、foot-gun 或验证手法浓缩进相关章节；已有规则则合并强化，不堆一次性日志。任务同时修改粒子、僵尸或天气时，也同步完善本次实际使用的对应 skill。更新后运行 skill-creator 的 `quick_validate.py` 校验全部改动过的 skill。

## 关联记忆

`[[project_pvz_gamedata_json]]`（权威单份资源）、`[[project_pvz_autotest_suite]]`、`[[reference_pvz_assets_worktree_autotest_gotchas]]`。
