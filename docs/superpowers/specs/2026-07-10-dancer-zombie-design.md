# 舞王僵尸(Zombie_Jackson) + 伴舞僵尸(Zombie_dancer) 设计 spec

日期：2026-07-10 ｜ 状态：主人已批准架构，待实现
参考：原版 C# `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Zombie\Zombie.cs`（注意：C# 实现的是新版 disco 舞王，本项目用主人准备的初代 MJ 版 reanim，状态机相同、断肢方案不同，见 §5）

## 0. 背景与素材现状

- reanim 已就绪：`resources.xml` 已注册 `ZombieJackson` → `Zombie_Jackson.reanim`、`ZombieDancer` → `Zombie_dancer.reanim`。
- 枚举已预置：`ZombieType.h` 有 `ZOMBIE_DANCER`（舞王）/`ZOMBIE_BACKUP_DANCER`（伴舞），但位于 `NUM_ZOMBIE_TYPES` 哨兵**之后**，必须移到之前才能出怪。
- 动画类型已预置：`AnimationTypes.h` 的 `ANIM_DANCE_ZOMBIE`、`ANIM_DANCERWITH_ZOMBIE`。
- 名表已含：`kZombieNames`(TestDriver.cpp)、`kDevZombieTable`(GameScene.cpp) 均已有这两项，无需改。
- 掉头粒子贴图已备：`PARTICLE_ZOMBIE_DANCERHEAD`（舞王头）、`PARTICLE_ZOMBIE_BACKUPDANCERHEAD`（伴舞头）（`_disco` 后缀两张不用）。
- 无聚光灯贴图、无出土音效素材 → 均不做（§8）。

### reanim 轨道事实（实测）

Zombie_Jackson（时间线 0–147，12fps）：
- 剪辑轨：`anim_moonwalk` 0–26 ｜ `anim_point` 27–36 ｜ `anim_walk` 46–66 ｜ `anim_armraise` 67–77 ｜ `anim_eat` 78–111 ｜ `anim_death` 112–147
- 头部件轨：`anim_head1`（头）、`anim_head2`（下巴，共用 `IMAGE_REANIM_ZOMBIE_JAW`）、`anim_hair`。**无 anim_tongue**。
- 外臂三段：`Zombie_Jackson_outerarm_upper` / `Zombie_outerarm_lower` / `Zombie_outerarm_hand`（后两个无 Jackson 前缀）。
- **无断臂残肢(_bone/upper2)轨道**——这决定了断手方案（§5）。

Zombie_dancer（时间线 0–101，12fps）：
- 剪辑轨：`anim_walk` 0–20 ｜ `anim_armraise` 21–31 ｜ `anim_eat` 32–64 ｜ `anim_death` 65–101。**无 moonwalk/point**。
- 部件轨复用 Jackson 前缀名（贴图指向 DANCER 图）；额外 `anim_earing`（耳环）、`Zombie_dancer_belt`。

## 1. 全队共舞同步 —— Board 节拍时钟（方案A，已批准）

- `Board` 新增 `int mBoardFrame = 0;`，固定步长 Update 每逻辑步 +1，**入存档**。
- 节拍帧 `danceFrame = mBoardFrame % (12 * 23) / 12`，值域 0~22，每拍 12 逻辑步 = 0.2s（等价原版 100Hz 下 20 tick/拍）。
- 舞王与所有伴舞由同一函数把节拍帧映射到动画 → 全队整齐划一；领队死亡不影响伴舞节拍；读档后依然同步。
- 动画映射：节拍 0~11 → `anim_walk`（舞步）循环；12~22 → `anim_armraise`（举手）循环；切换时 blend（沿用仓库统一 0.3f 风格，除非视觉不佳再调）。

## 2. 舞王 DancerZombie

文件：`Game/Zombie/DancerZombie.{h,cpp}`，参考 PaperZombie 结构。

数值（SetupZombie 硬编码）：HP `mBodyHealth = 500`，速度同普通僵尸，无头盔无护盾。

状态机 `enum class DancerPhase { DANCING_IN, SNAPPING, HOLD, DANCING }`：

