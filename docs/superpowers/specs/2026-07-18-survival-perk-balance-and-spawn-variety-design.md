# 生存词条平衡与出怪池多样性设计

- 日期：2026-07-18
- 范围：新增植物增益词条、收敛过强僵尸诅咒、限制每轮出怪种类并加入数量波动
- 关联：`.agents/skills/adding-survival-perk/SKILL.md`、`docs/agent-memory/project_pvz_perk_system.md`、`docs/agent-memory/project_pvz_survival_spawn_round_table.md`

## 背景与目标

生存模式每次选择会同时获得一个植物增益和一个僵尸诅咒。现有僵尸血量每层 +20%、免伤最多可把承伤压到 10%，出生免伤最多 20 次；植物侧除无限叠加的伤害外，其余词条又较早封顶。因此十几轮持续选择后，诅咒的有效成长明显快于植物增益，玩家会遇到“跳过优于选择”的情况。

本次目标：

1. 新增 3 个植物侧成长维度，让防守、经济和布阵周转都能对抗深轮压力。
2. 收敛现有三项过强僵尸词条，并让基础伤害与血量成长同速。
3. 生存出怪池每轮最多 8 种（含普通僵尸），第 3 轮起在基础数量上随机增加或减少 1～2 种。
4. 保持现有“一个植物增益 + 一个僵尸诅咒”的配对选择、存档格式和逐波点数预算不变。

## 词条数值

### 新增植物增益

| key | 显示名 | 每层效果 | 上限 | 满层结果 |
|---|---|---:|---:|---:|
| `PLANT_DAMAGE_REDUCTION` | 植物韧性 | 植物受到伤害 -3% | 15 | 承伤 55% |
| `PLANT_SUN_BONUS` | 阳光增产 | 收集阳光 +15% | 10 | 收益 2.5 倍 |
| `PLANT_CARD_RECHARGE` | 卡片加速 | 卡片冷却速度 +12% | 10 | 速度 2.2 倍 |

三项均为原型 A：状态只由 `SurvivalPerkManager` 层数派生，0 层返回中性倍率。

### 现有词条平衡调整

| 词条 | 调整前 | 调整后 | 理由 |
|---|---:|---:|---|
| 全体植物伤害 | +10%/层 | +12%/层 | 与僵尸血量同速，避免固定配对天然亏损 |
| 僵尸血量 | +20%/层 | +12%/层 | 直接处理主人指出的过强成长 |
| 僵尸免伤 | -5%/层，最多 18 层 | -3%/层，最多 15 层 | 最低承伤由 10% 提高到 55% |
| 僵尸出生免伤 | 10 次/层，最多 2 层 | 4 次/层，最多 2 层 | 满层从 20 次降到 8 次，保留机制特色 |

僵尸伤害、植物回血和植物攻速维持当前数值。

## 接入点

### 植物韧性

- manager：`GetPlantDamageTakenMultiplier()`、`ScaleDamageToPlant(int)`。
- 钟点：`Plant::TakeDamage` 在扣血前缩放伤害。
- 樱桃炸弹和清醒毁灭菇原有无敌覆写不进基类，行为保持不变；睡眠毁灭菇调用基类，自动获得韧性。

### 阳光增产

- manager：`GetSunIncomeMultiplier()`、`ScaleSunIncome(int)`。
- 钟点：`Board::AddSun`。当前唯一正常调用者是 `Sun::OnReachTargetBack`，因此天空阳光、向日葵和阳光菇产物都统一生效。
- `set_sun`、开局阳光和花费不走 `AddSun`，不会被误当成收益重复缩放。

### 卡片加速

- manager：`GetPlantCardRechargeMultiplier()`。
- 钟点：`CardComponent::Update` 冷却倒计时减去 `dt * multiplier`。
- `CardSlotManager` 提供只读 `GetBoard()`，供卡片组件取得 manager；选卡界面的卡片仍在函数首行返回，不参与冷却。
- 存档继续保存剩余秒数；读档后按当前词条倍率继续倒计时，不新增 per-card 状态。

## 出怪池数量

- 第 1～2 轮维持确定性。
- 第 3 轮起先按现有公式计算基础种类数：普通僵尸 1 种 + `1 + (round - 3) / 2` 个额外候选。
- 基础种类数先夹到上限 8，再随机生成 `±1` 或 `±2` 的非零波动，最终夹到 `[2, min(8, 可用候选数+1)]`。
- 深轮基础值到顶后，正波动保持 8，负波动产生 6～7 种；最终绝不超过 8。
- 候选仍使用无放回 Fisher-Yates 抽样；普通僵尸仍必出。
- `weight <= 0` 的类型不进入候选池，避免伴舞僵尸占用种类名额却永远无法被加权抽中。
- 不修改 `GetSurvivalPickWeight`、`PickZombieType`、点数成本、单波预算、血量成长和存档。

## 存档兼容

新增词条沿现有字符串 key 存档。旧档缺少新 key 时为 0；现有 key 不改名。调整后的 `maxStacks` 会在 Load 时自动夹紧旧档中过高的僵尸免伤层数。出怪池仍由轮次重建，不入存档。

## AutoTest

1. 新增 `smoke_perks_balance.json`：闭合断言三个新词条与四项平衡值。
2. 更新 `smoke_perks.json` 的旧数值断言。
3. `dump_state` 新增三个词条层数及整数投影：`damageToPlantOn100`、`sunIncomeOn100`、`cardRechargeOn1000`。
4. `dump_state` 新增 `spawnTypeCount`；更新 `smoke_survival_spawn_round.json`，固定 `-Seed 42` 检查各轮随机结果和深轮上限。
5. 回归既有 regen、attack speed、zombie damage、invulnerability、perk select 脚本。
