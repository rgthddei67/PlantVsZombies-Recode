---
name: adding-plant
description: Use when adding ANY new plant (新增植物) to PvZ — 射手/生产/即时消耗/全场结算，包括效果侵入僵尸或其他系统的（冻结/减速/魅惑/穿透/新子弹/格子占用）— proven on ScaredyShroom (胆小菇), IceShroom (寒冰菇), DoomShroom (毁灭菇+弹坑).
---

# 给 PvZ 新增植物

**最高原则：不确定的事立刻问主人，不要自行推断。** 已知必问项：①**帧事件的帧号**（主人会看动画预览，他有准确答案；从 reanim 活跃区间推断会错——胆小菇实测 25，区间末段推法给 28）②数值/行为想偏离原版时。**帧号口径（主人 2026-07-14 定死）：`AddFrameEvent` 真实帧号 = 动画预览帧号 − 1；主人报给你的帧号默认已经 −1 过，代码直接用、不许再减**。只有自己从预览工具读数时才需要手动 −1。

## 坐标换算铁律

**C# 原版逻辑场景是 800×600，本项目是 `SCENE_WIDTH=1100`、`SCENE_HEIGHT=600`。原版任何绝对 X/Y、范围端点、绘制偏移、碰撞框和粒子触发点都只能当语义参考，禁止直接抄入代码。**

- 场景范围由 `SCENE_WIDTH/SCENE_HEIGHT` 和 Board 当前背景几何重算；格子范围由 `GetCellCenterPosition`、`GetCellHeight` 与 `CELL_COLLIDER_SIZE_X` 派生。
- 植物局部点位先换算到本项目以格子中心为 `GetPosition()` 的口径，再叠加当前 gamedata 视觉偏移；逻辑格位置与 `mVisualOffset` 永远分开。
- 发射点、范围边界和附加 Animator 基点优先表达成“相对稳定视觉原点/父轨基准姿态”的差值，不把 C# 的世界坐标塞进局部偏移。
- AutoTest 先执行同步 `screenshot`，再用 `animatedObjectsByTag.Plant` 的 `renderProbeReady/worldBounds/visualToRenderCenterD*Int/nearestPlant` 验证本项目最终绘制几何相对格子与植物 collider 的关系；每阶段只保留一株目标植物以稳定数组索引。
- 修改 gamedata offset、附件、整株变换或 `SetRenderScale` 时，默认实例化与 `-NoInstance` 各跑同一静止用例并比较整数 `worldBounds`；截图负责肉眼基线，运动对象瞬时绝对 X/Y 只供诊断、不作稳定断言。

## 第 0 步：勘察（动手前全部做完）

1. **读 reanim**：`build/clang-release/resources/reanim/<Name>.reanim`，用 Grep `<name>` 提取全部 track 名，`anim_xxx` 即可用动画（具体可以询问主人，有些anim_xxx并不是可用动画，而是一个track）；`<f>-1/0</f>` 对定位 anim 轨活跃帧区间。
2. **读 C# 参考并主动盘点音效**：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Plant\Plant.cs`，grep 植物名，读专属 Update 函数 + 发射物类型 + mShootingCounter/state 分支，数值忠实原版；同时收集相关路径的全部 `PlayFoley` / `PlaySample`，不要等主人听出缺声才补。沿 `FoleyType → Sexy.TodLib/Foley/TodFoley.cs → Resources.SOUND_*` 得到精确资源键，以资源键去掉 `SOUND_` 后的小写名到 `D:\PVZ\中文年度加强版完整版\Test\sounds\` 查同名 `.ogg`。找到后复制到唯一权威 `build/clang-release/resources/sounds/` 合理子目录，并同步 `resources.xml` 与 `ResourceKeys.h`；找不到才问主人，禁止用相近声音静默替代。构建后检查 `manifest.txt` 和启动日志无 missing sound，并用可见行为路径及 `GetSoundPlayRequestCount` 投影验证触发次数（含读档不得重响）。
3. **盘点已就位的基建**（常常提前有了，别重复加）：`PlantType.h` 枚举、`TestDriver.cpp` kPlantNames、`ResourceKeys.h` RKEY、`AnimationTypes.h`、卡片图 `PlantImage/<Name>.png`、reanim 部件图。缺哪补哪。
   `image/reanim/` 全目录预加载生成 `IMAGE_<文件名大写>`；只有被 reanim XML 的 `<i>` 直接引用的部件才会额外获得 `IMAGE_REANIM_*` 别名。运行时动态换入、但不在 XML 时间线出现的受伤材质必须用前者。更新派生阶段时先确认 `GetTexture` 非空，再提交阶段缓存，避免“状态断言通过、画面仍是旧图”的假绿。

