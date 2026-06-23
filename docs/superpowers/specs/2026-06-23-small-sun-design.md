# 小阳光 (SmallSun) 设计

日期：2026-06-23
状态：已批准，待实现

## 目标

为游戏新增一个「小阳光」收集物：行为与 `Game/Sun.h` 中的 `Sun` 类**完全相同**，仅两点不同——
- 视觉缩放从普通阳光的 `0.85` 缩小到 **`0.6`**
- 阳光价值从 `25` 降为 **`15`**

并补齐相关创建函数、接线到 `SunShroom`（幼年产小阳光、长大产普通阳光）、适配 `GameInfoSaver` 存档。

## 背景 / 现状

- `Sun : public Coin`（`Game/Sun.h`，header-only，逻辑全部 inline）。`SunPoint = 25`，构造函数把固定点击碰撞箱 `Vector(55,55)` 传给 `Coin`，`scale` 仅影响视觉。
- `Board::CreateSun(...)` 三连（`Vector` 版 / `float x,y` 版 / `CreateSunWithID`）创建普通阳光，scale 写死 `0.85`，通过 `mEntityManager.AddCoin` / `AddCoinWithID` 注册。
- `SunShroom`（新增、未提交）当前在 `PlantUpdate` 中无条件调用 `mBoard->CreateSun(...)`。经典 PvZ 中幼年阳光菇产小阳光、成熟后产普通阳光。
- `GameInfoSaver`：序列化时遍历所有 coin，用 `dynamic_cast<Sun*>(coin)` 识别阳光、`dynamic_cast<Trophy*>(coin)` 识别奖杯；读档时按 `id` 走 `CreateSunWithID` 或 `CreateSun` 重建，并恢复 `animTrack` / `animFrame`。

## 设计

### 1. 新类 `SmallSun : public Sun`

约 10 行的子类。构造函数转发到 `Sun` 构造函数，传 `scale = 0.6f`、`tag = "SmallSun"`，并在函数体内设 `SunPoint = 15`。其余全部继承不改：抛物线 / 匀速下落、自动收集、点击→`AddSun(SunPoint)`、`Start` / `Update` 覆盖。不复制任何逻辑。碰撞箱保持 `55×55`（仍易点击），符合「其他功能相同」。

**放置位置：直接写在 `Sun.h` 中（`Sun` 类下方）**，而非新建文件。原因：`GameInfoSaver.cpp` 需要 `dynamic_cast<SmallSun*>`，要求该类型**完整可见**；凡能看到 `Sun` 的地方都已包含 `Sun.h`，因此零新增 `#include`、零「incomplete type」风险。

### 2. `Board` 创建函数（镜像现有 `CreateSun` 三连）

- `SmallSun* CreateSmallSun(const Vector& position, bool needAnimation = false)` — scale 0.6，经 `mEntityManager.AddCoin` 注册
- `SmallSun* CreateSmallSun(float x, float y, bool needAnimation = false)` — 重载，转调上一个
- `SmallSun* CreateSmallSunWithID(const Vector& pos, bool fromSky, int id)` — 走 `AddCoinWithID`（读档重建路径）

声明加入 `Board.h`；`Board.cpp` 已（经 `Sun.h`）可见 `SmallSun`，无需额外 include。

### 3. SunShroom 接线（`SunShroom::PlantUpdate`）

把现有产阳光调用按 `mIsGrown` 分支：未长大 → `CreateSmallSun(...)`，已长大 → `CreateSun(...)`。

### 4. 存档适配（`GameInfoSaver`）— 关键点

由于 `SmallSun` *is-a* `Sun`，存档序列化循环中现有的 `dynamic_cast<Sun*>(coin)` **已经**能捕获小阳光，它们与普通阳光同在 `"suns"` 数组里。因此：

- **写档（约 `GameInfoSaver.cpp:208-223`）**：仅新增一个字段 `s["small"] = (dynamic_cast<SmallSun*>(coin) != nullptr)`。
- **读档（约 `GameInfoSaver.cpp:470-490`）**：读取 `bool small = s.value("small", false)`；为 `true` 时走 `CreateSmallSun` / `CreateSmallSunWithID`，否则走原有 `CreateSun` / `CreateSunWithID` 路径。旧存档无 `"small"` 字段 → 默认 `false` → 还原为普通阳光，**完全向后兼容**。

## 不做（YAGNI）

- 不新增 `CoinType` 枚举值（存档靠 dynamic_cast + flag 区分，不靠 CoinType）
- 不新增纹理 / 动画（复用 `ANIM_SUN`）
- 不新增卡牌 / 图鉴条目

## 验收

- 构建：`clang-release` 0 warning / 0 error。
- AutoTest：小阳光产出、点击收集 +15 阳光、视觉明显小于普通阳光；存档→读档后小阳光仍为 15 价值且尺寸正确（不退化成普通 25 阳光）；旧存档读取不崩、阳光为普通 25。
- 回归：普通阳光行为、价值、尺寸不变。
