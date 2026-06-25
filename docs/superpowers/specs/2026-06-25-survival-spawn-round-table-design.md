# 生存模式刷怪：轮次解锁表 + 随机子集池 + 原版调制

**日期**：2026-06-25
**范围**：重写 `Board::BuildSurvivalSpawnList`，把硬编码的轮次闸门数据化进 `GameDataManager`，并加入原版 `Board.cs` 的两条调制机制。
**目标读者**：实现者（主人 / 后续 plan）。

---

## 1. 背景与现状

生存无尽模式（关卡 `1000` 白天 / `1001` 黑夜）每轮（每面旗）`SURVIVAL_WAVES_PER_ROUND = 10` 波，第 10 波为"一大波"。每轮清空后 `OnSurvivalRoundClear()` 推进 `mSurvivalRound++`、重置 `mCurrentWave = 0`，并调用 `BuildSurvivalSpawnList(mSurvivalRound)` 重建本轮**出怪池子** `mSpawnZombieList`。

### 1.1 两层结构（关键）

刷怪分两层，本次改动只动**建池层**，**逐波生成层不动**：

- **建池层** `BuildSurvivalSpawnList(round)`：构建 `mSpawnZombieList`（本轮**可用僵尸种类池**）。
- **逐波生成层** `TrySummonZombie()`：每波按点数预算反复 `PickZombieType() → GetWeightedRandomZombie()`，从池里**按 `weight` 加权随机**抽，直到预算耗尽。

### 1.2 现状代码事实

- `BuildSurvivalSpawnList` 当前用硬编码 `if (round >= N)` 串联解锁（normal→r1, cone→r2, polevaulter/newspaper→r3, bucket→r4, fastbucket→r5, fastpaper→r6）。
- `PickZombieType` 内 `(mIsSurvival || mCurrentWave >= minWave)` 短路 —— **生存模式完全无视故事模式的 `appearWave`**；当前唯一难度闸门就是建池层那串硬编码。
- **`weight` 字段一物两用**（本仓库与原版的关键差异）：`GetZombieWeight(type)` 既是 `GetWeightedRandomZombie` 的**抽中概率权重**，又是 `PickZombieType`/`TrySummonZombie` 里扣点数预算的**成本**。原版把这两者分成 `mPickWeight` 与 `mZombieValue` 两个字段。
- `ZombieType` 枚举前 7 个值（索引 0~6：NORMAL, TRAFFIC_CONE, POLEVAULTER, BUCKET, FASTBUCKET, NEWSPAPER, FASTPAPER）恰好是**全部已实现**僵尸；`ZOMBIE_DOOR`(7) 起均未实现（无 factory）。`NUM_ZOMBIE_TYPES` 当前在枚举末尾。

---

## 2. 目标行为

| 轮次 | 出怪池子 |
|---|---|
| 第 1 轮 | 只有「普通」 |
| 第 2 轮 | 「普通 + 路障」 |
| 第 3 轮起 | 必含「普通」，再从**合格候选**里**随机抽一个子集**，子集大小随轮**缓慢增长** |

外加两条复刻自原版 `Board.cs` 的调制：

- **杂兵稀释**：普通/路障的**抽中权重**随轮线性衰减（成本不变）→ 越后期杂兵越稀、硬僵尸占比升高。
- **旗数递减**：每种僵尸的**有效最早轮次**随大轮数下调 → 深局更早解锁强僵尸。**忠实复刻原版曲线（18 旗起步）**，当前 7 僵尸阵容下休眠，为未来加强僵尸预留。

---

## 3. 设计

### 3.1 枚举改动（`Game/Zombie/ZombieType.h`）

把 `NUM_ZOMBIE_TYPES` 从枚举末尾移到 `ZOMBIE_FASTPAPER` 之后，使 `NUM_ZOMBIE_TYPES == 7`。`ZOMBIE_DOOR`..`ZOMBIE_REDEYE_GARGANTUAR` 仍保留（值变为 8+）。

**安全性已核实**：

- `NUM_ZOMBIE_TYPES` 全部 6 处用法均为**哨兵/边界检查**（默认"无效类型"、JSON `spawnlists.json` 越界拒绝），**无一处当数组定长或全量迭代上界**。
- 图鉴 `ZombieAlmanacScene` 走 `GameDataManager::GetAllZombieTypes()`（只返回已注册的 7 个），**不受影响**。
- AutoTest 名字表 `TestDriver.cpp` 是显式 `ZT(...)` 键值对列表（含未实现僵尸名，供 `spawn_zombie` 调试用），**不按枚举值定长**，不受影响。
- 唯一行为变化：`Board.cpp` 的 `LoadSpawnListFromJson` 越界检查会拒掉 ≥7 的 JSON 值，符合预期（那些本就未实现）。

