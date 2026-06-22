# 生存模式「词条」查看面板 — 分页扩展 设计

日期：2026-06-22
范围：仅生存模式（无尽）的词条查看面板（`OpenPerkView`）。普通关卡、轮间选词条 UI、其他面板行为零改动。
落点：`PlantVsZombies/Game/GameScene.cpp` / `.h`，外加扩展一个 AutoTest 冒烟脚本。
前置：本扩展建立在已合并的「词条查看面板」之上（`5a68fe1`，见 `2026-06-22-survival-perk-view-panel-design.md`）。

## 目标

当前 `OpenPerkView` 把所有 distinct 词条（`stacks>0`）一次性塞进固定 560×420 面板，靠字号
18→10 自适应缩放避免溢出。词条种类多时字号被压得很小。本扩展改为**分页**：

- 每页最多展示 **5 个 distinct 词条**（重复选择已折叠为「已选 N 次」，不额外占行）。
- distinct 词条数 > 5 时分页；超出部分进入下一页。
- 当**还有下一页**时，底部出现「下一页」按钮；当**不在第一页**时（`page>0`），出现「上一页」按钮。
- 其余功能不变：暂停守卫、面板尺寸、纯色面板风格、0 词条提示、`OpenMenu` 守卫等。

UX 两处分叉已与主人确认：
1. 底部按钮布局：**一行三按钮** —「上一页」靠左 ·「关闭」居中 ·「下一页」靠右，左右两键按需出现。
2. 标题页码：**仅当 `totalPages>1` 时**在标题尾部追加「（第 m/n 页）」（m=`page+1`）。

## 现状锚点（已勘察）

- `OpenPerkView()`（`GameScene.cpp:798`）现负责：三向守卫（`mOpenMenu` / `mSurvivalPerkSelectActive`
  / `mPerkViewActive`）→ `mPerkViewActive=true` + `DeltaTime::SetPaused(true)` → 扫
  `0..PerkType::COUNT` 收集 `stacks>0` 的 distinct 行（`perkLines`，绿=PLANT_BUFF / 红=其余）、
  `distinct`、`total` → 组装标题 → 固定面板 `boxW=560,boxH=420,padX=30,padY=26` →
  字号 18→10 自适应（`availH = boxH - 2*padY - closeGap - closeBtnSize.y`，`innerW = boxW - 2*padX`）
  → 内容块在 `availH` 内垂直居中 → 一个底部居中「关闭」按钮（IMAGE_BUTTONBIG, `autoClose=true`,
  回调 `ClosePerkView()`）→ `mPerkViewBox = CreateMessageBox(..., "", Vector(boxW,boxH))`。
- `ClosePerkView()`（`GameScene.cpp:905`）：`mPerkViewActive=false` + 解除暂停。面板由「关闭」按钮
  `autoClose=true` 帧末自销。
- **重建安全性（关键）**：`GameMessageBox::Close()` → `DestroyGameObject(this)` 是**帧末延后销毁**
  （`GameMessageBox.cpp` 注释「销毁是延迟到帧末，回调期间 this 一定有效」证实）。按钮回调里
  先建新框、再让旧框 `autoClose` 自销；旧框析构（`~GameMessageBox`）只 `RemoveButton/RemoveSlider`
  自己 `m_buttons/m_sliders` 里的对象，与新框各自独立，互不影响。→ 翻页可安全「原地重建」。
- `PerkType::COUNT` 共 6 个词条 → 全选可凑 6 distinct → `ceil(6/5)=2` 页（5+1），足以覆盖分页路径。

## 组件设计

### 1. 头文件（GameScene.h）

私有区新增：

```cpp
void RenderPerkViewPage();                 // 按 mPerkViewPage 重建词条查看面板（OpenPerkView 主体迁入）
int  mPerkViewPage = 0;                     // 词条查看面板当前页（0-based）
```

`OpenPerkView` / `ClosePerkView` 声明不变。

### 2. OpenPerkView()（瘦身为入口）

保留三向守卫；保留 `mPerkViewActive=true` + `DeltaTime::SetPaused(true)`；**新增** `mPerkViewPage = 0;`；
然后 `RenderPerkViewPage();`。其余建框逻辑全部迁入 `RenderPerkViewPage`。

```cpp
void GameScene::OpenPerkView()
{
    if (!mBoard) return;
    if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive) return;
    mPerkViewActive = true;
    DeltaTime::SetPaused(true);
    mPerkViewPage = 0;
    RenderPerkViewPage();
}
```

### 3. RenderPerkViewPage()（建框主体 + 分页）

迁入现 `OpenPerkView` 的 `measureW`、配色、`perkLines` 收集（含 `distinct`/`total`）、固定面板常量。
**不**碰 `mPerkViewActive`/暂停（入口已设；翻页时维持）。新增/改动：

