# 生存模式「词条」查看面板 — 设计

日期：2026-06-22
范围：仅生存模式（无尽）。普通关卡行为零改动。
落点：`PlantVsZombies/Game/GameScene.cpp` / `.h`，外加一个 AutoTest 冒烟脚本。

## 目标

在生存模式中加一个独立按钮（样式与右上角「主菜单」按钮一致），点击后弹出一个信息面板，
展示玩家**已经选择的词条**及**各自的选择次数（层数）**，外加一行总计。面板沿用现有
「轮间选词条 UI」（`BeginSurvivalPerkSelect`）的纯色面板风格，但采用**固定面板尺寸 +
字号自动缩放**以适配内容，绝不溢出。打开时**暂停游戏**（与主菜单一致）。

设计三处分叉已与主人确认：
1. 显示内容：**只列已选词条（stacks>0）+ 顶部一行总计**。
2. 「自动调整字号」= **固定面板，字号自动缩小**（与选词条 UI 的"固定字号、撑大盒子"相反）。
3. 打开面板时**暂停**（同主菜单 / 同选词条模态）。

## 现状锚点（已勘察）

- 右上角按钮列：`OnEnter` 内用 `mUIManager.CreateButton(pos,size)` + `SetText` +
  `SetImageKeys(IMAGE_BUTTONSMALL…)` 建按钮。「主菜单」在 `(990,-5)`，速度键在 `(990,45)`，
  尺寸均为 `(125·0.9, 52·0.9)`，绿字配色。第三个按钮自然落在 `(990,95)`。
- 生存判定：`mBoard->mIsSurvival`，在 `Board` 构造函数（`Board.cpp:26`，`level==SURVIVAL_ENDLESS_LEVEL`）
  即设好，早于 `OnEnter` 的按钮创建，无时序隐患。
- 词条数据：`SurvivalPerkManager`（`mBoard->GetPerkManager()`）。`GetStacks(type)` 即"选择次数/层数"
  （每次 `AddPerk` +1）；`GetInfo(type)` 给 `nameZh`/`descZh`/`category`/`maxStacks`；
  遍历 `0..PerkType::COUNT` 可枚举全部 6 个词条。
- 面板原语：`UIManager::CreateMessageBox(pos, message, buttons, sliders, texts, title="",
  scale=1.0f, backgroundImageKey=IMAGE_MESSAGEBOX, explicitSize=zero)`。传
  `backgroundImageKey=""` + 非零 `explicitSize` → 画**纯色面板**（与选词条框同款，规避墓碑纹理花边内缩导致的文字溢出）。
- `TextConfig{ Vector pos, float size, std::string text, glm::vec4 color, std::string font=FONT_FZCQ }`。
  文字坐标是**绝对逻辑坐标**，由调用方算好放进面板矩形内（与 `BeginSurvivalPerkSelect` 一致）。
- 量宽原语：`TTF_Font* f = ResourceManager::GetInstance().GetFont(FONT_FZCQ, fontSize); TTF_SizeUTF8(f, s, &w, &h);`
  （取不到字体时按 `s.size()*fontSize*0.5f` 兜底）。

## 组件设计

### 1. 「词条」按钮（GameScene::OnEnter）

在速度按钮（`button2`）之后追加：

```cpp
if (mBoard->mIsSurvival) {
    auto b = mUIManager.CreateButton(Vector(990, 95), Vector(125 * 0.9f, 52 * 0.9f));
    mPerkViewButton = b;
    b->SetText(u8"词条");
    b->SetAsCheckbox(false);
    b->SetTextColor(glm::vec4{ 53, 191, 61, 255 });
    b->SetHoverTextColor(glm::vec4{ 53, 240, 61, 255 });
    b->SetImageKeys(IMAGE_BUTTONSMALL, IMAGE_BUTTONSMALL, IMAGE_BUTTONSMALL, IMAGE_BUTTONSMALL);
    b->SetClickCallBack([this](bool) { this->OpenPerkView(); });
}
```

仅生存模式创建 → 普通关卡完全无此按钮，零改动。

### 2. GameScene::OpenPerkView()

镜像 `BeginSurvivalPerkSelect` 的结构：

1. **守卫**：`if (!mBoard) return;` 以及
   `if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive) return;`
   （不与暂停菜单 / 选词条模态 / 自身重复叠加）。
2. 置 `mPerkViewActive = true; DeltaTime::SetPaused(true);`
3. **收集已选词条**：遍历 `t in 0..PerkType::COUNT`，`int n = pm.GetStacks(t);` 若 `n>0`
   收集 `{ GetInfo(t).descZh, GetInfo(t).category, n }`。同时累加 `distinct`（种类数）、`total`（Σ层数）。
4. **顶部总计行**：`已强化：X 种词条 · 累计 Y 层`。
   - 空态（无任何已选）：仅一行 `尚未选择任何强化词条`，不渲染词条明细行。
5. **每词条一行**：文案 `· {descZh}（已选 N 次）`；颜色 `PLANT_BUFF→绿{53,191,61,255}`，
   `ZOMBIE_CURSE→红{200,60,60,255}`（与选词条框同调色板）。