## 实现清单

1. **类**：`Game/Plant/<Name>.h/.cpp`。选基类：蘑菇→`Shroom`（白天睡觉自动处理）；豌豆系带独立头部动画→`Shooter`；其余→`Plant`。抄最像的现有植物结构（喷射蘑菇抄 `PuffShroom`）。
2. **注册**：`GameDataManager.cpp` 加 `#include` + `RegisterPlant(type, "PLANT_X", IMAGE_X, ANIM_X, "ReanimName", &MakePlant<T>)`。卡片由注册表数据驱动，**无需单独加卡**。
3. **gamedata.json**：在 `build/clang-release/resources/gamedata.json` 加 `{cost, cooldown, offset, scale}` 条目（缺任一字段拒启动 exit -6）；其他 preset 自动共享。
4. **info.txt（图鉴文案）**：在 `build/clang-release/resources/info.txt` 加两段——`[PLANT_X]` 下一行图鉴名字、`[PLANT_X_DESCRIPTION]` 下一行介绍（enum 名与注册的 "PLANT_X" 严格一致；解析器只认 `[key]`+正文，多行正文允许）。缺条目图鉴显示空白不报错，极易漏。
5. **资源入库**：reanim、贴图、声音、resources.xml 都只改 `build/clang-release/resources/` 这一份权威资源；严禁为 playtest/debug 建副本或 Copy-Item 同步。新增文件仍因 build/ 被忽略而需要 `git add -f`。
6. **射击类惯例**：帧事件发弹（帧号问主人）+ `mShootTimer` 乘词条攻速 `GetPlantAttackSpeedMultiplier()` + 索敌用 `ForEachZombieInRow`（严禁全表扫）。跨行/范围判定注意坐标换算：**C# 的 mX/mY 是格子左上角，本项目 `GetPosition()` 是 80×100 格子中心**，差 (40,50)。
7. **动画状态机**：一次性→循环用 `PlayTrackOnce(track, returnTrack, speed, blendTime, returnSpeed, returnTrackBlendTime)` 自动接轨，完成信号 = `GetCurrentTrackName()` 变成 returnTrack（天然兼容存读档，勿自造 loop 计数）。`blendTime` 只管进入一次性轨，`returnTrackBlendTime` 独立控制返回；原版若用 `SetFramesForLayer` 在重合/相邻包装轨边界硬切，最后一个参数显式传 `0.0f`，否则历史默认 0.5 秒可能插值出资源中不存在的部件姿态。返回速度与返回混合都属于待执行状态，主 Animator、Shooter 头和其他自管 Animator 必须一起保存；旧档缺返回混合字段时默认 0.5 秒。
8. **帧事件是全局帧号、跨轨道通用**（`Animator::mFrameEvents` 只按 int 帧号，不分轨道）：定下触发帧后必须核对**其他 anim 轨的活跃窗口扫不到它**（毁灭菇 51=explode(19..51) 末帧，sleep(52..76)/idle(0..19) 都够不着才安全；末帧触发安全——普通前进与循环回绕分支都覆盖）。原版动画速率≠reanim 基础 fps 时，用 `PlayTrack(track, 原版fps/reanim fps)` 折算（毁灭菇 23/12≈1.92）。
9. **整株世界变换必须覆盖复合 Animator 的两条 A/B 路径**：默认路径会把根 Animator 与任意深度附件按轨道顺序递归写入 GPU `InstanceRecord`，`-NoInstance` 才整棵走矩阵慢路径；外层 `Graphics` 变换栈不会覆盖默认实例路径。应在 `Animator` 最终矩阵/`InstanceRecord` 两处统一实现世界变换，并递归同步现有与以后附加的子 Animator；AutoTest 同时断言根/子变换，默认与 `-NoInstance` 都要逐张检查截图。
10. **C# 复合头附件要补 `inverse(basePose)`**：C# `AttachToAnotherReanimation` 的附件矩阵是父轨道当前姿态乘基准姿态逆矩阵，而本项目 `AttachAnimator` 目前只直接乘父轨道当前姿态。子 reanim 仍使用整株绝对坐标时，必须从根返回/待机轨首帧读取每条附件轨各自的基准姿态并在子 Animator 局部变换中抵消；基准旋转/缩放为单位时就是 `SetLocalPosition(-baseX, -baseY)`。不能给多个头套同一个 `gamedata` 偏移，否则某个头会与茎错位；默认和射击轨都要可见截图校对。
11. **不可啃食植物覆写 `CanBeEaten()`，不要在僵尸索敌处堆类型表**：植物自己声明契约，普通啃食路径会统一跳过；AutoTest 同时断言植物 `canBeEaten=false`、`eaterCount=0`、僵尸 `isEating=false` 和植物生命不变，防止只有视觉没播啃食但伤害仍在结算。
12. **只有完整时间轴、没有 `anim_*` 包装轨的循环 reanim**（如 `FirePea.reanim`）用 `SetFrameRangeToDefault()` + `Play(PLAY_REPEAT)`，不要捏造轨道名或帧事件。非等比 `SetRenderScale` 的 pivot 是**世界坐标**；命中特效应传自身绘制基点，传 `(0,0)` 会把整个特效按比例拉向屏幕左上角。
13. **跳跃阻拦植物只声明能力和反馈，不决定僵尸动画时序**：`BlocksZombieJump`/`OnZombieJumpBlocked` 由跳跃者在原版动画进度节点调用，接触植物时不得提前 Bonk、喷粒子或扣血。组合植物的跳跃目标取当前格顶层，避免先碰到底层睡莲/花盆便漏掉上层阻拦体。特殊僵尸若撞伤阻拦植物，应把植物引用传入品种钩子并走带正确 `DamageSource` 的正式承伤链；先确认规格中的受伤者，不能把“植物损失 N 血”误实现成僵尸自身扣血。
14. **台风锚定植物也只声明格位能力和直接撞击反馈**：用类似 `AnchorsPlantCellAgainstTyphoon` / `OnTyphoonPlantImpact` 的虚接口让天气唯一结算点派发，禁止在 `Board` 堆植物类型表。逐格位移中先让锚定源格保持不动，再只对直接进入锚定目标格的植物组合结算；后方被普通占格挡住时不传导压力。伤害必须逐格立即生效，使锚定植物中途死亡后剩余步数能重读格位；同阵风重复撞击可按锚定植物 ID 合并音画反馈，但不能合并伤害。组合植物按一个移动格计数，专项覆盖双向、紧邻多步、间隔移动、连续链、水路上下层和中途死亡放行。
15. **直接落水植物必须同时声明地形、层级与水面表现**：在 `Board::CanPlantAt` 的水生集中分支要求“水格且 `Cell::IsEmpty()`”，从而允许空水直种并同时拒绝陆地、已有睡莲和其他植物；除睡莲继续占 `under` 外，水草/海蘑菇这类能力植物占 `normal`，不要为了直种改 `CreatePlant` 的通用层级。原版不画陆地影子的品种移除 `ShadowComponent`；发射点和附属视觉从 `GetVisualAnchorPosition()` 派生以跟随水面浮动。专项用 `assert_can_plant` 覆盖空水 true、陆地 false、睡莲水格 false，再断言 `cells.*.normal/under`、无阴影和同步截图。