效果：随机抽取可安全用 `ZombieType(GameRandom::Range(0, NUM_ZOMBIE_TYPES - 1))`，绝不会抽到未完成僵尸。

### 3.2 轮次表（`Game/Plant/GameDataManager.{h,cpp}`）

采用**方案 A**：在 `ZombieInfo` 内加字段，贴合现有 `weight`/`appearWave` 惯例（单一真相源）。

- `ZombieInfo` 增 `int survivalRound = 0;`（生存模式最早出场轮；`0` = 不进生存，安全默认）。
- `RegisterZombie(...)` 增形参 `int survivalRound`（置于 `appearWave` 之后、`scale` 之前），构造后 `info.survivalRound = survivalRound;`（与 `scale`/`factory` 同样的"构造后赋值"风格，不动 `ZombieInfo` 构造函数）。
- 新增访问器 `int GetZombieSurvivalRound(ZombieType) const;`，镜像 `GetZombieAppearWave`（查不到返回 `0`）。
- 更新 7 处 `RegisterZombie` 调用 + `.h` 声明 + doc 注释。

**提议默认表**（可调）：

| 僵尸 | survivalRound |
|---|---|
| NORMAL | 1 |
| TRAFFIC_CONE | 2 |
| NEWSPAPER | 3 |
| POLEVAULTER | 3 |
| BUCKET | 4 |
| FASTBUCKET | 5 |
| FASTPAPER | 6 |

### 3.3 调制曲线辅助（钳制线性插值）

复刻原版 `TodCommon.TodAnimateCurve(Linear)` = 钳制线性插值 + 四舍五入取整。放在 `Board.cpp` 文件内静态函数（仅本翻译单元用）：

```cpp
// 复刻原版 TodAnimateCurve(..., TodCurves.Linear)：把 round 在 [startRound,endRound]
// 归一化并钳到 [0,1]，在 [fromVal,toVal] 间线性插值，四舍五入取整。
static int SurvivalCurveLerp(int startRound, int endRound, int round,
                             int fromVal, int toVal)
{
    if (endRound <= startRound) return toVal;            // 防 0 除
    float t = static_cast<float>(round - startRound)
            / static_cast<float>(endRound - startRound);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return static_cast<int>(std::lround(
        static_cast<float>(fromVal) + t * static_cast<float>(toVal - fromVal)));
}
```

**轮→旗映射**：原版以 `survivalFlagsCompleted` 驱动；我们模型里 `flagsCompleted = mSurvivalRound - 1`（当前轮之前已完成的轮数）。下文曲线一律传 `flagsCompleted`。

### 3.4 旗数递减（`BuildSurvivalSpawnList` 候选合格判定）

```cpp
// 复刻原版：num3 = TodAnimateCurve(18, 50, flags, 0, 15, Linear); eff = max(base - num3, 1)
int flagsCompleted = round - 1;
int reduction = SurvivalCurveLerp(SURVIVAL_UNLOCK_REDUCE_START_FLAG,   // 18
                                  SURVIVAL_UNLOCK_REDUCE_END_FLAG,     // 50
                                  flagsCompleted, 0,
                                  SURVIVAL_UNLOCK_REDUCE_MAX);         // 15
int effSurvivalRound = std::max(baseSurvivalRound - reduction, 1);
// 合格条件：baseSurvivalRound >= 1 (进生存) 且 effSurvivalRound <= round
```

> 当前阵容（survivalRound 最高 6）下，`flagsCompleted` 要到 18 才开始递减，故**此条休眠**——为未来高 survivalRound 僵尸预留。主人已确认选「忠实复刻原版（休眠）」。

### 3.5 杂兵稀释（`GetWeightedRandomZombie` 抽中权重侧）

**只动抽中概率，不动成本**（因 `weight` 一物两用）。新增 Board 私有助手：

```cpp
// 仅供 GetWeightedRandomZombie 使用的"抽中权重"，对 NORMAL/CONE 随轮稀释；成本侧仍用 GetZombieWeight。
int Board::GetSurvivalPickWeight(ZombieType type) const
{
    int base = GameDataManager::GetInstance().GetZombieWeight(type);
    if (!mIsSurvival) return base;
    int flags = mSurvivalRound - 1;
    if (type == ZombieType::ZOMBIE_NORMAL)
        return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
                                 flags, base, base / 10);   // 原版 Normal: base → base/10
    if (type == ZombieType::ZOMBIE_TRAFFIC_CONE)
        return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
                                 flags, base, base / 4);    // 原版 Cone: base → base/4
    return base;
}
```

- `GetWeightedRandomZombie` 内 `GetZombieWeight(type)` 两处替换为 `GetSurvivalPickWeight(type)`（累加总权重 + 减法选中）。
- `PickZombieType`(成本) 与 `TrySummonZombie`(成本) 的 `GetZombieWeight` **保持不变**（成本不稀释）。

