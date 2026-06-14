# 生存模式词条系统 · 第二批（3 个新词条）设计文档

- 日期：2026-06-14
- 范围：在已落地的 `SurvivalPerkManager`（见 `2026-06-14-perk-system-design.md`）之上**再加 3 个词条**，含数据层 + 逻辑钟点接入 + 存档 + AutoTest 验证。
- **不在本次范围**：词条选择 UI（主人明确"先不着急做 UI"）。仍只保证管理器留好 UI 所需查询接口。

## 1. 三个新词条

| ID（存档键） | 名称 | 每层效果 | 层数上限 | 备注 |
|---|---|---|---|---|
| `ZOMBIE_DAMAGE_UP`   | 僵尸伤害   | 僵尸对植物伤害 +5% | **不限**（实现为 9999） | 助"增难"，无状态倍率 |
| `ZOMBIE_INVULN_HITS` | 僵尸前 N 次免伤 | 出生后前 10 次受击伤害 = 0 | **2**（最多前 20 次） | **per-zombie 状态** |
| `PLANT_REGEN`        | 植物回血   | 每 5 秒回 25 HP | **5**（最高 125 HP/5s） | **全局脉冲**，封顶 maxHealth |

> 与第一批一致：系统对好/坏词条一视同仁，只是通用修正容器；层数/数值均为常量便于调参。

### 数值与策略（词条③，主人全权委托，已定）

| 决策 | 选择 | 理由 |
|---|---|---|
| 间隔 x | 5 秒 | 持续续航，非瞬回 |
| 每层每次 y | 25 HP | 多数植物 300 血；≈5 HP/s/层，挡不住被啃但恢复零散掉血 |
| 可叠加 | 可，**线性**（n 层 = 每 5 秒 25n） | 与"每层"模型一致 |
| 层数上限 | 5 层（最高 125 HP/5s） | 防回血碾压 |
| 是否可超 maxHealth | **否**，封顶 `mPlantMaxHealth` | 标准治疗语义，可预测；不做过量治疗 |

## 2. 架构：两个词条打破"无状态聚合"假设

第一批 3 个词条全是**无状态聚合修正量**（`SurvivalPerkManager` 只存层数，钟点读全局倍率）。本批：

- **词条①（僵尸伤害）** 仍是无状态倍率 —— 完全镜像现有"植物伤害"词条，加 `ScaleZombieDamage(int)`。
- **词条②（前 N 次免伤）** 需要**每只僵尸自己的剩余次数计数器** —— 系统首个 per-entity 状态词条。`SurvivalPerkManager` 只提供"初始次数 = 10×层数"的查询，实际计数器存在 `Zombie` 上并随僵尸存档。
- **词条③（植物回血）** 是**全场周期脉冲**，归 `Board` 拥有（与阳光/出怪计时器同类），不是 per-plant 状态。

### 词条③ 为何用全局计时器而非 per-plant 计时器

- 回血在概念上是**一次全场脉冲**（每 5 秒所有植物一起回），数据源（词条层数）已在 `Board` 上 → `Board` 拥有计时器，归属天然一致，与既有 `Board` 周期行为（阳光生成、出怪倒计时）同范式。
- per-plant 计时器适合"各自相位、各自参数"的行为（如 `SunFlower`/`Shooter` 冷却）；回血既无各自相位、参数又全局统一，硬塞每株植物属错配。
- **性能非问题**：O(n) 遍历只在计时器**触发的那一帧**发生（每 5 秒 1 次），不是每帧扫描；与 memory 记的"每帧 `GetAllGameObjects()` foot-gun"不同量级。
- **附带收益**：`Plant.{h,cpp}` 完全不用改（无需加字段）。
- 取舍：全场**同步脉冲**（所有植物同一刻回，玩家可感知"一起回了一下"，更可预测）；计时器**不进存档**，读档从 0 重计，最多损失 5 秒进度，换零存档改动。

## 3. 数据模型改动（`Game/Perk/`）

### `PerkType.h`

