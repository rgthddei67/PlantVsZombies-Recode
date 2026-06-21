# 生存模式词条选择 UI — 设计文档

- 日期：2026-06-21
- 前置：生存模式词条系统（`SurvivalPerkManager`，6 词条）已完成，见
  `docs/superpowers/specs/2026-06-14-perk-system-design.md` 与
  `docs/superpowers/specs/2026-06-14-perk-batch2-design.md`。数据层已留好
  `GetInfo` / `GetStacks` / `AddPerk` 供 UI 调用。
- 本文档范围：**只做"轮间词条选择 UI"**，不新增/改动任何词条数值与三处生效钟点。

## 1. 目标与玩法规则（主人 2026-06-21 确认）

生存模式每清掉一轮后、进入下一轮选卡之前，弹出一个词条选择框：

- **成对权衡**：每个可选项是一对 `{1 个植物增益 + 1 个僵尸增难}`。选了某项，
  就同时获得这个植物增益（buff）并背上对应的僵尸增难（curse）。
- **随机配对**：每轮从「未满层的植物增益」与「未满层的僵尸增难」里**各随机抽取**
  组成配对（不是预定义固定配对）。词条库现为 2 植物增益 + 4 僵尸增难，数量不对等，
  随机配对天然容纳这种不对等，且以后新增词条自动纳入。
- **3 选 1**：每轮展示 3 个互不相同的配对，玩家挑 1 个。
- **可跳过**：弹窗带「跳过本轮」按钮；跳过则本轮不加任何词条、难度不上升。
- **出现时机**：在选卡界面**之前**——轮清 → 弹词条框 → 选定/跳过 → 才进选卡。
- **选择时暂停游戏**（`DeltaTime::SetPaused(true)`），与暂停菜单语义一致。
- **不跨轮去重**：同一配对可能多轮重复出现（只要两侧都未满层），属 roguelike 常态。

## 2. 词条分类（buff / curse）

| PerkType | 类别 | 满层上限 |
|---|---|---|
| `PLANT_DAMAGE_UP` | `PLANT_BUFF` | 10 |
| `PLANT_REGEN` | `PLANT_BUFF` | 5 |
| `ZOMBIE_HEALTH_UP` | `ZOMBIE_CURSE` | 10 |
| `ZOMBIE_DAMAGE_RESIST` | `ZOMBIE_CURSE` | 10 |
| `ZOMBIE_DAMAGE_UP` | `ZOMBIE_CURSE` | 9999（实际不限） |
| `ZOMBIE_INVULN_HITS` | `ZOMBIE_CURSE` | 2 |

植物增益侧两个都满层时（`PLANT_DAMAGE_UP` 10 层 + `PLANT_REGEN` 5 层），无法组成配对
→ `RollPerkPairings` 返回空 → UI 只显示「跳过本轮」。僵尸侧因 `ZOMBIE_DAMAGE_UP`
上限 9999，几乎永不为空。

## 3. 数据层改动（`Game/Perk/`）

### 3.1 `PerkType.h`

新增：

```cpp
enum class PerkCategory { PLANT_BUFF, ZOMBIE_CURSE };

struct PerkInfo {
    const char*  key;
    const char*  nameZh;
    const char*  descZh;
    float        perStack;
    int          maxStacks;
    PerkCategory category;   // 新增列：决定词条在配对中处于植物侧还是僵尸侧
};

struct PerkPairing {
    PerkType plant;   // 植物增益词条
    PerkType zombie;  // 僵尸增难词条
};
```

### 3.2 `SurvivalPerkManager.h` / `.cpp`

- `kPerks` 表每行补一个 `category`（按上表）。`static_assert` 不变。
- 新增 manager 成员（const，纯查询）：
  - `PerkCategory GetCategory(PerkType t) const { return GetInfo(t).category; }`
  - `std::vector<PerkType> AvailablePerks(PerkCategory cat) const;`
    返回该类别下 `GetStacks(t) < maxStacks` 的全部词条。
- 新增**自由函数**（声明在 `SurvivalPerkManager.h`，定义在 `.cpp`，便于单测）：

