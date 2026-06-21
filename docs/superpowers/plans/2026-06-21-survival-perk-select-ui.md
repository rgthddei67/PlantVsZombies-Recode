# 生存模式词条选择 UI 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 生存模式每轮清场后、进选卡前，弹出一个"3 选 1 随机配对（1 植物增益 + 1 僵尸增难），可跳过"的词条选择框。

**Architecture:** 复用现有 `GameMessageBox`（数据驱动按钮+文本）。数据层 `SurvivalPerkManager` 加 buff/curse 分类与纯函数 `RollPerkPairings`（用 `GameRandom`，`-Seed` 可复现）。`Board::OnSurvivalRoundClear` 改为先调 `GameScene::BeginSurvivalPerkSelect`；玩家点「选择/跳过」后由 `ApplyPerkSelection` 应用词条并链式调用既有 `BeginSurvivalCardSelect`。存档沿用 `BeginSurvivalCardSelect` 里既有的延后存档，无需改存档代码。

**Tech Stack:** C++17、CMake + vcpkg（clang-release 预设）、nlohmann/json、SDL2、自研 Vulkan 封装、AutoTest JSON 脚本驱动。

---

## 前置：构建环境（每个含构建的步骤都先做一次）

按 `CLAUDE.md`，所有构建/编译命令须在已导入 VS 开发环境的 PowerShell 里跑。一次性导入：

```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
```

工作根目录：`D:\PVZ\PlantsVsZombies\PlantVsZombies`（下称 `<repo>`）。源码在 `<repo>\PlantVsZombies\`，脚本在 `<repo>\autotest\scripts\`，可执行文件在 `<repo>\build\clang-release\PlantsVsZombies.exe`。

---

## File Structure（改动地图）

| 文件 | 责任 | 改动 |
|---|---|---|
| `PlantVsZombies/Game/Perk/PerkType.h` | 词条枚举 + 元数据结构 | 加 `PerkCategory`、`PerkInfo.category`、`PerkPairing` |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.h` | 词条聚合管理器接口 | 加 `GetCategory`/`AvailablePerks` 声明 + 自由函数 `RollPerkPairings` 声明 + `#include <vector>` |
| `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp` | 实现 | `kPerks` 补 `category` 列；实现 `AvailablePerks`、`RollPerkPairings` |
| `PlantVsZombies/Game/GameScene.h` | 游戏场景 | 加 3 个成员 + `BeginSurvivalPerkSelect`/`ApplyPerkSelection` + 2 个 AutoTest getter |
| `PlantVsZombies/Game/GameScene.cpp` | 实现 | 实现上述两方法 |
| `PlantVsZombies/Game/Board.cpp` | 关卡管理 | `OnSurvivalRoundClear` 改调 `BeginSurvivalPerkSelect` |
| `PlantVsZombies/Game/AutoTest/TestDriver.cpp` | AutoTest 命令 | 加 `survival_perk_open`/`survival_perk_pick` + `dump_state` 的 `perkSelect` 块 |
| `autotest/scripts/smoke_perk_select.json` | 端到端验证脚本 | 新建 |

---

## Task 1: 数据层 — 词条分类 + 随机配对函数

**Files:**
- Modify: `PlantVsZombies/Game/Perk/PerkType.h`
- Modify: `PlantVsZombies/Game/Perk/SurvivalPerkManager.h`
- Modify: `PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp`
- Test (throwaway): `<repo>/test_roll_perk_pairings.cpp`

- [ ] **Step 1: 写失败测试**

新建 `<repo>/test_roll_perk_pairings.cpp`：