## 存读档心智清单

普通 AutoTest 仍会短路玩家 `saves/`，但可用 `save_level_snapshot` → 主动改局面 → `reload_level_snapshot` 在脚本输出目录内验证“正式序列化 → 销毁旧 GameScene → 新场景正式反序列化”。这能覆盖实体与 Animator 的进程内往返；中央存档路径、跨进程退出重进和迁移行为仍需按任务风险另行验证。禁止临时关闭 `GameAPP::mAutoTestMode` 绕过保护。

- 自定义状态（状态机枚举、计时器）→ `SaveExtraData/LoadExtraData`。
- 新字段能用中性默认值表示旧档时保持兼容；结构或语义变化无法只靠默认值表达时，提升 `SaveSchema::kCurrentLevelVersion`，增加连续迁移和 `SaveSchemaTests`。JSON 必须先升级成功，再恢复 `Board` 与实体。
- 动画轨道/PlayTrackOnce 进行态：`GameInfoSaver::RestoreAnimState` 已统一恢复，**不用自己存**；`LoadExtraData` 在其后运行可放心覆盖。
- **由生命值等已保存数据派生的材质/阶段要在 `LoadExtraData` 主动重建，但必须走显式“只恢复终态、不播放反馈”路径**；不要复用会喷粒子、播音效或再次结算阈值奖励的实战更新入口。快照测试同时断言派生材质/阶段恢复且对应反馈计数为 0。
- **凡是被状态机消费的节流缓存，读档第一帧不得吃初值**——缓存初值=给读档后的世界注入捏造状态（胆小菇实测：`mScaredCached=false` 初值让 SCARED 态读档后误判"僵尸走了"先伸头再缩回）。修法=计时器初始即到期，首帧强制真算。

