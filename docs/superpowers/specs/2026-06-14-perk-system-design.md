# 生存模式词条系统 设计文档

- 日期：2026-06-14
- 范围：生存模式（无尽）专用的"词条"（perk）系统的**数据/逻辑层**。
- **不在本次范围**：词条选择 UI（下次对话单独做）。本次只保证管理器留好 UI 所需的查询接口。

## 1. 背景与目标

生存模式后期需要可累积的"词条"修正局内规则。本次先实现 3 个试水词条，并搭建一个**统一管理类**承载所有词条的：层数、文字描述、数值效果、存档。要求：

- 生存模式专用；非生存关完全无副作用。
- 已获得的词条要随本局生存进度存档/读档。
- 每个词条可**独立配置**能否叠加及叠加上限。

### 三个试水词条

| ID（存档键） | 名称 | 每层效果 | 上限 | 总上限 |
|---|---|---|---|---|
| `PLANT_DAMAGE_UP` | 全体植物伤害 | +10% | 10 层 | +100% |
| `ZOMBIE_HEALTH_UP` | 僵尸血量 | +20% | 10 层 | +200% |
| `ZOMBIE_DAMAGE_RESIST` | 僵尸免伤 | +5% | 10 层 | 50%（伤害 ×0.5） |

> 注：词条 2、3 是"增难"，词条 1 是"助玩家"。系统对好/坏一视同仁——它只是一个通用修正容器。层数上限均为常量，便于调参。

## 2. 架构

采用 **Board 持有 `SurvivalPerkManager` 值成员 + 聚合修正量**的方案。

- 生命周期天然 = 一局生存（= Board 生命周期）。无全局可变状态，无需手动跨关 reset。
- 三个修正钟点（chokepoint）都已持有 `Board* mBoard`（`Zombie.h:18`、`Bullet.h:32`），可直接 `mBoard->GetPerkManager()` 读取。
- **中性默认即自动失效**：聚合 getter 在无词条时返回乘法单位元（1.0 / 原值），令非生存关、未获取的词条在各钟点自动 no-op，避免到处写 `if (mIsSurvival)`。
- 匹配本仓库既有范式：硬编码元数据表照 `GameDataManager`，Board 持有子系统照 `mEntityManager`。

### 被否决的方案

- **全局单例**：三个钟点反正都有 `mBoard`，单例的"任意处可达"优势用不上；却引入全局可变状态的跨局泄漏风险、AutoTest 隔离负担、与"一局"语义不符的生命周期。
- **Board 上裸标量、无管理器**：拿不到可枚举的词条列表 + 文字描述，违背"统一类管理所有词条及其描述"的要求（UI 下次要逐条列名称/描述/层数）。

## 3. 数据模型与 API

新增目录 `Game/Perk/`，两个文件。

### `PerkType.h`

```cpp
enum class PerkType { PLANT_DAMAGE_UP, ZOMBIE_HEALTH_UP, ZOMBIE_DAMAGE_RESIST, COUNT };

struct PerkInfo {
    const char* key;        // 存档稳定键名（不随 enum 顺序变）
    const char* nameZh;     // 显示名（UI 用）
    const char* descZh;     // 每层效果描述（UI 用）
    float       perStack;   // 每层数值（0.10 / 0.20 / 0.05）
    int         maxStacks;  // 每词条独立上限（=1 即一次性词条）
};
```

### `SurvivalPerkManager.{h,cpp}`

```cpp
class SurvivalPerkManager {
public:
    bool AddPerk(PerkType type);          // +1 层，到 maxStacks 截断；已满返回 false
    int  GetStacks(PerkType type) const;
    void Clear();                         // 进新生存局时清空

    // 聚合效果——空词条时返回中性值，三处钟点自动 no-op
    double GetZombieHealthMultiplier() const;     // 1 + 0.20 * stacks
    int    ScalePlantDamage(int base) const;      // round(base * (1 + 0.10*stacks))，结果 >=1
    int    ScaleDamageToZombie(int base) const;   // round(base * (1 - 0.05*stacks))，结果 >=1

    static const PerkInfo& GetInfo(PerkType type);  // 静态元数据表（UI 用）

    void Save(nlohmann::json& j) const;
    void Load(const nlohmann::json& j);
private:
    std::array<int, static_cast<size_t>(PerkType::COUNT)> mStacks{};  // 全 0 初始
};
```