```cpp
// 随机生成至多 count 个互不相同的 {植物增益, 僵尸增难} 配对。
// 用 GameRandom（受 -Seed 控制，AutoTest 可复现）。
// 任一侧无可用词条 → 返回空。可用配对总数 < count → 返回全部。
std::vector<PerkPairing> RollPerkPairings(const SurvivalPerkManager& mgr, int count);
```

实现：构造 `availPlants × availZombies` 笛卡尔积成 `vector<PerkPairing>`，
`GameRandom::Shuffle`，截取前 `count`。互不相同由"笛卡尔积每项唯一 + 截取"自然保证。

> 决策记录：RNG 放进**自由函数**而非 `SurvivalPerkManager` 成员，保持 manager 仍是
> 纯数据（不依赖 `GameRandom`）；`count=3` 是 UI 策略，但 roll 逻辑放 Perk 模块是为了
> 单测，count 作参数传入。UI 侧（GameScene）只调用、不重复实现随机逻辑。

## 4. 触发与场景接线（`Game/Board.cpp`、`Game/GameScene.{h,cpp}`）

### 4.1 改触发点

`Board::OnSurvivalRoundClear()` 末尾：

```cpp
// 旧： if (mGameScene) mGameScene->BeginSurvivalCardSelect();
// 新：
if (mGameScene) mGameScene->BeginSurvivalPerkSelect();
```

`OnSurvivalRoundClear` 由最后一只僵尸 `Zombie::Die()` 中途调用。`BeginSurvivalPerkSelect`
只创建一个 UI 框（不遍历僵尸、不触碰 EntityManager），在该调用栈里安全。重活
（相机平移、重建选卡 UI、延后存档）仍在玩家点击后的 `BeginSurvivalCardSelect` 里发生，
此时已远离濒死僵尸调用栈——比现状更安全。

### 4.2 `GameScene` 新增

成员：
```cpp
std::weak_ptr<GameMessageBox> mPerkSelectBox;
std::vector<PerkPairing>      mCurrentPerkOffer;   // 本轮 offer（供 AutoTest dump）
bool                          mSurvivalPerkSelectActive = false;
```

方法：
- `void BeginSurvivalPerkSelect();`
  1. `mCurrentPerkOffer = RollPerkPairings(mBoard->GetPerkManager(), 3);`
  2. 组 `GameMessageBox::ButtonConfig`：每个 offer 一个「选择」按钮；末尾一个「跳过本轮」按钮。
     用 `TextConfig` 画每行的绿字(植物)/红字(僵尸)说明（含当前层数）。
  3. `mUIManager.CreateMessageBox(...)`，存进 `mPerkSelectBox`。
  4. `mSurvivalPerkSelectActive = true; DeltaTime::SetPaused(true);`
- `void ApplyPerkSelection(int index);`（按钮回调统一入口；`index<0` 或越界=跳过）
  1. 若 `index` 合法：`auto& mgr = mBoard->GetPerkManager(); mgr.AddPerk(offer.plant); mgr.AddPerk(offer.zombie);`
  2. `mSurvivalPerkSelectActive = false; mCurrentPerkOffer.clear();`
  3. `DeltaTime::SetPaused(false);`
  4. 链式：`BeginSurvivalCardSelect();`（box 由按钮 `autoClose=true` 自行关闭）

### 4.3 存档

词条在 `BeginSurvivalCardSelect` **之前**写入 `PerkManager`。`BeginSurvivalCardSelect`
里既有的 `mPendingSurvivalSave`（延后一帧 `SaveLevelData`）天然把新词条序列化进档。
**本设计不新增/改动任何存档代码。**

## 5. UI 外观（主人已授权由我定）

- 背景：复用 `GameMessageBox` 默认背景（`IMAGE_MESSAGEBOX`），`scale≈1.4`，居中
  `Vector(SCENE_WIDTH/2, SCENE_HEIGHT/2)`。若空间不足，改用更大的背景图或上调 scale。
