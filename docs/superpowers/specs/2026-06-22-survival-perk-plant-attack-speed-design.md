# 生存模式词条：植物攻速（Plant Attack Speed）

- 日期：2026-06-22
- 类型：原型 A（无状态倍率）植物增益词条
- 关联：`docs/superpowers/specs/2026-06-14-perk-batch2-design.md`、`.claude/skills/adding-survival-perk/SKILL.md`

## 背景与目标

生存模式当前植物方 DPS 偏弱——僵尸诅咒含「僵尸血量 +20%/层」「僵尸免伤 -5%/层」，
而植物增益只有「伤害 +10%/层」「回血」，主人反馈"打不动僵尸"。

本词条提供第二条植物 DPS 杠杆——**开火速度（攻速）**，与现有伤害词条叠乘后形成可观输出。

## 数值（强力档，主人拍板）

| 字段 | 值 |
|---|---|
| 枚举 | `PLANT_ATTACK_SPEED` |
| 类别 | `PerkCategory::PLANT_BUFF` |
| key（存档稳定键） | `"PLANT_ATTACK_SPEED"` |
| 显示名 | `植物攻速` |
| 每层效果描述 | `每层使全体植物开火速度 +15%（最多 8 层）` |
| perStack | `0.15f` |
| maxStacks | `8` |

满层倍率 = 1 + 0.15×8 = **2.2×** 开火频率。基准间隔 1.5s → 0.68s（仍远离动画吐弹卡点，安全余量充足）。

## 核心机制

### 开火节奏的两段时间（关键）

`Shooter::PlantUpdate`（以及 `Repeater`、`PuffShroom` 的对应实现）的开火逻辑分两段：

1. **节奏阈值**：`mShootTimer` 每帧累加 `dt`，越过 `mShootTime`（默认 1.5s）且本行有僵尸时，
   `mShootTimer=0` 并 `PlayTrackOnce("anim_shooting", …)` 播放射击动画。
2. **子弹吐出**：子弹由射击动画的 **frame event**（Shooter 第 64 帧 / PuffShroom 第 28 帧）触发，
   而非阈值越过的瞬间。计时器是在动画**开始**时清零的。

### 耦合陷阱（为何必须同步加快动画）

若只缩短阈值而不动动画速度：当 `有效间隔 < 动画跑到吐弹帧的时间`，
动画会在吐子弹前被反复重启 → 一颗子弹都打不出来。

因此本词条对每个开火卡点**两处一起改**：
- 阈值：`mShootTimer >= mShootTime / mult`（间隔等比缩短）
- 动画：`PlayTrackOnce` 的 clip-speed 字面量 `× mult`（吐弹点等比前移，DPS 干净缩放，视觉上明显在快速射击）

## 实现

### 1. `PerkType.h`

`COUNT` 之前新增枚举项 `PLANT_ATTACK_SPEED`（追加在 `PLANT_REGEN` 之后，保持存档/表顺序稳定）。

### 2. `SurvivalPerkManager.cpp` `kPerks[]`

对应位置追加一行（顺序须与枚举一致，`static_assert` 强制）：

```cpp
{ "PLANT_ATTACK_SPEED", u8"植物攻速", u8"每层使全体植物开火速度 +15%（最多 8 层）", 0.15f, 8, PerkCategory::PLANT_BUFF },
```

### 3. 聚合 getter（镜像 `GetPlantDamageMultiplier`）

```cpp
// 头文件声明
double GetPlantAttackSpeedMultiplier() const;   // 1 + 0.15 * stacks，0 层=1.0

// 实现
double SurvivalPerkManager::GetPlantAttackSpeedMultiplier() const {
    return 1.0 + GetInfo(PerkType::PLANT_ATTACK_SPEED).perStack
               * GetStacks(PerkType::PLANT_ATTACK_SPEED);
}
```

无需 `ScaleXxx`——攻速不是"缩放一个伤害整数"，由调用方直接用倍率除间隔 / 乘 clip-speed。

### 4. 三个开火卡点

每处先取一次倍率（`mBoard` 守卫防预览植物空指针，非生存关恒返 1.0）：

```cpp
float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
```

| 文件 | 阈值 | 动画 clip-speed |
|---|---|---|
| `Game/Plant/Shooter.cpp` `PlantUpdate` | `mShootTimer >= mShootTime / mult` | `1.5f * mult` |
| `Game/Plant/Repeater.h` `PlantUpdate` | 同上 | 主发 `1.9f * mult`、补发 `2.0f * mult` |
| `Game/Plant/PuffShroom.cpp` `PlantUpdate` | 同上（自带独立 `mShootTime`） | `1.5f * mult` |

> 注：`Repeater` 的补发分支（`mPendingSecondShot`）在第一发 frame event 后触发，
> 同样乘 `mult` 以保持双发紧凑。补发分支同样需取 `mult`。

### 5. 存档

层数走现有 `SurvivalPerkManager::Save/Load`（按 key 字符串；新 key 旧档缺失→0）。
倍率从层数实时派生；`mShootTimer` 已随植物 `SaveExtraData` 落盘，读档后有效间隔自动重算。
**无需 per-plant 新状态，无需改 GameInfoSaver。**

### 6. AutoTest

- `TestDriver.cpp` `kPerkNames` 加 `PK(PLANT_ATTACK_SPEED)`。
- `dump_state` 的 `perks` 块追加：
  - `stacks["PLANT_ATTACK_SPEED"] = pm.GetStacks(PerkType::PLANT_ATTACK_SPEED)`
  - `perks["plantAttackSpeedMult"] = pm.GetPlantAttackSpeedMultiplier()`（原始倍率，可见性）
  - `perks["shootIntervalOn1500"] = round(1500 / mult)`（整数化，闭合断言用；mult 取上面的倍率，inline 计算）
- 新建 `autotest/scripts/smoke_perks_attackspeed.json`：进 GAME → `add_perk` ×2 → `dump_state` →
  断言 `shootIntervalOn1500 == 1154`（= round(1500 / 1.30)，数学闭合，不赌时序）。
  并断言 `stacks["PLANT_ATTACK_SPEED"] == 2`。

## 测试与验收

- `clang-release` 构建 **0 warning / 0 error**（唯一报全量警告的配置）。
- `smoke_perks_attackspeed.json` 通过：攻速 dump 值与数学预期逐位吻合。
- 回归 `smoke_perks.json`：旧词条 dump 值**逐位不变**（中性默认 = 自动失效的证明）。
- 已选档位下满层间隔 0.68s 远离吐弹卡点，无"打不出子弹"回归。

## 非目标 / YAGNI

- 不新增僵尸诅咒（`RollPerkPairings` 笛卡尔积自动容纳非对称数量）。
- 不引入 per-plant 攻速字段（倍率全场统一，从 manager 层数派生即可）。
- 不改动植物身体 idle 动画速度（`mAnimator->SetSpeed(Range(1.1,1.3))` 是 idle，与射击 track 解耦，保持自然观感）。
- 不为其它非 Shooter 植物（樱桃、坚果等）做攻速——它们没有 `mShootTime` 节奏，词条对其天然 no-op。
