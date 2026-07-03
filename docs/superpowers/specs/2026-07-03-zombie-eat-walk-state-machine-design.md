# 僵尸「啃食 → 走路」状态机重构设计

日期：2026-07-03
状态：设计已批准，待落实现计划

## 1. 背景与动机

僵尸基类 `Zombie` 的「啃完回走路」逻辑目前由两个语义重叠的虚函数承担：

- `WalkTrackAfterEat() const → const char*`：只返回走路轨道名（浅层钩子）。
- `ResumeWalkAfterEat(float blendTime)`：实际播轨道 + 副作用（深层钩子）。

同时「回走路」这一动作在基类里散落在 **5 个触发点**，其中两处**硬编码** `PlayTrack("anim_walk2", …)` 而没有走虚函数：

| 触发点（`PlantVsZombies/Game/Zombie/Zombie.cpp`） | 现状 |
|---|---|
| `StopEat` 啃僵尸分支 (669) | `ResumeWalkAfterEat(0.2f)` ✓ |
| `StopEat` 啃植物分支 (682) | **硬编码 `anim_walk2`** ✗ |
| `EatTarget` 目标没了 (577) | `ResumeWalkAfterEat(0.2f)` ✓ |
| `StartMindControlled` (390/397) | `ResumeWalkAfterEat` ✓ |
| `ValidateEatingState` 植物没了 (742) | **硬编码 `anim_walk2`** ✗ |
| `ValidateEatingState` 啃僵尸分支 (751) | `ResumeWalkAfterEat(0.3f)` ✓ |

### 病根

因为 682/742 焊死了 `"anim_walk2"`，任何 reanim 里没有该轨道的僵尸（`PaperZombie`、`Polevaulter`——`PlayTrack` 打不存在的轨道是**静默失败**）都被迫**整段复制** `StopEat` 的植物分支和 `ValidateEatingState`，只为换掉那一个字符串。这就是设计臃肿、且「继承来继承去容易漏」的根源。

### 子类审计矩阵（重构前，8 个僵尸子类）

| 类 | reanim | 有 `anim_walk2`? | 覆写啃食/走路虚函数 | 现状 |
|---|---|---|---|---|
| `Zombie`(普通) | NormalZombie | ✅ | 基类本体 | 安全 |
| `ConeZombie` | ConeZombie | ✅ | 无 | 安全 |
| `BucketZombie` | BucketZombie | ✅ | 无 | 安全 |
| `FastBucketZombie` | BucketZombie(复用) | ✅ | 无(啃食侧) | 安全 |
| `DoorZombie` | DoorZombie | ✅ | Resume/Validate/Start/Stop(为手臂) | 安全 |
| `PaperZombie` | PaperZombie | ❌ 无 | Resume/Validate/Start/Stop | 安全(必须覆写) |
| `FastPaperZombie` | PaperZombie(复用) | ❌ 无 | 啃食侧一个都没覆写 | 安全**——纯靠运气** |
| `Polevaulter` | Polevaulter | ❌ 无 | WalkTrack/Validate/Start/Stop | 安全 |
| `ZombieCharred` | Zombie_Charred | 不适用 | 非 Zombie 子类(烧焦残骸特效) | 无关 |

**结论：当前无立即崩溃的 bug**，但安全性靠一条**反向的隐性不变量**撑着（「要么 reanim 有 walk2，要么整段覆写」），且两级继承 `FastPaperZombie` 只因「恰好没覆写」而安全。两颗潜在地雷：
- **地雷一**：未来加只有 `anim_walk` 的新僵尸，忘覆写 `ValidateEatingState` → 读「正啃植物已死」的存档时基类 742 播不存在的 walk2 → 静默卡死。
- **地雷二**：`FastPaperZombie` 将来若加自己的 `StopEat` 并误写 `Zombie::StopEat`（而非 `PaperZombie::StopEat`）→ 命中基类硬编码 walk2 → 卡死。

## 2. 目标