| 阶段 | 行为 | 转移 |
|---|---|---|
| DANCING_IN | 循环 `anim_moonwalk` 向左推进，计时 2.0 + rand()*0.12 秒 | 计时到 → SNAPPING；**开始啃食 → 立即置计时=0 提前 snap**（原版行为） |
| SNAPPING | 停止移动，`PlayTrackOnce("anim_point")` | 播完 → 执行 SummonBackupDancers() → HOLD(2s) |
| HOLD | 定格 2 秒 | 计时到 → DANCING |
| DANCING | 恢复移动，按 §1 节拍映射切 walk/armraise | （驻留；见补充召唤） |

- **补充召唤**：DANCING 中每帧检查——存在空位（`EntityManager::GetZombie(mFollowerID[i]) == nullptr` 且该位行合法）&& `mHasHead` && `x < DANCE_LIMIT_X`（取 700，可调）&& 节拍帧 == 12 → 回到 SNAPPING 重播 `anim_point`，播完只补空位。
- **啃食**：`mIsEating` 时状态机整体暂停（update 首行 guard，原版同）；啃食动画走基类。
- **无头**：`mHasHead == false` 后永不再召唤，其余走基类流血死亡。
- 帧事件：`EatTarget` @ **90、99**（repeating）；`Die()` @ **146**。（帧号主人指定）

## 3. 伴舞 BackupDancerZombie

文件：`Game/Zombie/BackupDancerZombie.{h,cpp}`。

数值：HP `mBodyHealth = 270`，速度同普通僵尸，无防具。gamedata.json `weight = 0` → 永不被波次抽中，只能被召唤（AutoTest `spawn_zombie` 仍可直接生成）。

状态机 `enum class BackupPhase { RISING, DANCING }`：
- **RISING**（出生）：`mVisualOffset.y = +145` 起（沉入地下），2.5 秒线性升到 0。期间不移动、不主动啃食；**动画不定格、照常随节拍跳舞**（实现修订：原版是定格姿势升起，但本引擎暂停骨架后若升起中被打死/断头会卡冻结帧无法播 anim_death，跳着舞升起是安全方案；视觉观感由主人验收裁决）。**无出土粒子、无音效**（主人已定）。穿模不裁剪，主人看效果后再议。
- **DANCING**：与舞王共用节拍映射，正常向左推进、可啃食。
- 帧事件：`EatTarget` @ **46、59**（repeating）；`Die()` @ **100**。（帧号主人指定）

## 4. 召唤逻辑（舞王侧 SummonBackupDancers）

十字型 4 位（**非四角**），slot i 只在空位时召唤：

| slot | 行 | x | 特殊 |
|---|---|---|---|
| 0 | mRow − 1 | 舞王 x | 行越界（<0）→ 永久空置 |
| 1 | mRow + 1 | 舞王 x | 行越界（≥mRows）→ 永久空置 |
| 2 | mRow | 舞王 x − 100 | 舞王 x < 130 时跳过（防出左屏） |
| 3 | mRow | 舞王 x + 100 | — |

- 走 `mBoard->CreateZombie(ZOMBIE_BACKUP_DANCER, row, x)`（y 由 row 派生，引擎既有约定）。
- 创建后：伴舞记 `mLeaderID`，舞王记 `mFollowerID[i]`（均 EntityManager 整型 ID；死亡后 `GetZombie` 自动返回 nullptr，即天然空位，无需死亡回调）。
- 舞王被魅惑状态下召唤的伴舞：创建后补调 `StartMindControlled()` 继承阵营。
- **舞王死后**：伴舞照常按节拍跳舞前进（原版即如此，不转换为普通僵尸——C# 的 `ConvertToNormalZombie` 从未被调用）。

## 5. 断手 / 断头（主人已拍板）

背景：C# disco 版断手 = 隐藏整条外臂 + 显示 reanim 自带的 `_upper_bone` 残肢轨道，**无材质替换**。MJ 版 reanim 无 bone 轨道，故采用 disco 伴舞式方案：

| 事件 | 隐藏轨道 | 材质替换 | 粒子 | 音效 |
|---|---|---|---|---|
| 断手（两者同） | `Zombie_outerarm_lower`、`Zombie_outerarm_hand` | **无**（`Zombie_Jackson_outerarm_upper` 大臂原样保留当残肢） | **无** | 基类 `arm_head_drop` |
| 舞王断头 | `anim_head1`、`anim_head2`、`anim_hair` | 无 | 新建 `ZombieJacksonHeadOff.xml`（图 `PARTICLE_ZOMBIE_DANCERHEAD`） | 基类 |
| 伴舞断头 | 同上三轨 + `anim_earing` | 无 | 新建 `ZombieBackupDancerHeadOff.xml`（图 `PARTICLE_ZOMBIE_BACKUPDANCERHEAD`） | 基类 |