> 原版 Normal/Cone 稀释起步于 10 旗（`TodAnimateCurve(10,50,...)`），同样在当前阵容下较晚才显著。常量化便于调早。

### 3.6 `Board::BuildSurvivalSpawnList(round)` 重写

```cpp
void Board::BuildSurvivalSpawnList(int round)
{
    mSpawnZombieList.clear();
    mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);   // 普通：每轮必有，先固定放入

    // 1) 收集本轮合格候选（排除 NORMAL；应用旗数递减后的有效轮次）
    int flagsCompleted = round - 1;
    int reduction = SurvivalCurveLerp(SURVIVAL_UNLOCK_REDUCE_START_FLAG,
                                      SURVIVAL_UNLOCK_REDUCE_END_FLAG,
                                      flagsCompleted, 0, SURVIVAL_UNLOCK_REDUCE_MAX);
    std::vector<ZombieType> candidates;
    for (int i = 0; i < static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES); ++i)
    {
        ZombieType t = static_cast<ZombieType>(i);
        if (t == ZombieType::ZOMBIE_NORMAL) continue;
        int base = GameDataManager::GetInstance().GetZombieSurvivalRound(t);
        if (base < 1) continue;                              // 0 = 不进生存
        int eff = std::max(base - reduction, 1);
        if (eff <= round) candidates.push_back(t);
    }

    // 2) 第 1~2 轮：候选全放（确定性）→ 自然得到 {普通} / {普通,路障}
    if (round < SURVIVAL_RANDOM_POOL_START_ROUND)            // <3
    {
        for (ZombieType t : candidates) mSpawnZombieList.push_back(t);
        return;
    }

    // 3) 第 3 轮起：随机抽 extra 种（无放回，部分洗牌，绝不死循环）
    int extra = SURVIVAL_POOL_BASE_EXTRA
              + (round - SURVIVAL_RANDOM_POOL_START_ROUND) / SURVIVAL_POOL_GROWTH_EVERY;
    if (extra > static_cast<int>(candidates.size()))
        extra = static_cast<int>(candidates.size());
    // 部分 Fisher-Yates：把随机选中的元素 swap 到前缀
    for (int k = 0; k < extra; ++k)
    {
        int j = GameRandom::Range(k, static_cast<int>(candidates.size()) - 1);
        std::swap(candidates[k], candidates[j]);
        mSpawnZombieList.push_back(candidates[k]);
    }
}
```

**关于"随机抽取 + 重抽"**：主人原话是"抽到轮次未解锁的就重抽，直到满足条件"。本实现等价但**循环安全**——先把不合格者排除在 `candidates` 之外，再做无放回随机选取（部分洗牌），从根本上消除"反复抽到不合格"的浪费和死循环风险。分布上与拒绝采样一致（合格集内均匀）。

### 3.7 新增可调常量（`Game/Board.h`）

```cpp
constexpr int SURVIVAL_RANDOM_POOL_START_ROUND = 3;   // 第几轮起改为随机子集池
constexpr int SURVIVAL_POOL_BASE_EXTRA         = 1;   // 起始随机种类数（除普通外）
constexpr int SURVIVAL_POOL_GROWTH_EVERY       = 2;   // 每多少轮 +1 种（缓慢增长）

constexpr int SURVIVAL_UNLOCK_REDUCE_START_FLAG = 18; // 旗数递减起步旗（原版 18）
constexpr int SURVIVAL_UNLOCK_REDUCE_END_FLAG   = 50; // 满递减旗（原版 50）
constexpr int SURVIVAL_UNLOCK_REDUCE_MAX        = 15; // 最大递减轮数（原版 15）

constexpr int SURVIVAL_DILUTE_START_FLAG = 10;        // 杂兵稀释起步旗（原版 10）
constexpr int SURVIVAL_DILUTE_END_FLAG   = 50;        // 满稀释旗（原版 50）
```

**池子大小成长表**（`extra`，受候选数上限钳制）：

| round | 3 | 4 | 5 | 6 | 7 | 9 | 11 | 13 |
|---|---|---|---|---|---|---|---|---|
| extra | 1 | 1 | 2 | 2 | 3 | 4 | 5 | 6 |

### 3.8 不改动的部分

- `PickZombieType` 每波加权随机的**主干逻辑**不变（池已预过滤为合格僵尸，每波抽自然合格）；`(mIsSurvival||...)` 短路保持。
- 成本/点数预算 `CalculateWaveZombiePoints`、血量增长、`OnSurvivalRoundClear` 流程、预览僵尸均零改动。
- **存档零改动**：池子每轮由 `round` 重新派生，不入存档；新增 `survivalRound` 是静态注册数据。

