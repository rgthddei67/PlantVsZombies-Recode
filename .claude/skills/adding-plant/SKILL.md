---
name: adding-plant
description: Use when adding ANY new plant (新增植物) to PvZ — 射手/生产/即时消耗/全场结算，包括效果侵入僵尸或其他系统的（冻结/减速/魅惑/穿透/新子弹）— proven on ScaredyShroom (胆小菇) and IceShroom (寒冰菇).
---

# 给 PvZ 新增植物

**最高原则：不确定的事立刻问主人，不要自行推断。** 已知必问项：①**帧事件的帧号**（主人会看动画预览，他有准确答案；从 reanim 活跃区间推断会错——胆小菇实测 25，区间末段推法给 28）②数值/行为想偏离原版时。帧号落在**循环轨末帧**可正常触发（寒冰菇 anim_idle 区间 0..16 挂第 16 帧实测有效）；不可靠的是 PlayTrackOnce-hold 的末帧（舞王教训），两者别混淆。

## 第 0 步：勘察（动手前全部做完）

1. **读 reanim**：`build/clang-release/resources/reanim/<Name>.reanim`，用 Grep `<name>` 提取全部 track 名，`anim_xxx` 即可用动画；`<f>-1/0</f>` 对定位 anim 轨活跃帧区间。
2. **读 C# 参考**：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Plant\Plant.cs`，grep 植物名，读专属 Update 函数 + 发射物类型 + mShootingCounter/state 分支。数值忠实原版。
3. **盘点已就位的基建**（常常提前有了，别重复加）：`PlantType.h` 枚举、`TestDriver.cpp` kPlantNames、`ResourceKeys.h` RKEY、`AnimationTypes.h`、卡片图 `PlantImage/<Name>.png`、reanim 部件图。缺哪补哪。
4. **音效缺失时**：先在 C# 参考里查 `FoleyType.Xxx → Resources.SOUND_XXX`（TodFoley.cs）拿原版文件名，再去原版素材库 `D:\PVZ\中文年度加强版完整版\Test\sounds\` 捞同名 .ogg，拷进**两个 preset** 的 `resources/sounds/`，并在**两个 preset 的 resources.xml** 各加一行 `<Sound>` 条目（寒冰菇 frozen.ogg 实证流程）。

## 实现清单

1. **类**：`Game/Plant/<Name>.h/.cpp`。选基类：蘑菇→`Shroom`（白天睡觉自动处理）；豌豆系带独立头部动画→`Shooter`；其余→`Plant`。抄最像的现有植物结构（喷射蘑菇抄 `PuffShroom`）。
2. **注册**：`GameDataManager.cpp` 加 `#include` + `RegisterPlant(type, "PLANT_X", IMAGE_X, ANIM_X, "ReanimName", &MakePlant<T>)`。卡片由注册表数据驱动，**无需单独加卡**。
3. **gamedata.json**：`{cost, cooldown, offset, scale}` 条目，**两个 preset 的 `build/<preset>/resources/` 都要加**（缺任一字段拒启动 exit -6）。
4. **资源同步**：reanim + 贴图通常只放进了 clang-release——**必须 Copy-Item 同步到 msvc-debug**（资源不在 git，只活在 exe 旁）。
5. **射击类惯例**：帧事件发弹（帧号问主人）+ `mShootTimer` 乘词条攻速 `GetPlantAttackSpeedMultiplier()` + 索敌用 `ForEachZombieInRow`（严禁全表扫）。跨行/范围判定注意坐标换算：**C# 的 mX/mY 是格子左上角，本项目 `GetPosition()` 是 80×100 格子中心**，差 (40,50)。
6. **动画状态机**：一次性→循环用 `PlayTrackOnce(track, returnTrack)` 自动接轨，完成信号 = `GetCurrentTrackName()` 变成 returnTrack（天然兼容存读档，勿自造 loop 计数）。