```cpp
#include "PlantVsZombies/Game/Perk/SurvivalPerkManager.h"
#include <cassert>
#include <set>
#include <utility>
#include <cstdio>

int main() {
    GameRandom::SetSeed(42);

    // 1) 全新管理器：3 个 offer，植物侧=BUFF、僵尸侧=CURSE，互不相同
    SurvivalPerkManager fresh;
    auto offers = RollPerkPairings(fresh, 3);
    assert(offers.size() == 3);
    std::set<std::pair<int, int>> seen;
    for (const auto& o : offers) {
        assert(SurvivalPerkManager::GetInfo(o.plant).category  == PerkCategory::PLANT_BUFF);
        assert(SurvivalPerkManager::GetInfo(o.zombie).category == PerkCategory::ZOMBIE_CURSE);
        bool inserted = seen.insert({ static_cast<int>(o.plant), static_cast<int>(o.zombie) }).second;
        assert(inserted);   // 互不相同
    }

    // 2) 植物侧两个增益都满层 → 无法配对 → 返回空
    SurvivalPerkManager noPlant;
    for (int i = 0; i < 10; ++i) noPlant.AddPerk(PerkType::PLANT_DAMAGE_UP);
    for (int i = 0; i < 5;  ++i) noPlant.AddPerk(PerkType::PLANT_REGEN);
    assert(RollPerkPairings(noPlant, 3).empty());

    // 3) 仅 1 植物增益 + 1 僵尸增难可用 → 恰好 1 个确定配对
    SurvivalPerkManager one;
    for (int i = 0; i < 5;  ++i) one.AddPerk(PerkType::PLANT_REGEN);          // 回血满层，仅伤害可用
    for (int i = 0; i < 10; ++i) one.AddPerk(PerkType::ZOMBIE_HEALTH_UP);     // 满
    for (int i = 0; i < 10; ++i) one.AddPerk(PerkType::ZOMBIE_DAMAGE_RESIST); // 满
    for (int i = 0; i < 2;  ++i) one.AddPerk(PerkType::ZOMBIE_INVULN_HITS);   // 满
    // ZOMBIE_DAMAGE_UP 上限 9999，永远可用 → 僵尸侧仅它可用
    auto oneOffer = RollPerkPairings(one, 3);
    assert(oneOffer.size() == 1);
    assert(oneOffer[0].plant  == PerkType::PLANT_DAMAGE_UP);
    assert(oneOffer[0].zombie == PerkType::ZOMBIE_DAMAGE_UP);

    std::printf("ALL PASS\n");
    return 0;
}
```

- [ ] **Step 2: 编译测试，确认 RED**

在已导入 VS 环境的 PowerShell、`<repo>` 下运行：

```powershell
cl /nologo /std:c++17 /utf-8 /EHsc /I "build\clang-release\vcpkg_installed\x64-windows-static\include" /Fe:test_roll.exe test_roll_perk_pairings.cpp PlantVsZombies\Game\Perk\SurvivalPerkManager.cpp
```

Expected: 编译失败（`RollPerkPairings` 未声明、`PerkCategory` / `.category` 未定义）。这就是 RED。

- [ ] **Step 3: 改 `PerkType.h`，加分类与配对结构**

把 `PerkType.h` 的 `PerkInfo` 之后整体替换为（保留原 `enum class PerkType` 不动，只在其后追加）：

```cpp
enum class PerkCategory { PLANT_BUFF, ZOMBIE_CURSE };

struct PerkInfo {
    const char*  key;        // 存档稳定键名（不随 enum 顺序变）
    const char*  nameZh;     // 显示名（UI 用）
    const char*  descZh;     // 每层效果描述（UI 用）
    float        perStack;   // 每层数值
    int          maxStacks;  // 每词条独立上限（=1 即一次性词条）
    PerkCategory category;   // 配对归属：植物增益 / 僵尸增难
};

// 一个可选项 = 1 植物增益 + 1 僵尸增难（成对权衡）
struct PerkPairing {
    PerkType plant;
    PerkType zombie;
};
```

（即在原 `PerkInfo` 末尾加 `category` 字段，并在文件末尾追加 `PerkCategory`、`PerkPairing`。注意 `PerkCategory` 的定义须在 `PerkInfo` 之前——把 `enum class PerkCategory` 放到 `struct PerkInfo` 上方。）