1. 把「这只僵尸此刻怎么走路」收敛成**唯一权威**，所有走路站点都调它——从结构上根除「漏一处就卡死」。
2. 把「啃食视觉残留」（当前仅铁门的手臂）收敛成**一对对称钩子**，消灭主人已修 3 次的门臂 bug 的结构成因。
3. 删掉三个子类各自复制的整段覆写（约 6 个函数）。
4. 行为逐条等价（唯一有意变更见 §6）。
5. 每个新虚函数带「用途 + 示例」注释，让未来扩展一目了然。

非目标（YAGNI）：不引入 `PlayEatAnimation`（啃食动画轨道选择）——当前只有 PaperZombie 需要，且它 `StartEat` 还有 gasp 守卫必须保留，收益不足；不改碰撞/存档格式；不动 C 组「初始起步随机」站点。

## 3. 新契约（写进 `PlantVsZombies/Game/Zombie/Zombie.h`）

```cpp
// ═══ 啃食 → 走路 状态机：扩展新僵尸只需覆写下面带 ★ 的 virtual，永不碰 ResumeWalkAfterEat ═══

// ★关注点A｜"我此刻怎么走路"的唯一权威：播哪条稳态走路轨道 + clip 速度（须确定性，勿随机）。
//   啃完回走路 / 撑杆落地 / 读档恢复 全经它，所以"改走路动画"永远只改这一处。
//   何时覆写：reanim 没有 anim_walk2，或走路轨道随状态变。
//   例（读报僵尸·狂暴撕报后换轨道并带 clip 速度）：
//     void PlayWalkAnimation(float blend) override {
//         if (mHasNewspaper) PlayTrack("anim_walk", 0.0f, blend);
//         else               PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blend);
//     }
virtual void PlayWalkAnimation(float blendTime = 0.0f);

// ★关注点B｜啃食视觉残留（一对对称钩子，默认空操作，绝大多数僵尸不用管）：
//   OnStartEating 一开吃触发、OnStopEating 一停吃触发。在 StartEat 里改过的视觉状态，在这对里对称还原。
//   例（铁门僵尸·啃食露常规手臂、收尾藏回门后，门还在才动）：
//     void OnStartEating() override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(true);  }
//     void OnStopEating()  override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(false); }
virtual void OnStartEating() {}
virtual void OnStopEating()  {}

// 模板方法（非虚，勿覆写）：啃完回走路 = 先收尾、再走路。执行顺序由基类锁死。
void ResumeWalkAfterEat(float blendTime) { OnStopEating(); PlayWalkAnimation(blendTime); }
```

- **删除** `WalkTrackAfterEat()`（职责被 `PlayWalkAnimation` 吸收）。
- `ResumeWalkAfterEat` 从 `virtual` 改为**非虚模板方法**：子类永不覆写，只填两个 ★ 钩子。

### 设计原则

两个正交关注点，各有单一钩子，无「该覆写哪个」歧义：
- **A｜怎么走路**（轨道 + clip 速度）→ `PlayWalkAnimation`。
- **B｜啃食视觉残留**（show/hide 对称）→ `OnStartEating` / `OnStopEating`。

`ResumeWalkAfterEat` 作为非虚骨架锁死「先收尾后走路」的顺序 → 防漏从「靠人记得」升级为「结构上不可能漏」。

## 4. 调用点收敛

### A 组｜六个「啃食结束」站点 → 全部统一走 `ResumeWalkAfterEat(blend)`
把现有两处硬编码收编：
- `Zombie.cpp:682`（StopEat 植物分支）：`PlayTrack("anim_walk2", 0.0f, 0.2f)` → `ResumeWalkAfterEat(0.2f)`
- `Zombie.cpp:742`（ValidateEatingState 植物没了）：`PlayTrack("anim_walk2", 0.0f, 0.3f)` → `ResumeWalkAfterEat(0.3f)`
- 其余 4 处（669/577/390/397/751）已经在调 `ResumeWalkAfterEat`，不变。

→ 收编后 `OnStopEating` 必然在**每一条**啃食结束路径触发，门臂「漏藏」结构性根除。