```cpp
enum class PerkType {
    PLANT_DAMAGE_UP,
    ZOMBIE_HEALTH_UP,
    ZOMBIE_DAMAGE_RESIST,
    ZOMBIE_DAMAGE_UP,       // 新增：僵尸对植物伤害 +5%/层（不限层）
    ZOMBIE_INVULN_HITS,     // 新增：出生前 N 次免伤（N = 10×层，最多 2 层）
    PLANT_REGEN,            // 新增：全场每 5 秒回 25×层 HP（最多 5 层，封顶 maxHealth）
    COUNT
};
```

`PerkInfo` 结构**不变**（仍 `{key, nameZh, descZh, perStack, maxStacks}`）。`perStack` 一字多用：

- `ZOMBIE_DAMAGE_UP`：`perStack = 0.05`（百分比），`maxStacks = 9999`（等效不限，避 int 溢出）。
- `ZOMBIE_INVULN_HITS`：`perStack = 10`（次/层），`maxStacks = 2`。
- `PLANT_REGEN`：`perStack = 25`（HP/层/次），`maxStacks = 5`。回血**间隔**不进 `PerkInfo`（只此一词条用），用管理器内常量 `kPlantRegenIntervalSec = 5.0`。

> 决策：不为间隔扩 `PerkInfo` 字段（YAGNI，仅一词条需要第二参数），改用管理器内具名常量 + getter 暴露，保持元数据表对其余词条简洁。

### `SurvivalPerkManager.{h,cpp}` 新增 API

```cpp
// 词条①：僵尸对植物伤害（无状态，镜像 ScalePlantDamage）
int    ScaleZombieDamage(int base) const;        // round(base * (1 + 0.05*stacks))，base>=1→结果>=1

// 词条②：僵尸出生免伤次数（per-zombie 计数器的初值来源）
int    GetZombieInvulnHits() const;              // 10 * stacks（无层→0）

// 词条③：植物回血脉冲参数（Board 全局计时器消费）
float  GetPlantRegenInterval() const;            // 常量 5.0（恒定，便于 Board 取间隔）
int    GetPlantRegenPerPulse() const;            // 25 * stacks（无层→0，Board 据此 skip）
```

- `ScaleZombieDamage` 复用现有 `RoundScale`（已 `+0.5` 四舍五入、`base>=1` 夹底 >=1）。此处是**增伤**（倍率>1），夹底无副作用。
- `GetZombieInvulnHits` / `GetPlantRegenPerPulse` 在无层时返回 0 → 调用方天然 no-op。
- `kPerks` 表补 3 行，`static_assert` 强制与枚举一一对应（顺序须与枚举一致）。

## 4. 集成点（精确改法）

| 词条 | 钟点 | 改法 |
|---|---|---|
| ① 僵尸伤害 +5% | `Zombie::EatTarget()` `Zombie.cpp:460`（全局唯一一处僵尸伤植物） | `plant->TakeDamage(mBoard->GetPerkManager().ScaleZombieDamage(mAttackDamage));`（`mBoard` 在 EatTarget 路径恒非空）。**在使用时缩放，不写回 `mAttackDamage`**——否则存档 `attackDamage`（`GameInfoSaver:164/409`）被污染，读档叠加重复放大。 |
| ② 前 N 次免伤 | `Zombie::TakeDamage` 首行 `Zombie.cpp:364`（`if(damage<=0)return;` 之后、免伤倍率 `:367` 之前） | `if (mFreeHitsRemaining > 0) { --mFreeHitsRemaining; return; }`。一处覆盖所有伤害来源；提前 return → **不触发受击白光**（`SetGlowingTimer`），0 伤害不应闪。 |
| ③ 植物回血 | `Board::Update`（阳光/出怪计时器同处）每帧累加 `mPlantRegenTimer += dt`；满 `GetPlantRegenInterval()` 触发一次脉冲 | 脉冲：`int heal = pm.GetPlantRegenPerPulse(); if (heal>0) for (id : GetAllPlantIDs()) { p=GetPlant(id); if(!p||p预览) continue; p->mPlantHealth = min(p->mPlantHealth+heal, p->mPlantMaxHealth);}` 仅 `mBoardState==GAME` 时跑（与选卡冻结一致）。 |