- 两类均覆写 `ArmDrop()` / `HeadDrop()`（不调基类的 SetTrackImage/粒子部分，自行组合）。
- 两类均覆写 `ZombieItemUpdate()`：按 `mHasArm/mHasHead` 重建轨道可见性（读档残肢恢复；DoorZombie.h 模式）。
- 粒子 XML 照抄 `ZombieHeadOff.xml` 改 `<Name>` 与 `<Image>`，放入 `particles/config/`（整目录自动扫描，**两个 preset 都放**）。

## 6. 魅惑交互（照原版）

- **舞王被魅惑**：清空 `mFollowerID[4]`（放弃旧伴舞，它们继续以敌方身份跳舞）；此后新召唤伴舞继承魅惑（§4）。实现机制（不改基类，`StartMindControlled` 非虚）：舞王自持 `bool mCharmHandled = false;`，`ZombieUpdate` 里检测魅惑边沿——`IsMindControlled() && !mCharmHandled` → 清空 follower 数组并置位。存档由基类 `mIsMindControlled` + 自存 `mCharmHandled` 恢复。
- **伴舞被魅惑**：同样的边沿检测，清自己 `mLeaderID` 脱队（原领队检测到空位后按 §2 规则补人）。
- 两者 `CanBeCharmed()` 默认 true 不覆写。

## 7. 存档

- 舞王 `SaveExtraData/LoadExtraData`：`phase`、`phaseTimer`、`followerID[4]`、`hasSummoned`。读档链中 ID 经 `CreateZombieWithID` 保值，交叉引用安全（死者返回 null）。
- 伴舞：`phase`、`risingRemain`、`leaderID`。
- Board：`mBoardFrame` 入档（节拍连续）。
- `LoadExtraData` 首行 `if (mIsEating) return;` 后按 phase 恢复动画（Polevaulter 模式）；RISING 中读档要恢复 `mVisualOffset.y`。

## 8. 明确不做（v1 简化）

- 聚光灯（disco 版特效，无素材）。
- disco 循环音乐 foley 与"≤15 僵尸才放音乐"逻辑（无素材）。
- 伴舞出土粒子与音效（主人砍）。
- 冻结/黄油致全队节拍定格、减速全队传播（本仓库尚无冻结机制，留待寒冰菇时代）。
- 升起期间地面裁剪（先看穿模效果再议）。

## 9. 落地改动清单

1. `ZombieType.h`：`ZOMBIE_DANCER`/`ZOMBIE_BACKUP_DANCER` 移到 `NUM_ZOMBIE_TYPES` 之前。
2. 新建 `Game/Zombie/DancerZombie.{h,cpp}`、`BackupDancerZombie.{h,cpp}`（GLOB_RECURSE 自动收编，无需改构建文件）。
3. `GameDataManager.cpp`：`RegisterZombie(ZOMBIE_DANCER, "ZOMBIE_DANCER", ANIM_DANCE_ZOMBIE, "ZombieJackson", ...)` + 伴舞一条（`ANIM_DANCERWITH_ZOMBIE`, `"ZombieDancer"`）+ 两个 include。
4. `gamedata.json` ×2 preset：两条完整 5 字段条目（weight/appearWave/survivalRound/offset/scale），伴舞 `weight=0`（缺字段启动即拒，注意别漏）。
5. 粒子 XML ×2 ×2 preset（§5）。
6. `Board.h/.cpp` + `GameInfoSaver`：`mBoardFrame` 计数与存读档。
7. AutoTest：`smoke_dancer.json`——spawn 舞王 → 等响指召唤 → `assert_state` 僵尸数 ≥5 → 截图验全队与断肢；魅惑分支断言。

## 10. 验收

- 舞王入场月球漫步 → 打响指 → 十字 4 伴舞升起 → 全队齐舞（截图目测同拍）。
- 打死一个伴舞，舞王在界限内于节拍 12 重新打响指补位。
- 断手：小臂+手消失、大臂保留、无粒子；断头：头/下巴/头发（伴舞加耳环）消失、对应头图粒子飞出。
- 存档读档：phase/伴舞关联/节拍/残肢均恢复（注意 AutoTest 存档短路，读档路径需真人或专项验证）。
- 魅惑舞王：新召唤伴舞为友方；魅惑伴舞：脱队且原队补人。