- [ ] **Step 4: 改 `SurvivalPerkManager.h`，加查询 + 自由函数声明**

顶部 include 区加：

```cpp
#include <vector>
```

`public:` 区，在 `static const PerkInfo& GetInfo(PerkType type);` 之后加：

```cpp
    PerkCategory GetCategory(PerkType type) const { return GetInfo(type).category; }
    std::vector<PerkType> AvailablePerks(PerkCategory cat) const;  // 该类别下 stacks < maxStacks 的词条
```

类定义结束的 `};` 之后（文件末尾）加自由函数声明：

```cpp
// 随机生成至多 count 个互不相同的 {植物增益, 僵尸增难} 配对，用 GameRandom（-Seed 可复现）。
// 任一侧无可用词条 → 返回空；可用配对总数 < count → 返回全部。
std::vector<PerkPairing> RollPerkPairings(const SurvivalPerkManager& mgr, int count);
```

- [ ] **Step 5: 改 `SurvivalPerkManager.cpp`，补分类列 + 实现**

顶部 include 区（在 `#include <nlohmann/json.hpp>` 之后）加：

```cpp
#include "../../GameRandom.h"
#include <algorithm>
```

把匿名命名空间里的 `kPerks` 表整体替换为（每行末尾补 `category`）：

```cpp
    const PerkInfo kPerks[] = {
        { "PLANT_DAMAGE_UP",      u8"全体植物伤害", u8"每层使全体植物伤害 +10%",            0.10f, 10,   PerkCategory::PLANT_BUFF },
        { "ZOMBIE_HEALTH_UP",     u8"僵尸血量",     u8"每层使僵尸血量 +20%",                0.20f, 10,   PerkCategory::ZOMBIE_CURSE },
        { "ZOMBIE_DAMAGE_RESIST", u8"僵尸免伤",     u8"每层使僵尸受到伤害 -5%（最高 50%）", 0.05f, 10,   PerkCategory::ZOMBIE_CURSE },
        { "ZOMBIE_DAMAGE_UP",     u8"僵尸伤害",     u8"每层使僵尸对植物伤害 +5%（不限层）", 0.05f, 9999, PerkCategory::ZOMBIE_CURSE },
        { "ZOMBIE_INVULN_HITS",   u8"僵尸前N次免伤", u8"每层使僵尸出生后前 10 次受击免伤（最多 2 层）", 10.0f, 2, PerkCategory::ZOMBIE_CURSE },
        { "PLANT_REGEN",          u8"植物回血",     u8"每 5 秒回 25 HP；满 5 层解锁过量治疗至 3 倍上限", 25.0f, 5, PerkCategory::PLANT_BUFF },
    };
```

在 `void SurvivalPerkManager::Clear()` 实现之后（任意成员实现之间均可）加 `AvailablePerks` 实现：

```cpp
std::vector<PerkType> SurvivalPerkManager::AvailablePerks(PerkCategory cat) const {
    std::vector<PerkType> out;
    for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
        PerkType t = static_cast<PerkType>(i);
        const PerkInfo& info = GetInfo(t);
        if (info.category == cat && GetStacks(t) < info.maxStacks)
            out.push_back(t);
    }
    return out;
}
```

文件末尾加自由函数实现：

```cpp
std::vector<PerkPairing> RollPerkPairings(const SurvivalPerkManager& mgr, int count) {
    std::vector<PerkType> plants  = mgr.AvailablePerks(PerkCategory::PLANT_BUFF);
    std::vector<PerkType> zombies = mgr.AvailablePerks(PerkCategory::ZOMBIE_CURSE);

    std::vector<PerkPairing> all;
    for (PerkType p : plants)
        for (PerkType z : zombies)
            all.push_back(PerkPairing{ p, z });

    GameRandom::Shuffle(all);                       // 笛卡尔积每项唯一，洗牌后截取即"互不相同"
    if (static_cast<int>(all.size()) > count)
        all.resize(count);
    return all;
}
```