## 验证（缺一不可）

1. 默认用 `clang-playtest` 构建 0 warning；只有正式发布、主人明确要求或验证发布配置时才用 `clang-release` 做 LTO 验证。两个 Clang 预设报告同一套全量警告。
2. **站位+影子截图校对**（写完必做，别等主人指出）：临时脚本把新植物与小喷菇/向日葵种同一行，截图比脚底基线。两套独立坐标：**本体 = gamedata.json 的 offset**（改无需重编译）；**影子 = 代码里 `ShadowComponent::SetOffset`**（改要重编译）。抄同类植物的值大概率不准。
   植物若有阵风插值、水面浮动等动态位移，收口成**不含品种静态 offset 的公共视觉锚点**：本体=`锚点+gamedata offset`，影子=`锚点+shadow offset`，禁止影子退回裸 `Transform` 而漏掉动态量。专项在同步截图后导出 `ShadowComponent` 最近实际提交的中心，并用**同一绘制帧**的 Animator render base 减 gamedata offset 得到锚点再断言；不要在截图后的下一逻辑帧重算正弦锚点，否则会产生亚像素相位差假失败。最后跨两个动画相位读图确认共同移动。
   卡槽/选卡卡图由 `CardDisplayComponent::DrawPlantImage` 独立绘制，不能为缩卡图去改 gamedata `scale`（那会改草坪本体）。需要品种特例时在通用卡图倍率上追加独立倍率，并从既有卡图矩形中心缩放，避免向左上角漂移；用实际卡槽截图验收。
   若卡槽本身承载“下一株”的玩法设置（方向、模式等），权威状态放在该 `CardComponent`，卡面图标/箭头等提示和预览都实时消费同一状态，种植成功时再复制到实体；之后改卡不能追溯已落地实例。卡片和实体分别保存各自状态，旧档给中性默认。右键切换必须先于通用右键取消选择消费，并允许冷却中的卡修改。AutoTest 连续合成点击时，`click` 完成只代表松开事件已入队；移动鼠标或断言选中前至少留一个真实输入处理帧，否则松开会在新坐标结算而丢失点击。
   修改卡槽悬停或点击落格时，预览与种植必须共用同一个“世界坐标 → 唯一 `Cell`”解析入口和相同的边界归属规则。`ColliderComponent::ContainsPoint` 的矩形四边都是闭区间，相邻格边缘会同时命中；禁止预览按行列扫描、点击却依赖 `ClickableComponent` 渲染顺序，否则边缘和四格交点会显示一格却种到另一格。
3. **AutoTest 冒烟**：`autotest/scripts/smoke_<name>.json`，每阶段**只种一棵**（plants dump 顺序来自 unordered_map，多棵时下标不可靠），断言 `plants.0.track`；`plantDefinitions.<TYPE>.sunCost/cooldownMs` 可直接锁定 gamedata 数值。几何验收用 `animatedObjectsByTag.Plant.0` 的最终世界包围盒与相对 collider 投影，禁止把 C# 绝对坐标写成期望值。时序估算用僵尸判定矩形 `[x±25]×[y-65,y+35]`、步速 23~45px/s。**exit 0 ≠ 通过**：必须逐张 Read 同步截图 + dump 数值核对（防假绿）。
4. 蘑菇夜测用 level 10-18（九关制的 2-1..2-9）；白天睡觉断言 `anim_sleep`；魅惑僵尸清场用 `charm_zombie`（不触发输局）。
   只能种水路的蘑菇改用夜间泳池 level 28+ 验活跃态，并另在日间泳池 level 19+ 验 `anim_sleep`。

## 特性侵入其他系统时（寒冰菇冻结、魅惑、穿透这类）

纯植物侧的清单不够用了，先按效果落点归类，逐类有先例可抄：