### 词条② 的 per-zombie 状态：初始化 + 存档

- **字段**：`Zombie.h` 加 `int mFreeHitsRemaining = 0;`（public 区，与 `mAttackDamage` 等同级）。
- **出生初始化**：`Board::CreateZombie` `Board.cpp:170`（`ApplyHealthMultiplier` 同一波次出生钟点，`!isPreview && !skipsettings` 块内）后追加：
  `zombie->mFreeHitsRemaining = GetPerkManager().GetZombieInvulnHits();`
  读档路径 `CreateZombieWithID` **不**在此处赋值（与血量倍率同契约），改由下方 Load 还原。
- **存档**：写进 `Zombie::SaveProtectedData` / `LoadProtectedData`（`Zombie.cpp:94/108`，基类无条件调用，不受子类 `SaveExtraData` 覆盖影响）：
  - Save：`j["freeHitsRemaining"] = mFreeHitsRemaining;`
  - Load：`mFreeHitsRemaining = j.value("freeHitsRemaining", 0);`（旧档缺字段 → 0）
- **理由**：存档**会**在 `GAME` 状态带场上僵尸落盘（`GameInfoSaver:69-70` + `:177` 序列化 zombies），故 per-zombie 计数器必须持久化，否则中途存退再读档免伤次数会错乱。

### 编译依赖

无新增头依赖。`Board.cpp` 已 include `SurvivalPerkManager.h`（经 `Board.h`）与 `EntityManager`；`Zombie.cpp` 已可达 `mBoard->GetPerkManager()`。

## 5. 存档

- 词条**层数**：第一批已做的 `SurvivalPerkManager::Save/Load`（`GameInfoSaver:80/289`）自动覆盖新增 3 项（按 key 字符串，新 key 旧档缺失 → 0 层），**无需改 GameInfoSaver 的词条段**。
- 词条②**per-zombie 计数器**：经 `SaveProtectedData/LoadProtectedData`（见 §4），随每只僵尸存档。
- 词条③**全局计时器**：不持久化（读档从 0 重计，见 §2 取舍）。
- AutoTest 模式存档短路 → 不影响测试隔离。

## 6. 生命周期

- 进新生存局 `mPerkManager.Clear()`（`Board.cpp:42`，已有）→ 新增 3 项层数随之归 0。
- 全局回血计时器 `mPlantRegenTimer` 进新局 / 读档置 0。
- 非生存关：层数恒空，`ScaleZombieDamage` 返回原值、`GetZombieInvulnHits`/`GetPlantRegenPerPulse` 返回 0 → 三钟点全 no-op。

## 7. 验证

- `clang-release` 构建零 warning / 零 error。
- 非生存路径回归：无词条时数值逐位不变（中性默认/返 0 保证）。
- AutoTest 端到端（`TestDriver.cpp`）：
  - `add_perk` 名称表补 3 个新枚举（`ZOMBIE_DAMAGE_UP` / `ZOMBIE_INVULN_HITS` / `PLANT_REGEN`）。
  - `dump_state` 的 `perks` 块补字段：`zombieDamageOn100 = ScaleZombieDamage(100)`、`zombieInvulnHits = GetZombieInvulnHits()`、`plantRegenPerPulse = GetPlantRegenPerPulse()`。
  - 新 smoke 脚本验证：① 加 N 层后僵尸啃食扣血 = round(50×(1+0.05N))；② 加 1~2 层后僵尸前 10/20 次受击血量不掉、第 11/21 次起正常掉；③ 加 K 层后等待 ≥5s，受损植物回血 = 25K（且不超 maxHealth）。

## 8. 明确不做（YAGNI）

- 词条选择 UI（后续）。
- 词条③回血间隔的可配置化 / 进 `PerkInfo`（仅一词条需要，用常量）。
- 词条③全局计时器存档（损失 ≤5s，不值当）。
- 词条②"哪些来源算一次受击"的细分（统一 = 一次 `TakeDamage` 调用 = 一次）。