- [ ] **Step 6: 重新编译测试，确认 GREEN**

```powershell
cl /nologo /std:c++17 /utf-8 /EHsc /I "build\clang-release\vcpkg_installed\x64-windows-static\include" /Fe:test_roll.exe test_roll_perk_pairings.cpp PlantVsZombies\Game\Perk\SurvivalPerkManager.cpp
.\test_roll.exe
```

Expected: 编译通过，运行打印 `ALL PASS`，退出码 0。

- [ ] **Step 7: 全量构建（验证 `static_assert` / `kPerks` 在真实工程仍编译、0 警告）**

```powershell
cmake --build --preset clang-release
```

Expected: 0 error、0 warning（`static_assert` 依旧成立，因为 `kPerks` 行数未变）。

- [ ] **Step 8: 清理一次性测试产物**

```powershell
Remove-Item -Force test_roll_perk_pairings.cpp, test_roll.exe, *.obj -ErrorAction SilentlyContinue
```

- [ ] **Step 9: 提交**

```powershell
git add PlantVsZombies/Game/Perk/PerkType.h PlantVsZombies/Game/Perk/SurvivalPerkManager.h PlantVsZombies/Game/Perk/SurvivalPerkManager.cpp
git commit -m @'
feat(perk): 词条加 buff/curse 分类 + RollPerkPairings 随机配对

PerkInfo 加 category 列，新增 AvailablePerks/RollPerkPairings(GameRandom，-Seed 可复现)；
单测覆盖 3 选 1 互不相同、植物侧满层返回空、唯一配对确定。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 2: GameScene 词条选择框 + Board 触发点

**Files:**
- Modify: `PlantVsZombies/Game/GameScene.h`
- Modify: `PlantVsZombies/Game/GameScene.cpp`
- Modify: `PlantVsZombies/Game/Board.cpp:655-656`

- [ ] **Step 1: `GameScene.h` 加 include、成员、方法声明**

顶部 include 区（若尚无）加：

```cpp
#include "Perk/PerkType.h"
```

`public:` 区，在 `void BeginSurvivalCardSelect();`（约 72 行）之后加：

```cpp
    void BeginSurvivalPerkSelect();          // 轮清后弹词条选择框（选卡之前）
    void ApplyPerkSelection(int index);      // index<0 或越界 = 跳过；应用后链式进选卡
    // AutoTest 内省接口（对真实游戏无副作用）
    bool IsPerkSelectActive() const { return mSurvivalPerkSelectActive; }
    const std::vector<PerkPairing>& GetCurrentPerkOffer() const { return mCurrentPerkOffer; }
```

`private:` 区，在 `std::weak_ptr<GameMessageBox> mMenu;`（约 103 行）附近加：

```cpp
    std::weak_ptr<GameMessageBox> mPerkSelectBox;
    std::vector<PerkPairing>      mCurrentPerkOffer;        // 本轮展示的配对（AutoTest dump 用）
    bool                          mSurvivalPerkSelectActive = false;
```

- [ ] **Step 2: `GameScene.cpp` 加 include 并实现两方法**

顶部 include 区（在 `#include "../UI/GameMessageBox.h"` 附近）加：

```cpp
#include "Perk/SurvivalPerkManager.h"
```

在 `void GameScene::BeginSurvivalCardSelect()` 实现之前，新增：

