# 寒冰菇 (Ice-shroom) 设计

日期：2026-07-14　状态：交互矩阵已获主人拍板

## 目标

新增经典植物寒冰菇：种下后一次性冻结全场僵尸（完全定身 + 减速尾巴 + 少量伤害），随即消失。
数值忠实原版（参照 C# `Plant.cs::IceZombies` + `Zombie.cs::HitIceTrap/ApplyChill`）。

## 主人拍板的决策（2026-07-14）

1. **触发**：不播 anim_face，就播 `anim_idle`，**第 16 帧**帧事件触发全场结算（帧号主人指定，
   已按代码口径 −1 过，`AddFrameEvent(16)` 直用）。
   **必须 isPreview 特判**——否则图鉴/预览也会触发。
2. **数值**：忠实原版——首冻 4~6s 随机、已减速/已冻再冻 3~4s、附带 20s 减速尾巴（解冻后继续慢走）、
   每僵尸 20 点伤害、75 阳光、50s 冷却。
3. **持盾僵尸（铁门/报纸类）**：冻结冻所有（含持盾），减速尾巴仍走现有 `SetCooldown`（其持盾守卫保留
   ——持盾免疫减速，两套机制各自自洽）。
4. **音效/特效**：音效用原版 `frozen.ogg`（已从 D:\PVZ\中文年度加强版完整版\Test\sounds 拷入两 preset 的
   `resources/sounds/Plant/`，键 `SOUND_FROZEN`，**resources.xml 两 preset 都要加条目**）；种下结算瞬间做
   **简单全屏白闪**（走 GameScene RegisterDrawCommand，仿 ShowPrompt 模式）；**解冻特效不做**（主人明确）。
5. **icetrap.png**（image 根目录，79×45 冰晶簇）：冻结期间绘制在僵尸**脚底**（Zombie::Draw 内
   `g->DrawTexture`，画在本体之后=前景，位置截图后微调）。

## 免疫/交互矩阵（沿原版语义映射到本项目）

| 对象/状态 | 冻结 | 减速尾巴 |
|---|---|---|
| 普通/路障/铁桶/快速类 | ✅ | ✅ |
| 持盾（铁门、报纸的报纸不算盾——按 mShieldType 判） | ✅ | ❌（SetCooldown 持盾守卫） |
| 魅惑僵尸 | ❌（原版 CanBeChilled 排除 mMindControlled） | ❌ |
| 出土中的伴舞（DancerRising 等价态） | ❌ | ❌ |
| 撑杆跳跃中（CanBeFrozen 排除、chill 照上） | ❌ | ✅ |
| 濒死/已死/预览僵尸 | ❌ | ❌ |
| 冻结期间新刷出的僵尸 | 不受影响（原版草地行为） | 不受影响 |

- 舞王/伴舞：冻结期间动画停格 → 帧事件/召唤/齐舞自然停；解冻后按 Board 节拍钟回到齐舞
  （节拍钟是 Board 级、按 dt 折算，冻结个体不影响全局钟）。
- 正在啃食的僵尸：动画停格 → 啃食帧事件不再触发（伤害停），mIsEating 状态保留，解冻继续啃。
- 冻结中被打死：死亡路径必须先清冻结（恢复动画速度），否则死亡动画停格（入口/出口清单见下）。

## 僵尸侧实现（Zombie.h/.cpp）

- 字段 `float mFrozenTimer = 0.0f`（protected，默认中性）+ `IsFrozen()` getter。
- 唯一入口 `bool StartFrozen()`（完全镜像 HitIceTrap，**含 20 点伤害**——原版伤害在免疫判定之后，
  魅惑/跳跃中撑杆连伤害也不吃；实现时修正了最初"植物侧无差别结算伤害"的偏差）：
  ```
  wasSlowedOrFrozen = (mCooldownTimer>0 || mFrozenTimer>0)
  SetCooldown(20.0f)                      // 减速尾巴；持盾守卫在 SetCooldown 内部生效
  if (!CanBeFrozen()) return false        // 虚函数守卫（魅惑/濒死/预览基类判；撑杆跳跃中子类覆写）
  mFrozenTimer = wasSlowedOrFrozen ? Range(3,4) : Range(4,6)
  mAnimator->SetExtraSpeedMultiplier(0)   // 动画停格（extra 层，PlayTrack/SetSpeed 覆盖不掉）
  ```
- 行为守卫放虚函数 `CanBeFrozen()`（不放 lambda）：基类判 `!mIsDying && !mIsMindControlled && !mIsPreview`；
  撑杆（跳跃中）、伴舞（出土中）覆写补充。