### B 组｜三个「稳态走路」站点 → 改调 `PlayWalkAnimation()`（行为等价）
- `Polevaulter::EndJump` (124)：`SetAnimationSpeed(...)` 后 `PlayTrack("anim_walk")` → `PlayWalkAnimation()`
- `Polevaulter::LoadExtraData` WALKING 恢复 (191)：`PlayTrack("anim_walk")` → `PlayWalkAnimation()`
- `PaperZombie::LoadExtraData` 狂暴脱困 (125)：`PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.0f)` → `PlayWalkAnimation(0.0f)`

### C 组｜三个「初始起步变化」站点 → 保持不动（有意随机，与稳态走路是不同美术意图）
- `Zombie::SetupZombie` 随机 walk/walk2 (76-79)
- `DoorZombie::SetupZombie` 随机 walk/walk2 (20-23)
- `PaperZombie::SetupZombie` 起步 walk

### `OnStartEating` 触发点（契约：任何「开吃」处都必须触发一次）
在基类 `Zombie::StartEat` 的两个分支（啃僵尸 / 啃植物）置 `mIsEating = true` 之后，仅「本次真正开吃」调用一次。实现方式：函数顶部捕获 `const bool wasEating = mIsEating;`，两分支末尾统一 `if (!wasEating && mIsEating) OnStartEating();`（复刻 DoorZombie 现有 `!wasEating && mIsEating` 守卫，覆盖植物/僵尸两入口）。

**普适性缺口与修补**：`OnStopEating` 因所有停吃路径都过非虚 `ResumeWalkAfterEat` 而**必然触发**；但 `OnStartEating` 从基类 `StartEat` 触发，凡是**完全覆写了「开吃」路径、不回调 `Zombie::StartEat`** 的子类会绕过它。当前唯一这种情形是 `PaperZombie::StartEat` 的**啃植物分支**（自定义 `anim_eat_nopaper`，不调基类）。为让契约「开吃必触发」保持普适，`PaperZombie` 在该分支的「真开吃」处补一行 `OnStartEating();`。今天纸僵尸无残留、此调用是 no-op（行为不变），但它保证将来给纸僵尸加残留时钩子不漏。（`PaperZombie::StartEat` 的**啃僵尸分支**调 `Zombie::StartEat`，天然覆盖；`Polevaulter::StartEat` 亦调基类，天然覆盖。）

## 5. 每个子类改动 + 删除清单

| 类 | 新增覆写 | 删除（回归基类） | 保留 |
|---|---|---|---|
| `Zombie`(基类) | `PlayWalkAnimation`(播 anim_walk2, clip=0)、非虚 `ResumeWalkAfterEat`、空 `OnStartEating`/`OnStopEating`；`StartEat` 加 `OnStartEating` 触发 | `WalkTrackAfterEat` | — |
| `ConeZombie` / `BucketZombie` / `FastBucketZombie` | 无 | 无 | 现状（reanim 有 walk2，吃基类） |
| `Polevaulter` | `PlayWalkAnimation`(播 anim_walk) | `WalkTrackAfterEat`、`ValidateEatingState`、`StopEat` | `StartEat`(RUNNING 守卫)；EndJump/Load 改调 `PlayWalkAnimation` |
| `PaperZombie` | `PlayWalkAnimation`(= 旧 `ResumeWalkAfterEat` 体：walk / walk_nopaper+clip) | `ValidateEatingState`、`StopEat`、旧 `ResumeWalkAfterEat` | `StartEat`(gasp 守卫；植物分支补一行 `OnStartEating()` 保持钩子普适)；Load 改调 `PlayWalkAnimation` |
| `FastPaperZombie` | 无 | 无 | 继承 `PaperZombie::PlayWalkAnimation`（两级链结构性安全） |
| `DoorZombie` | `OnStartEating`(ShowArm true)、`OnStopEating`(ShowArm false) | `ValidateEatingState`、`StopEat`、`StartEat`、旧 `ResumeWalkAfterEat` | SetupZombie / HeadDrop / ArmDrop / ZombieItemUpdate 等 |

**净删除：约 6 个整段覆写函数**，由「基类模板方法 + 各自一两行钩子」取代。