## 存读档心智清单（AutoTest 测不到，只能靠脑内过一遍）

AutoTest 模式存档读写被短路 = **盲区**；写完必须逐条自查，最后请主人手动"退出重进"验证：

- 自定义状态（状态机枚举、计时器）→ `SaveExtraData/LoadExtraData`。
- 动画轨道/PlayTrackOnce 进行态：`GameInfoSaver::RestoreAnimState` 已统一恢复，**不用自己存**；`LoadExtraData` 在其后运行可放心覆盖。
- **凡是被状态机消费的节流缓存，读档第一帧不得吃初值**——缓存初值=给读档后的世界注入捏造状态（胆小菇实测：`mScaredCached=false` 初值让 SCARED 态读档后误判"僵尸走了"先伸头再缩回）。修法=计时器初始即到期，首帧强制真算。

## 验证（缺一不可）

1. `clang-release` 构建 0 warning（唯一报全量警告的配置）。
2. **站位+影子截图校对**（写完必做，别等主人指出）：临时脚本把新植物与小喷菇/向日葵种同一行，截图比脚底基线。两套独立坐标：**本体 = gamedata.json 的 offset**（改无需重编译）；**影子 = 代码里 `ShadowComponent::SetOffset`**（改要重编译）。抄同类植物的值大概率不准。
3. **AutoTest 冒烟**：`autotest/scripts/smoke_<name>.json`，每阶段**只种一棵**（plants dump 顺序来自 unordered_map，多棵时下标不可靠），断言 `plants.0.track`；时序估算用僵尸判定矩形 `[x±25]×[y-65,y+35]`、步速 23~45px/s。**exit 0 ≠ 通过**：必须逐张 Read 截图 + dump 数值核对（防假绿）。
4. 蘑菇夜测用 level 10-19；白天睡觉断言 `anim_sleep`；魅惑僵尸清场用 `charm_zombie`（不触发输局）。

## 特性侵入其他系统时（寒冰菇冻结、魅惑、穿透这类）

纯植物侧的清单不够用了，先按效果落点归类，逐类有先例可抄：

| 落点 | 先例 | 关键点 |
|---|---|---|
| **僵尸新状态效果**（冻结/减速/魅惑…） | 减速=SnowPea→`Zombie::SetCooldown`；冻结=IceShroom→`StartFrozen`；魅惑=HypnoShroom→`StartMindControlled` | 见下方专属清单 |
| **全场即时结算**（寒冰菇式） | `IceShroom`（帧事件→音效+白闪+逐行结算→`Die()`，仿 CherryBomb 骨架） | 全屏特效走 GameScene `ShowScreenFlash`/RegisterDrawCommand；**isPreview 特判否则图鉴也结算**（主人叮嘱） |
| **新子弹** | `Game/Bullet/PuffBullet.h` | `BulletType` 枚举 + BulletPool 对象池 + 命中粒子 `EmitEffect` |
| **TakeDamage 类钩子** | FumeShroom 的 `penetrateShield` 参数 | 穿透只对二类护盾（门/报纸），不穿头盔；改签名先看全部调用点 |
| **即时/范围结算** | CherryBomb/大喷菇锥形 | 帧事件触发结算帧（帧号问主人），范围判定用行桶不全扫 |

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

这类跨系统植物**不走"简短 spec 直实现"捷径**：回到完整 brainstorm（交互矩阵逐项问主人）→spec→必要时 writing-plans。

## 流程与模板

纯植物侧的小套路：brainstorm 问清关键项→简短 spec 存 `docs/superpowers/specs/`→直接实现（不必单独 writing-plans）。模板：`2026-07-08-scaredyshroom-design.md`。commit 由 Claude 做、push 等主人发话。

## 关联记忆

`[[project_pvz_gamedata_json]]`（双 preset foot-gun 起源）、`[[project_pvz_autotest_suite]]`、`[[reference_pvz_assets_worktree_autotest_gotchas]]`。