1. 分页量：
   ```cpp
   constexpr int kPerksPerPage = 5;
   const int totalPages = (distinct > 0) ? ((distinct + kPerksPerPage - 1) / kPerksPerPage) : 1;
   if (mPerkViewPage < 0) mPerkViewPage = 0;
   if (mPerkViewPage > totalPages - 1) mPerkViewPage = totalPages - 1;
   const int pageStart = mPerkViewPage * kPerksPerPage;
   const int pageEnd   = std::min(distinct, pageStart + kPerksPerPage);  // distinct == perkLines.size()
   ```
2. 标题加页码（仅多页时）：
   ```cpp
   std::string title = (distinct > 0)
       ? (std::string(u8"已强化：") + std::to_string(distinct) + u8" 种词条 · 累计 " + std::to_string(total) + u8" 层")
       : std::string(u8"尚未选择任何强化词条");
   if (totalPages > 1)
       title += std::string(u8"（第 ") + std::to_string(mPerkViewPage + 1) + u8"/" + std::to_string(totalPages) + u8" 页）";
   ```
3. 字号自适应 + 布局：原循环与布局逻辑**只对本页切片** `perkLines[pageStart..pageEnd)` 计算与绘制
   （把原先遍历 `0..N`、`N=perkLines.size()` 改成遍历切片，`N` 改为本页行数 `pageEnd-pageStart`）。
   切片 ≤5 行 → 自适应几乎恒取 18，可读性提升。`availH` / `innerW` / 垂直居中算法不变。
4. 底部一行三按钮（`btnY = boxTop + boxH - padY - closeBtnSize.y`，与原「关闭」同 y）：
   - `navBtnSize(110,44)`，`closeBtnSize(160,44)`（沿用）。
   - 「关闭」：居中 `cx - closeBtnSize.x/2`，IMAGE_BUTTONBIG，回调 `ClosePerkView()`，`autoClose=true`（不变）。
   - 「上一页」：仅 `mPerkViewPage > 0`。x = `boxLeft + padX`（=300），IMAGE_BUTTONSMALL，
     回调 `[this]{ --mPerkViewPage; RenderPerkViewPage(); }`，`autoClose=true`。
   - 「下一页」：仅 `mPerkViewPage < totalPages - 1`。x = `boxRight - padX - navBtnSize.x`（=690），
     IMAGE_BUTTONSMALL，回调 `[this]{ ++mPerkViewPage; RenderPerkViewPage(); }`，`autoClose=true`。
   - 三键 x 区间 300–410 / 470–630 / 690–800，互不重叠（`cx=550, boxLeft=270, boxRight=830`）。
5. `mPerkViewBox = mUIManager.CreateMessageBox(Vector(cx,cy), "", buttons, sliders, texts, "", 1.0f, "", Vector(boxW,boxH));`

### 4. ClosePerkView()

不变。翻页不经此路径。

## 行为不变量（回归校验点）

- **0 词条**：`distinct=0 → totalPages=1`，标题"尚未选择任何强化词条"（无页码），无翻页键，仅「关闭」。
  与扩展前逐像素一致。
- **1–5 词条**：`totalPages=1`，无翻页键，无页码后缀，仅「关闭」。布局同扩展前（区别仅切片=全集，等价）。
- **暂停 / 守卫**：`mPerkViewActive`、`DeltaTime::SetPaused`、`OpenMenu`/`BeginSurvivalPerkSelect` 三向守卫
  均不动。翻页全程 `mPerkViewActive` 维持 true、保持暂停。
- **面板尺寸**：560×420 固定不变。

## 验证（AutoTest）

扩展 `smoke_perk_view`（或新增 `smoke_perk_view_paged`）：
1. 进生存关卡，令 PerkManager 累积 ≥6 个 distinct 词条（凑出 2 页；具体喂入手段在 plan 阶段定，
   优先复用现有「轮间选词条」AutoTest 路径或直接多次 `AddPerk`）。
2. click「词条」按钮开面板 → `screenshot` 第 1 页（应见 5 行 + 「下一页」，**无**「上一页」，标题含「第 1/2 页」）。
3. click「下一页」→ `screenshot` 第 2 页（应见剩余行 + 「上一页」，**无**「下一页」，标题含「第 2/2 页」）。
4. click「上一页」→ `screenshot` 回第 1 页（与步骤 2 一致）。
5. click「关闭」→ 面板消失、游戏恢复（`mPerkViewActive=false`）。
6. 逐张 Read 截图核验按钮出现条件与页码。

构建：`clang-release`（0 warning/0 error 验证）。

## 不做（YAGNI）

- 不做滚动条 / 不做每页可配数量（固定 5）。
- 不持久化 `mPerkViewPage`（关面板即丢；下次开恒从第 1 页）。
- 不动字号自适应算法本身（仅改其作用域为本页切片）。
- 不改面板尺寸、不加翻页动画。