- 标题：`u8"第 N 轮 · 选择强化"`（N=`mBoard->mSurvivalRound`）。
- 3 个选项行（纵向排列）。每行：
  - 绿字（`{53,191,61,255}`）：`u8"植物：<nameZh> <效果>（当前 X 层）"`
  - 红字（`{200,60,60,255}`）：`u8"僵尸：<nameZh> <效果>"`
  - 右侧「选择」按钮：`IMAGE_BUTTONSMALL`，回调 `ApplyPerkSelection(i)`，`autoClose=true`。
- 底部「跳过本轮」按钮：`IMAGE_BUTTONBIG`，回调 `ApplyPerkSelection(-1)`，`autoClose=true`。
- 效果文案直接取 `PerkInfo::descZh`（已含每层数值）；层数取 `GetStacks`。
- 具体像素坐标在实现期定，主人已授权。

## 6. AutoTest 验证（`Game/AutoTest/TestDriver.cpp`、新脚本）

- `dump_state` 增 `perkSelect` 块：
  ```json
  "perkSelect": { "active": true,
    "offers": [ {"plant":"PLANT_DAMAGE_UP","zombie":"ZOMBIE_HEALTH_UP"}, ... ] }
  ```
  非激活时 `active:false, offers:[]`。
- 新增两个 **test-only** 命令（仅 AutoTest 路径，对真实游戏零影响）：
  - `survival_perk_open`：强制 `GameScene::BeginSurvivalPerkSelect()`（须在生存关），
    免去 AutoTest 真打通一整轮的高成本。
  - `survival_perk_pick { "index": N }`：等价玩家点击第 N 个「选择」按钮；
    `index:-1`（或 `"skip"`）= 跳过。走与点击同一条 `ApplyPerkSelection` 路径。
- 新脚本 `autotest/scripts/smoke_perk_select.json`：
  进生存关 → `survival_perk_open` → `dump_state`（确认 `active:true` 且 3 个 offer）
  → 记录 offer[0] 的 plant/zombie → `survival_perk_pick {index:0}`
  → `dump_state`（确认该 plant、该 zombie 的 stacks 各 +1，`active:false`）→ `screenshot`。
- 独立小单测：`RollPerkPairings`
  - 正常态：返回 3 个、两侧合法、互不相同；
  - 植物侧全满层：返回空；
  - 仅 1 个可用配对时：返回 1 个不崩。
  （沿用 perk-system 既有的"clang++ 单独编 + 一次性 test"范式。）

## 7. 边界与决策记录

- **暂停期间死亡动画定格**：最后一只僵尸死亡动画会定格一瞬，换取干净的模态语义，
  主人已接受。
- **配对不跨轮去重**：同配对可重复出现，主人已接受。
- **两侧都无可用词条**（极端：植物两个都满层）：UI 只剩「跳过本轮」，玩家只能跳过。
- **选择期间退出/重开**：box 是 GameObject，随场景拆除销毁；未点选则未应用词条，无副作用。
- **不改动**：词条数值、三处生效钟点、`SurvivalPerkManager` 的存/读档逻辑、
  `GameInfoSaver`。

## 8. 影响文件清单

- `PlantVsZombies/Game/Perk/PerkType.h`（+category/+PerkCategory/+PerkPairing）
- `PlantVsZombies/Game/Perk/SurvivalPerkManager.h`（+AvailablePerks/+GetCategory/+RollPerkPairings 声明）
- `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp`（kPerks 补 category、实现新增 API）
- `PlantVsZombies/Game/Board.cpp`（`OnSurvivalRoundClear` 改调 `BeginSurvivalPerkSelect`）
- `PlantVsZombies/Game/GameScene.h` / `.cpp`（新成员 + `BeginSurvivalPerkSelect` / `ApplyPerkSelection`）
- `PlantVsZombies/Game/AutoTest/TestDriver.cpp`（dump_state perkSelect 块 + 两命令）
- `autotest/scripts/smoke_perk_select.json`（新脚本）
- `RollPerkPairings` 单测（临时 test，验证后不入库或入 autotest 辅助目录，按既有范式）