```cpp
void GameScene::BeginSurvivalPerkSelect()
{
    if (!mBoard) return;

    mCurrentPerkOffer = RollPerkPairings(mBoard->GetPerkManager(), 3);
    mSurvivalPerkSelectActive = true;
    DeltaTime::SetPaused(true);

    auto& pm = mBoard->GetPerkManager();

    std::vector<GameMessageBox::ButtonConfig> buttons;
    std::vector<GameMessageBox::SliderConfig> sliders;
    std::vector<GameMessageBox::TextConfig>   texts;

    // —— 首版坐标，Task 4 截图后微调 ——（场景中心 550,300）
    const float baseY = 250.0f;
    const float rowH  = 80.0f;
    const glm::vec4 green{ 53, 191, 61, 255 };
    const glm::vec4 red  { 200, 60, 60, 255 };

    for (size_t i = 0; i < mCurrentPerkOffer.size(); ++i) {
        const PerkPairing& pr = mCurrentPerkOffer[i];
        const PerkInfo& bp = SurvivalPerkManager::GetInfo(pr.plant);
        const PerkInfo& cz = SurvivalPerkManager::GetInfo(pr.zombie);
        float y = baseY + rowH * static_cast<float>(i);

        std::string plantLine = std::string(u8"植物：") + bp.nameZh + u8"  " + bp.descZh
            + u8"（当前 " + std::to_string(pm.GetStacks(pr.plant)) + u8" 层）";
        std::string zombieLine = std::string(u8"僵尸：") + cz.nameZh + u8"  " + cz.descZh;

        texts.push_back({ Vector(380, y - 16), 14, plantLine,  green });
        texts.push_back({ Vector(380, y + 6),  14, zombieLine, red });

        int idx = static_cast<int>(i);
        buttons.push_back({ u8"选择", Vector(760, y - 6), Vector(125 * 0.8f, 52 * 0.8f), 16,
            [this, idx]() { this->ApplyPerkSelection(idx); },
            ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
    }

    buttons.push_back({ u8"跳过本轮", Vector(550, 500), Vector(213 * 0.9f, 50 * 0.9f), 20,
        [this]() { this->ApplyPerkSelection(-1); },
        ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

    std::string title = std::string(u8"第 ") + std::to_string(mBoard->mSurvivalRound) + u8" 轮 · 选择强化";

    mPerkSelectBox = mUIManager.CreateMessageBox(
        Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2),
        "", buttons, sliders, texts, title, 1.5f);
}

void GameScene::ApplyPerkSelection(int index)
{
    if (mBoard && index >= 0 && index < static_cast<int>(mCurrentPerkOffer.size())) {
        const PerkPairing& pr = mCurrentPerkOffer[index];
        auto& pm = mBoard->GetPerkManager();
        pm.AddPerk(pr.plant);
        pm.AddPerk(pr.zombie);
    }

    mSurvivalPerkSelectActive = false;
    mCurrentPerkOffer.clear();
    DeltaTime::SetPaused(false);

    // 词条已写入 PerkManager；进选卡子流程，其内既有的延后存档天然包含新词条。
    // 选择框由被点按钮的 autoClose=true 自行 Close（延后到帧末销毁，安全）。
    BeginSurvivalCardSelect();
}
```

- [ ] **Step 3: `Board.cpp` 改触发点（约 655-656 行）**

把：

```cpp
	// 通知场景重新进入选卡子流程（植物/阳光保留）
	if (mGameScene)
		mGameScene->BeginSurvivalCardSelect();
```

改为：

```cpp
	// 通知场景：先弹词条选择（选卡之前）；玩家选定/跳过后由 ApplyPerkSelection 链式进选卡。
	if (mGameScene)
		mGameScene->BeginSurvivalPerkSelect();
```

- [ ] **Step 4: 构建**

```powershell
cmake --build --preset clang-release
```

Expected: 0 error、0 warning。

- [ ] **Step 5: 提交**

```powershell
git add PlantVsZombies/Game/GameScene.h PlantVsZombies/Game/GameScene.cpp PlantVsZombies/Game/Board.cpp
git commit -m @'
feat(survival): 轮间词条选择框 BeginSurvivalPerkSelect + ApplyPerkSelection

OnSurvivalRoundClear 改为先弹 3 选 1 配对框（可跳过），选定后 AddPerk 两侧并
链式进 BeginSurvivalCardSelect；选择期间暂停。复用 GameMessageBox。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 3: AutoTest 钩子（命令 + dump 块）

**Files:**
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`

- [ ] **Step 1: 加两个 test-only 命令**