- Update：`mFrozenTimer` 用**真实 deltaTime** 倒数（同减速滴答）；冻结期间 return 掉
  ZombieMove/ZombieUpdate（在无头流血之后——流血照走，原版确定性死亡语义）。
- 出口（解冻/各入口清单，逐一同步动画速度）：
  - 超时解冻：`mCooldownTimer>0 ? extra=mExtraSpeed*0.6 : extra=mExtraSpeed`；
  - 死亡（TakeBodyDamage→死亡动画 / Die）：先清 mFrozenTimer + 恢复 extra，死亡动画才会动；
  - 被魅惑 StartMindControlled：清冻结（原版魅惑免疫 chill/freeze）；
  - 断头：不解冻（原版无头冻僵尸保持冻结，流血照走）；
  - 读档：mFrozenTimer 进 `Save/LoadProtectedData`，Load 时 >0 则重新停格动画（在 cooldown 恢复之后覆盖）。
- 绘制：`Zombie::Draw` 内 `mFrozenTimer>0` 时画 icetrap.png（RKEY `IMAGE_ICETRAP`）。
- dump_state：加 `frozen`（bool，供 assert_state）+ `frozenTimer`（float，仅肉眼核对，勿 equals 断言）。

## 植物侧实现（Game/Plant/IceShroom.h/.cpp）

- `IceShroom : Shroom`；`SetupPlant()`：先 `Shroom::SetupPlant()`（白天睡觉），`if (mIsPreview) return;`
  再挂 anim_idle 第 16 帧帧事件（白天睡着播 anim_sleep 不经过 idle 第 16 帧，天然不触发；无咖啡豆无唤醒路径）。
- 帧事件体：`AudioSystem::PlaySound(SOUND_FROZEN)` → 全屏白闪 → 行 0..4 `ForEachZombieInRow`：
  先 `zombie->TakeDamage(20)` 再 `zombie->StartFrozen()`（顺序：伤害在先可触发报纸狂暴等，再冻——
  原版 HitIceTrap 内 TakeDamage 在设置计数器之后，但先伤后冻等价且死亡的僵尸 StartFrozen 会被
  CanBeFrozen 的濒死守卫挡下）→ `Die()`。
- 行索引已排除失活/濒死；魅惑僵尸在 CanBeFrozen 内被挡（减速尾巴对魅惑也应跳过——SetCooldown 需
  确认/补魅惑守卫，魅惑后本就打不中子弹但寒冰菇是无差别结算）。

## 白闪（GameScene）

`GameScene` 加 `ShowScreenFlash(float duration≈0.5f)`：白色全屏 `DrawRect`，alpha 255→0 线性衰减，
RegisterDrawCommand 挂 LAYER_UI 上方（仿 Prompt 命令）。植物经 `mBoard->mGameScene` 触发。

## 注册/数值

- `RegisterPlant(PLANT_ICESHROOM, "PLANT_ICESHROOM", IMAGE_ICESHROOM, ANIM_ICE_SHROOM, "IceShroom", &MakePlant<IceShroom>)`
- gamedata.json（两 preset）：`"PLANT_ICESHROOM": { cost:75, cooldown:50.0, offset 参照胆小菇截图微调, scale:1.0 }`
- ResourceKeys：`RKEY(SOUND_FROZEN)`、`RKEY(IMAGE_ICETRAP)`；AnimationTypes 已有 `ANIM_ICE_SHROOM`。
- resources.xml（两 preset）：`<Sound>./resources/sounds/Plant/frozen.ogg</Sound>`。

## 验收（AutoTest，夜间关 goto 10-19，-Seed 固定）

`autotest/scripts/smoke_iceshroom.json`：
1. 刷普通+铁门僵尸 → 种寒冰菇 → 截图（icetrap 冰晶在脚底、僵尸停格）+ assert `zombies.*.frozen==true`、
   植物已消失、僵尸掉 20 血（断整数血量字段）；
2. 等 3~4s 未解冻中间截图；等 >6s 解冻 → assert `frozen==false`、普通僵尸 `slowCooldown>0` 用
   dump 肉眼核对（浮点勿 equals）、铁门 `slowCooldown==0.0`→ 用 frozen/track 断言代替；
3. `charm_zombie` 后再种一颗 → assert 魅惑僵尸 `frozen==false`；
4. 白天关种下 → assert `plants.0.track=="anim_sleep"` 且不触发。
读档验证 AutoTest 测不到：请主人手动「冻结进行中存档→退出重进」核对停格+冰晶+剩余时长。