## 6. 行为兼容性（逐条自证等价）

- **撑杆读档「植物已死」**：旧 `PlayTrack("anim_walk", 0.0f, 0.2f)` → 新 `ResumeWalkAfterEat(0.3f)` → `PlayWalkAnimation` → `anim_walk`。**唯一有意变更：混合时间 0.2 → 0.3**（读档瞬间多 0.1s 过渡，已获主人认可）。
- **铁门各啃食结束路径**：`OnStopEating`(ShowArm false, 门在守卫) + `PlayWalkAnimation`(walk2) 与旧代码逐语句对齐；`ShowArm` 与 `PlayTrack` 触及不同轨道，先后顺序无影响。
- **铁门 `OnStartEating`**：复刻旧 `StartEat` 两分支的 `ShowArm(true)`（门在守卫、仅真开吃一次），啃植物 / 啃僵尸都覆盖。
- **纸僵尸 / 快纸僵尸**：`PlayWalkAnimation` 体 == 旧 `ResumeWalkAfterEat` 体，walk / walk_nopaper+clip 选择不变；`FastPaperZombie` 继承之，两级链一致。植物分支新增的 `OnStartEating()` 在纸僵尸为 no-op（无残留），行为不变。
- **有 walk2 的僵尸（普通/路障/铁桶/快铁桶）**：基类 `PlayWalkAnimation` 播 walk2，与旧默认逐字相同，零变化。
- **存档格式**：零改动（走路轨道从状态派生，不新增持久化字段）。

## 7. AutoTest 回归矩阵（改完逐条跑，绿了才提交）

| 用例 | 验证点 | 对应历史 bug |
|---|---|---|
| 撑杆啃植物 → 植物拔除 → 回走路 | 播 anim_walk 不卡 | — |
| 读档「正啃的植物已死」→ 回走路 | 拆地雷一：不播不存在轨道 | d83bab0 同类 |
| 纸 / 快纸僵尸被魅惑收尾 | 不卡 anim_eat（两级继承复验） | d83bab0 |
| 铁门被魅惑 → 啃食 → 收尾 | 手臂不多不少（OnStart/OnStop 对称） | c0cb799 |
| 铁门魅惑后啃敌方僵尸 | 啃时有臂、收尾藏臂 | 52bd86d |

（若已有 `dump_state` 的 `armVisible` / `doorArmVisible` 断言字段，回归用例应校验之。）

## 8. 风险与缓解

- **风险：删整段覆写时漏掉某分支的副作用** → 缓解：§5 每个删除项均在 §6 逐语句自证等价；实现时对照旧函数体逐行核对再删。
- **风险：`OnStartEating` 触发点放错导致重复 show 或漏 show** → 缓解：严格用 `!wasEating && mIsEating` 守卫，AutoTest 门臂两用例（c0cb799 / 52bd86d）覆盖。
- **风险：B 组走路站点转换后 clip 或 blend 默认值不一致** → 缓解：实现时确认 `PlayWalkAnimation` 的 clip=0（回落 base 走速）与各站点旧语义一致（EndJump 先设 base 速度再走，正是 clip=0 意图）。

## 9. 涉及文件

- `PlantVsZombies/Game/Zombie/Zombie.h`（新契约 + 注释）
- `PlantVsZombies/Game/Zombie/Zombie.cpp`（PlayWalkAnimation 实现、StartEat 加 OnStartEating、682/742 收编、删 WalkTrackAfterEat）
- `PlantVsZombies/Game/Zombie/Polevaulter.{h,cpp}`（新增 PlayWalkAnimation，删 3 覆写，EndJump/Load 转调）
- `PlantVsZombies/Game/Zombie/PaperZombie.{h,cpp}`（ResumeWalkAfterEat→PlayWalkAnimation，删 2 覆写，Load 转调）
- `PlantVsZombies/Game/Zombie/DoorZombie.{h,cpp}`（新增 OnStartEating/OnStopEating，删 4 覆写）
- （`FastPaperZombie` / `FastBucketZombie` / `ConeZombie` / `BucketZombie` 无需改动）