| 落点 | 先例 | 关键点 |
|---|---|---|
| **僵尸新状态效果**（冻结/减速/魅惑…） | 减速=SnowPea→`Zombie::SetCooldown`；冻结=IceShroom→`StartFrozen`；魅惑=HypnoShroom→`StartMindControlled` | 见下方专属清单 |
| **全场即时结算**（寒冰菇式） | `IceShroom`（帧事件→音效+白闪+逐行结算→`Die()`，仿 CherryBomb 骨架） | 植物先检查 `mBoard->GetPresentation()`，再用 `ShowScreenFlash(...)` 请求全屏效果；由 `GameScene` 实现 `BoardPresentation` 并注册绘制，禁止植物或 `Board` 恢复具体 `GameScene` 依赖；**isPreview 特判否则图鉴也结算**（主人叮嘱） |
| **格子占用系统**（弹坑/墓碑类） | `Crater`（毁灭菇弹坑，阻种 180s） | 轻量 GameObject（Trophy 先例，LAYER_GAME_OBJECT=背景上植物下）+Board weak_ptr 簿记+存档数组（旧档兼容=无字段则空）；**占格判定有两处独立口径都要改**：`CanPlaceInCell`（阻种）+`UpdatePlantPreviewPosition`（落点预览隐藏——漏改=悬停占用格仍显示预览，主人验收抓出）。AutoTest `plant` op 直连 CreatePlant **绕过闸门**：基础规则用 `assert_can_plant` 同时断言占用格 false 与旁格 true；若改了卡槽/悬停 UI，再补 `click` 真实路径与同步截图 |
| **清除同格组合植物** | DoomShroom→`KillOtherPlantsInCell` | 按 EntityManager 的全部植物 ID 快照遍历，以逻辑 `row/column` 过滤并排除施法者，再逐株 `Die()`；不要只写死当前 under/normal 两层，否则以后南瓜等额外层会留在结算后的弹坑里 |
| **引爆倒计时无敌** | CherryBomb / DoomShroom 的 `TakeDamage` 覆写 | 只 `SetGlowingTimer(0.1f)` 不掉血；**有睡觉态的要放行睡觉分支**（白天=普通蘑菇照常被啃，毁灭菇实证） |
| **新子弹/运行时变种** | `PuffBullet`；Torchwood→FirePea | `BulletType` + BulletPool；动画子弹可在 `Bullet` 组合独立 Animator，但命中语义必须按“当前类型”分派，不能依赖分配时的 C++ 子类 |
| **新粒子特效/染色变种** | IceFumeCloud（寒冰大喷菇） | XML 标签全参考在 **adding-particle skill**，勿再读 ParticleSystem 源码 |
| **TakeDamage 类钩子** | FumeShroom 的 `penetrateShield` 参数 | 穿透只对二类护盾（门/报纸），不穿头盔；改签名先看全部调用点 |
| **即时/范围结算** | CherryBomb/大喷菇锥形 | 帧事件触发结算帧（帧号问主人），范围判定用行桶不全扫 |
| **目标类型拥有特殊受击语义** | Caltrop → `ZamboniZombie::HandleCaltropHit` | 植物只负责命中与派发，目标基类拥有虚事件并处理消耗植物/动画/存活；精英变体覆写目标方法，禁止在植物攻击函数里继续堆具体精英类型分支 |
| **场上唯一、死亡后可重种并复用卡槽作控制台** | Plantern → `Board::mActivePlanternID` + `CardSlotManager` | 唯一性从当前活动实体 ID 派生，创建/读档重建、死亡/压扁释放，不能复用“累计种植次数”计数；卡槽菜单只持 UI 瞬态，玩法状态留在实体/Board。卡片和本体只请求展开，同一按钮输入走真实 `click` + 截图验收。需要覆盖场景自定义面板时，不能只依赖 CardUI 的对象层级：`GameObjects` 绘制命令会早于后注册的天气面板，应把菜单拆成更晚的场景 UI 绘制命令；重叠区输入仍须由菜单优先消费。若资源经济要求前宽后紧，按每关当前波/总波数独立归一化，不把补给挂在后期天气压力上；至少用两种总波数验证首尾端点。每波“出生分配上限”不能防止跨波携带者被范围伤害同时兑现：另在 `ReserveFuel` 限制同批在途吸收量，并对高收益挡按同一波次进度提高消耗；专项须同帧结算超过上限的多份奖励 |
| **飞行资源抵达实体后才到账** | Plantern `ReserveFuel` → `MistFuel` → `DeliverReservedFuel` | 生成飞行物时只预留容量，抵达目标才增加玩家可见数值；多团在途必须共同占用容量，目标死亡则丢弃。飞行对象若不进入通用存档，实体须保存预留量，并在读档时结算或重建飞行，禁止奖励丢失或永久占仓 |
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