6. **固定面板 + 字号自动缩放**（见下「算法」）。
7. **关闭按钮**：底部居中一个 `u8"关闭"`，`autoClose=true`，回调 `this->ClosePerkView()`，
   贴图 `IMAGE_BUTTONBIG`（与选词条框「跳过本轮」同款）。
8. 建框：`mPerkViewBox = mUIManager.CreateMessageBox(Vector(cx,cy), "", buttons, {}, texts,
   "", 1.0f, "", Vector(boxW,boxH));`

### 3. 字号自动缩放算法

固定面板（逻辑像素，场景 1100×600，居中于 `(550,300)`）：

```
boxW = 560, boxH = 420
padX = 30, padY = 26
closeBtnSize = (160, 44), closeGap = 18
innerW  = boxW - 2*padX                              // 500
availH  = boxH - 2*padY - closeGap - closeBtnSize.y  // 标题+词条行可用高度
```

候选字号从大到小线性搜索（`rowFont` 18→10，`titleFont = rowFont + 4`）：

```
for rowFont in [18 .. 10]:
    titleFont = rowFont + 4
    rowLineH   = round(rowFont   * 1.4)
    titleLineH = round(titleFont * 1.4)
    titleGap   = round(rowFont * 0.9)
    rowGap     = round(rowFont * 0.5)
    N = 已选词条行数（空态 N=0，只有标题行）
    contentH = titleLineH + (N>0 ? titleGap + N*rowLineH + (N-1)*rowGap : 0)
    maxW = max( measureW(标题, titleFont), 各词条行 measureW(line, rowFont) )
    if contentH <= availH and maxW <= innerW:
        选定该字号; break
否则用 floor=10（极端长串容忍轻微挤压，仍优于溢出）
```

选定字号后单趟生成 `TextConfig`：内容块在 `availH` 区域内**垂直居中**
（`blockTop = boxTop + padY + (availH - contentH)/2`），词条少时不至于孤悬顶部。
标题水平居中，词条行左对齐于 `boxLeft + padX`。关闭按钮居中于 `boxTop + boxH - padY - closeBtnSize.y`。

> 备注：当前 6 词条、每个最多一行，最坏 1 标题 + 6 行，18 号字也能塞进 420 高，
> 字号缩放主要兜底**长描述串的宽度**与未来新增词条的高度。

### 4. GameScene::ClosePerkView()

```cpp
void GameScene::ClosePerkView() {
    mPerkViewActive = false;
    DeltaTime::SetPaused(false);
    // 面板由「关闭」按钮 autoClose=true 自行 Close（延后帧末销毁，安全），同 ApplyPerkSelection。
}
```

### 5. 暂停卫生 / OpenMenu 守卫

`OpenMenu()` 顶部追加 `if (mPerkViewActive) return;`，与既有
`if (mSurvivalPerkSelectActive) return;` 对称，避免主菜单在面板下叠开。
速度按钮已有 `if (DeltaTime::IsPaused()) return;` 守卫，暂停期间天然不响应，无需改动。

### 6. GameScene.h 增量

```cpp
void OpenPerkView();
void ClosePerkView();
std::weak_ptr<Button>         mPerkViewButton;
std::weak_ptr<GameMessageBox> mPerkViewBox;
bool                          mPerkViewActive = false;
```

### 7. OnExit 清理

追加 `mPerkViewButton.reset();`（与 `mMainMenuButton.reset()` 并列）。
`mPerkViewBox` 是 weak_ptr，框随场景对象销毁，无需显式重置。

## 验证（AutoTest）

新增 `autotest/scripts/smoke_perk_view.json`：

1. `goto_level` 进生存关 → `wait_state` 至游戏中。
2. 用既有 `survival_perk_open` / `survival_perk_pick` 选 1–2 轮词条（驱动 `AddPerk`）。
3. `click` 命中 `(990, 95)` 打开面板（合成输入走真实点击路径）。
4. `screenshot` → 用 Read 直接核验：面板纯色、总计行正确、各词条行配色（绿/红）与「已选 N 次」、
   字号未溢出可视边框、关闭按钮在位。
5. `click` 命中关闭按钮 → 面板消失、游戏恢复（`screenshot` 复核）。

可选增强（非必须）：`dump_state` 增加 `perkView` 字段（已选词条 key + 次数列表）供断言。
本轮先以截图核验为准；若主人要可断言化，再加 dump 字段。

## 不做（YAGNI）

- 不做滚动（`GameMessageBox` 无滚动；字号缩放已覆盖容量）。
- 不做未选词条的灰显（主人选「只列已选 + 总计」）。
- 不做多列 / 文字换行布局。
- 不改普通模式任何路径。

## 风险与对策

- **字号过小可读性**：floor=10 兜底；当前词条规模下实际几乎不会触底。截图核验把关。
- **暂停叠态**：三向守卫（菜单 / 选词条 / 本面板互斥）+ 速度键既有暂停守卫，避免双重 SetPaused 错乱。
- **面板坐标对齐**：复用选词条框已验证的「纯色面板 + 绝对坐标文字严格对齐」做法，规避墓碑花边溢出。