在 `add_perk` 命令块（`if (op == "add_perk") { ... }`，约 286-294 行）之后插入：

```cpp
	if (op == "survival_perk_open") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("survival_perk_open: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->mIsSurvival) { Fail("survival_perk_open: 非生存关"); return false; }
		gs->BeginSurvivalPerkSelect();
		return true;
	}
	if (op == "survival_perk_pick") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("survival_perk_pick: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->IsPerkSelectActive()) { Fail("survival_perk_pick: 当前无词条选择"); return false; }
		int index = cmd.value("index", -1);   // -1 = 跳过
		gs->ApplyPerkSelection(index);
		return true;
	}
```

- [ ] **Step 2: `dump_state` 加 `perkSelect` 块**

在 `dump_state` 里 `out["perks"] = perks;` 所在花括号块结束之后（约 370 行，`out["perks"]` 块的 `}` 之后、写文件之前）插入：

```cpp
		{
			nlohmann::json psel;
			psel["active"] = gs->IsPerkSelectActive();
			nlohmann::json offers = nlohmann::json::array();
			for (const PerkPairing& pr : gs->GetCurrentPerkOffer()) {
				offers.push_back({
					{ "plant",  SurvivalPerkManager::GetInfo(pr.plant).key },
					{ "zombie", SurvivalPerkManager::GetInfo(pr.zombie).key },
				});
			}
			psel["offers"] = offers;
			out["perkSelect"] = psel;
		}
```

- [ ] **Step 3: 构建**

```powershell
cmake --build --preset clang-release
```

Expected: 0 error、0 warning。（`PerkPairing`、`SurvivalPerkManager` 经 `Board.h` 传递可见；`GameScene` 已 include。）

- [ ] **Step 4: 提交**

```powershell
git add PlantVsZombies/Game/AutoTest/TestDriver.cpp
git commit -m @'
test(autotest): 加 survival_perk_open/survival_perk_pick + dump_state.perkSelect

强制开框/按下标选定(或-1跳过)，dump 暴露 active + offers[{plant,zombie}]，
免去真打通一轮即可验证选择/应用路径。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 4: 端到端 smoke 脚本 + 运行验证

**Files:**
- Create: `autotest/scripts/smoke_perk_select.json`

- [ ] **Step 1: 新建脚本**

`<repo>/autotest/scripts/smoke_perk_select.json`：

```json
{
  "commands": [
    { "op": "goto_level", "level": 1000 },
    { "op": "wait_state", "state": "CHOOSE_CARD" },
    { "op": "survival_perk_open" },
    { "op": "dump_state", "name": "perk_select_open.json" },
    { "op": "screenshot", "name": "perk_select_open.png" },
    { "op": "survival_perk_pick", "index": 0 },
    { "op": "dump_state", "name": "perk_select_after.json" },
    { "op": "quit" }
  ]
}
```

- [ ] **Step 2: 运行脚本（固定种子）**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perk_select.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected: 退出码 `0`。产物在 `build\clang-release\autotest\out\smoke_perk_select\`。

- [ ] **Step 3: 读 `perk_select_open.json`，验证 offer 合法**

用 Read 打开 `build\clang-release\autotest\out\smoke_perk_select\perk_select_open.json`。
断言（人工核对）：
- `perkSelect.active == true`
- `perkSelect.offers` 长度 == 3，且互不相同
- 每个 offer 的 `plant` ∈ {`PLANT_DAMAGE_UP`,`PLANT_REGEN`}，`zombie` ∈ {`ZOMBIE_HEALTH_UP`,`ZOMBIE_DAMAGE_RESIST`,`ZOMBIE_DAMAGE_UP`,`ZOMBIE_INVULN_HITS`}
- 记下 `offers[0].plant` 与 `offers[0].zombie`（设为 P0、Z0）

- [ ] **Step 4: 读 `perk_select_after.json`，验证选定已应用**

用 Read 打开 `perk_select_after.json`。断言：
- `perkSelect.active == false`，`perkSelect.offers` 为空数组
- `perks.stacks[P0] == 1` 且 `perks.stacks[Z0] == 1`（P0、Z0 来自 Step 3；若 P0/Z0 不同词条则两者各为 1）
- 其余四个词条 `stacks == 0`

- [ ] **Step 5: 读截图做视觉自检**

用 Read 打开 `perk_select_open.png`，确认：标题「第 1 轮 · 选择强化」、3 行绿/红文字、3 个「选择」按钮、1 个「跳过本轮」按钮都在框内、未越界。若坐标越界或重叠，回 Task 2 Step 2 调 `baseY`/`rowH`/各 `Vector(...)`，重构建、重跑本 Task，直至版面正常。

- [ ] **Step 6: 提交**

```powershell
git add autotest/scripts/smoke_perk_select.json
git commit -m @'
test(autotest): smoke_perk_select 端到端验证词条选择框