---

## 4. 改动文件清单

| 文件 | 改动 |
|---|---|
| `Game/Zombie/ZombieType.h` | 移动 `NUM_ZOMBIE_TYPES` 到 `ZOMBIE_FASTPAPER` 后 |
| `Game/Plant/GameDataManager.h` | `ZombieInfo` 加 `survivalRound`；`RegisterZombie` 加形参；声明 `GetZombieSurvivalRound`；更新 doc 注释 |
| `Game/Plant/GameDataManager.cpp` | 7 处 `RegisterZombie` 填 `survivalRound`；`RegisterZombie` 体内赋值；实现 `GetZombieSurvivalRound` |
| `Game/Board.h` | 8 个新常量；声明 `GetSurvivalPickWeight` 私有助手；新增 `public` const getter `GetSpawnZombieList()`（供 dump_state 读，`mSpawnZombieList` 是 private） |
| `Game/Board.cpp` | `SurvivalCurveLerp` 静态函数；重写 `BuildSurvivalSpawnList`；实现 `GetSurvivalPickWeight`；`GetWeightedRandomZombie` 改用 pick weight；需 `#include <algorithm>`/`<cmath>`（如未含） |
| `Game/AutoTest/TestDriver.cpp` | `dump_state` 增 `out["survivalRound"]`（`mSurvivalRound` 已 public）+ `out["spawnList"]`（经新 getter，逐个 `ZombieTypeName`） |

---

## 5. 边界与风险

- **候选不足**：早轮合格种类 < extra 时，`extra` 钳到候选数；第 1~2 轮候选全放，确定性满足"只普通 / 普通+路障"。
- **`weight` 一物两用**：稀释**只**走 `GetSurvivalPickWeight`（抽中侧），成本侧绝不能动 —— 否则普通僵尸每波数量异常膨胀。实现时务必只替换 `GetWeightedRandomZombie` 内的两处。
- **`extra==0` 不会发生**：`round >= 3` 时 `SURVIVAL_POOL_BASE_EXTRA = 1` 保证至少 1（除非候选为空，此时池=仅普通，仍可正常刷怪）。
- **旗数递减休眠**：当前阵容无可见效果，属预期（主人已确认）。
- **`GameRandom::Range(k, n-1)` 当 `k==n-1`**：返回 `k` 本身，`swap` 自身无害。

---

## 6. 验证计划（AutoTest）

**前置**：`dump_state` 先按 §4 加 `survivalRound` + `spawnList` 两字段（否则无法直接断言池子组成）。

新增脚本 `autotest/scripts/smoke_survival_spawn_round.json`：

1. `goto_level` 进 `1000`（白天无尽）。
2. 每轮 `dump_state` 读出 `survivalRound` + `spawnList`（类型名数组）。
3. 断言：第 1 轮池 == `{NORMAL}`；第 2 轮 == `{NORMAL, TRAFFIC_CONE}`；第 3 轮起池含 NORMAL、大小符合 §3.7 成长表、且从不含未解锁（survivalRound > round）或未实现（索引 ≥7）僵尸。
4. **推进轮次的手段**：生存推进靠清场（`mZombieNumber` 归零且本轮波次打完）。AutoTest 下最直接是反复 `dump_state` 取当前 `survivalRound`，配合推进命令观察池子变化；具体推进方式（快进 / 调试钩子）在 plan 阶段定。**因建池纯由 `round` 派生**，更轻量的验证是直接对 `BuildSurvivalSpawnList` 做单元式驱动：进关后**逐一断言每轮**，不必真打通整轮。
5. 截图 + `state.json` 人工复核；`-Seed` 固定以复现池子组成。

构建用 `clang-release`（0 warning 验证），`-Seed` 固定随机以便复现池子组成。

---

## 7. 与原版 `Board.cs` 的对照小结

| 机制 | 原版 | 本设计 |
|---|---|---|
| 每种僵尸最早出场门槛 | `zombieDefinition.mFirstAllowedWave`（波） | `ZombieInfo.survivalRound`（轮） |
| 深局提前解锁 | `TodAnimateCurve(18,50,flags,0,15)` 减 firstAllowedWave | `SurvivalCurveLerp` 减 survivalRound（同参数，休眠） |
| 杂兵稀释 | Normal→base/10、Cone→base/4（`TodAnimateCurve(10,50,...)`） | `GetSurvivalPickWeight` 同形状 |
| 池组成 | 全 `mZombieAllowed[]` 每波按门槛+权重 | **每轮随机子集**（本设计特有，按主人偏好） |
| pick 权重 vs 成本 | `mPickWeight` / `mZombieValue` 分离 | 本仓库 `weight` 合并 → 稀释只走抽中侧 |