**动画子弹与对象池附加清单**（Torchwood/FirePea 实证）：

1. 对象池槽位保留不可变的“分配类型”，另存可变的“当前类型”；`Reset()` 必须恢复类型、基础伤害、转换防重标记、纹理/Animator、速度和阴影。否则点燃过的 Pea 槽位会以 Fireball 身份污染下一发。
2. Animator 组合要镜像 `AnimatedObject` 的并行推进握手：`UpdateParallelDeferred` 成功后串行 `Update` 只清标记，没走并行时才回退 `Animator::Update()`；Draw 直接走 Animator，默认实例化与 `-NoInstance` 都要截图。
3. 存档同时保存不可变 `poolType`、可变“当前类型”和会影响后续转换的防重字段；读档按 `poolType` 走 `BulletPool::AcquireShared`，再恢复当前表现，**不得把 `mFromPool` 改回 false**。专项快照断言 bullet 的 `fromPool=true`、`poolType` 和转换列，防止动画变种读回错误池槽。
4. 同帧 AutoTest 可能先构建行索引再 `spawn_zombie`；`EntityManager::AddZombie/AddZombieWithID` 必须置 `mRowIndexDirty=true`，否则随后同帧的范围弹只看见旧桶。
5. `dump_state` 为类型数量、动画表现和防重状态加聚合整数抓手；不要依赖 `unordered_map` 导出的 `bullets.N` 顺序。运动弹仍断言相对量，不断言绝对 X/Y。
6. `onTriggerStay` 的逐碰撞逻辑帧伤害不受渲染 FPS 波动影响，但固定逻辑步的回调次数不会随 `timeScale` 改变；若直接每次扣固定整数，移动弹在 0.5x/2.0x 下会因重叠帧数反向变化而失衡。默认按目标累计 `damage × GetDeltaTime()/GetFixedStep()` 的小数额度再取整结算，并把每个整数额度分别走一次 1 点正式承伤链，禁止合并成 `TakeDamage(N)` 而改变免伤次数/逐击取整。若特定目标要修改“每帧总基础伤害”，必须先由目标侧虚钩子修正 `damage` 再累计；不能依赖后续普通单击上限，因为拆开的每次输入已经只有 1 点。专项断言 0.5x 60 帧、1x 30 帧、2.0x 15 帧的同目标总伤害完全一致，并覆盖特殊目标的有/无防具两态。
7. 持续命中且限制穿透数的子弹要按稳定实体 ID 记录不同目标：首次接触才登记并播放反馈，`stay` 只继续伤害；达到上限时先让最后目标承伤再回收。目标名单和逐目标小数伤害余额必须一起随存档恢复，并在对象池 `Reset()` 清空；专项至少覆盖“上限减一”目标后的存读档、达到上限消弹、倍速等伤和复用槽位归零，禁止把测试写死在某个穿透数。

这类跨系统植物**不走"简短 spec 直实现"捷径**：回到完整 brainstorm（交互矩阵逐项问主人）→spec→必要时 writing-plans。

## 流程与模板

纯植物侧的小套路：brainstorm 问清关键项→简短 spec 存 `docs/superpowers/specs/`→直接实现（不必单独 writing-plans）。模板：`2026-07-08-scaredyshroom-design.md`。完成并验证后由 Codex 提交，再按仓库风险、工作区状态和上游是否明确决定是否常规 push。

**每次完成并验证任何植物新增或实质修改后，必须在提交前完善本 skill**：把本次实际暴露的新坐标换算、交互契约、foot-gun 或验证手法浓缩进相关章节；已有规则则合并强化，不堆一次性日志。任务同时修改粒子、僵尸或天气时，也同步完善本次实际使用的对应 skill。更新后运行 skill-creator 的 `quick_validate.py` 校验全部改动过的 skill。

## 关联记忆

`[[project_pvz_gamedata_json]]`（权威单份资源）、`[[project_pvz_autotest_suite]]`、`[[reference_pvz_assets_worktree_autotest_gotchas]]`。