- 元数据表为 `.cpp` 内函数级 `static` 数组，按 `PerkType` 索引。
- 取整一律 `+0.5` 四舍五入；`ScalePlantDamage`/`ScaleDamageToZombie` 在原值 `>=1` 时结果**夹到 >=1**，防 50% 免伤把 1 点伤害抹成 0 造成无敌。

## 4. 集成点（精确改法）

| 词条 | 钟点 | 改法 |
|---|---|---|
| 僵尸血量 +20% | `Board::GetZombieHpMultiplier()`（`Board.h:230`，header-inline） | 末尾 `multiplier *= mPerkManager.GetZombieHealthMultiplier();`，折进同一个 `double`。出生单点生效（`Board.cpp:168`）；读档走 `CreateZombieWithID` 不重缩，沿用现有契约。 |
| 植物伤害 +10% | 子弹命中 `Bullet.cpp:29`；瞬时伤害 `Board::CreateBoom` 入口（`Board.cpp:75`，樱桃炸弹 AOE） | 子弹：`zombie->TakeDamage(mBoard->GetPerkManager().ScalePlantDamage(dmg))`。CreateBoom：**在入口处把 `damage` 缩放一次**（`damage = GetPerkManager().ScalePlantDamage(damage);`），令 `Charred` 秒杀阈值判定（`:87`）与实扣血（`:93`）用同一加成后值，避免同一 damage 在函数内不一致。割草机走 `LawnMower.cpp:39` 直连 `TakeDamage`，不在此路径，**自动排除**（割草机非植物）。 |
| 僵尸免伤 +5% | `Zombie::TakeDamage` 首行（`Zombie.cpp:359`，`if(damage<=0)return;` 之后） | `if (mBoard) damage = mBoard->GetPerkManager().ScaleDamageToZombie(damage);` 单点覆盖一切伤害来源。 |

- **叠加语义**：植物伤害在伤害侧（子弹/爆炸）放大，免伤在僵尸侧 `TakeDamage` 缩减，两者自然乘法叠加：`基础 ×1.1 ×(1−免伤)`。会有一次"双重取整"（子弹侧 + TakeDamage 侧），误差 <1 血，可忽略；换取两个钟点语义独立。
- **范围切分依据**：免伤放 `TakeDamage` 才能一处覆盖所有来源；植物伤害必须放伤害侧才能精确排除割草机这类非植物伤害。

### 编译依赖说明

`Board::GetZombieHpMultiplier()` 是 header-inline，且 `Board` 以**值成员**持有 `SurvivalPerkManager`，故 `Board.h` 必须 `#include "Perk/SurvivalPerkManager.h"`。该头文件须保持轻量（仅 `<array>` + `PerkType.h` + nlohmann/json 前置/包含），避免拖慢 `Board.h` 的编译扇出。

## 5. 存档

跟随 `mSurvivalRound`，存在 board 的关卡存档里。

- 写（`GameInfoSaver` 写 `survivalRound` 旁，约 `:79`）：
  ```cpp
  if (board->mIsSurvival) board->mPerkManager.Save(j["perks"]);  // {"PLANT_DAMAGE_UP":3, ...}
  ```
- 读（约 `:286`）：
  ```cpp
  if (board->mIsSurvival && j.contains("perks")) board->mPerkManager.Load(j["perks"]);
  ```
- 按 `PerkInfo::key` 字符串存储 → enum 重排不坏档；缺 key → 0 层；旧档无 `perks` 字段 → 天然兼容。
- AutoTest 模式存档被短路，词条不持久 → 符合测试隔离，不影响真实游戏。

## 6. 生命周期

- 进新生存局（`Board` 初始化 survival 分支，`Board.cpp:39-44`）：`mPerkManager.Clear()`。
- 读档：`Load()` 覆盖恢复。
- 非生存关：管理器恒空，聚合 getter 返回中性值，三钟点 no-op。

## 7. 验证

- `clang-release` 构建零 warning / 零 error。
- AutoTest 行为回归：现有非生存脚本数值逐位不变（中性默认保证）。
- 手动/脚本验证生存路径：用 `set` 接口或临时注入给僵尸/玩家加词条，确认三项效果数值正确、存读档往返层数一致。（具体测试手段在实现计划里定。）

## 8. 明确不做（YAGNI）

- 词条选择 UI（下次对话）。
- 词条获取的触发流程（每轮选一个之类）——本次只提供 `AddPerk` 接口，触发交给 UI/上层。
- 超过 3 个的词条；本次只 3 个试水，扩展只需往元数据表 + 三个聚合 getter 加分支。