进生存 → 强制开框 → dump 校验 3 offer 合法 → 选 index0 → dump 校验两侧 stacks 各 +1。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
'@
```

---

## Task 5: 回归与收尾

- [ ] **Step 1: 回归既有 perk smoke（确认中性默认 + 旧路径未坏）**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_perks.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected: 退出码 `0`。用 Read 打开 `build\clang-release\autotest\out\smoke_perks\perks.json`，确认 `perks` 聚合量与历史一致（豌豆相关倍率、`perkSelect.active == false`）。

- [ ] **Step 2: 跑通用 demo 验收脚本，确认非生存路径零回归**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\demo_peashooter.json -Seed 42
$LASTEXITCODE
Pop-Location
```

Expected: 退出码 `0`（非生存关 `OnSurvivalRoundClear` 早返回，触发点改动不影响冒险关）。

- [ ] **Step 3: 最终人工确认清单**
- 选择框只在生存关、轮清后出现（`OnSurvivalRoundClear` 首行 `if(!mIsSurvival) return;`）。
- 选/跳过后能正常进入选卡（`BeginSurvivalCardSelect` 链式调用）。
- 暂停在选定后解除（`DeltaTime::SetPaused(false)`）。

---

## Self-Review（写完计划后的自检）

**1. Spec 覆盖**
- 成对权衡（buff+curse）→ Task 1（category）+ Task 2（`ApplyPerkSelection` 两侧 AddPerk）。✅
- 随机配对、未满层抽取池 → Task 1（`AvailablePerks` + `RollPerkPairings`）。✅
- 3 选 1 → Task 2（`RollPerkPairings(...,3)` + 每 offer 一按钮）。✅
- 可跳过 → Task 2（「跳过本轮」→ `ApplyPerkSelection(-1)`）。✅
- 选卡之前出现 → Task 2（`OnSurvivalRoundClear` → 开框 → 选后才 `BeginSurvivalCardSelect`）。✅
- 选择时暂停 → Task 2（`SetPaused(true/false)`）。✅
- 不跨轮去重 → 设计天然（每轮独立 roll，无 seen 集合）。✅
- 存档含新词条 → Task 2（词条先于 `BeginSurvivalCardSelect` 写入；无新存档代码）。✅
- AutoTest（dump perkSelect + 两命令 + 脚本）→ Task 3 + Task 4。✅
- `RollPerkPairings` 单测 → Task 1 Step 1-6。✅

**2. 占位符扫描**：无 TBD/TODO；每个代码步骤含完整代码与确切命令。✅

**3. 类型/命名一致性**：`PerkCategory`/`PerkPairing`/`RollPerkPairings`/`AvailablePerks`/`BeginSurvivalPerkSelect`/`ApplyPerkSelection`/`IsPerkSelectActive`/`GetCurrentPerkOffer`/`mCurrentPerkOffer`/`mSurvivalPerkSelectActive`/`mPerkSelectBox` 在 Task 1-4 间用法一致。`offers[].plant/zombie` 键名在 Task 3 产出与 Task 4 校验间一致。✅
