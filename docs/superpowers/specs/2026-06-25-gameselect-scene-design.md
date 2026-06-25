# GameSelectScene 设计文档

日期：2026-06-25
状态：已与主人确认布局方案，待评审

## 1. 目标与范围

参照主人提供的参考图（经典 PvZ「挑战模式」选择界面，`参考图片.png`），新增一个继承自 `Scene` 的
`GameSelectScene`，**纯视觉布局复刻**：

- 复刻顶部标题文字、9 张关卡卡片网格（6+3 排布）、左下角「返回菜单」按钮、羊皮纸背景面板。
- 卡片为**占位**：可悬停高亮、可点击（点击仅打日志，不跳转）。具体每张卡进哪个模式/关卡**不在本次范围**。
- **入口暂不接**：场景做好并注册到 `SceneManager` 即可，主菜单从哪进以后再定。

明确**不做**的事（YAGNI）：
- 卡片不绑定任何真实关卡/模式跳转。
- 卡片上半部图标区**留空**（不引入奖杯/僵尸头等图标素材）。
- 不改主菜单按钮、不加新入口按钮。

## 2. 已确认的关键参数

- 逻辑分辨率：`SCENE_WIDTH = 1100`，`SCENE_HEIGHT = 600`（`GameApp.h`）。
- 顶部标题文字：**「选择关卡」**（主人指定，替换参考图的「挑战模式」）。
- 卡片占位标签：**「关卡①」~「关卡⑨」**。

## 3. 素材（已存在于项目 `resources/image/`，键由文件名自动派生）

| 用途 | 文件 | 尺寸 | 资源键 |
|---|---|---|---|
| 羊皮纸背景 + 顶部撕纸横幅（横幅已画进背景图） | `UI/Challenge_Background.png` | 1280×720 | `IMAGE_CHALLENGE_BACKGROUND` |
| 关卡卡片框（常态） | `UI/Challenge_Window.png` | 118×120 | `IMAGE_CHALLENGE_WINDOW` |
| 关卡卡片框（悬停高亮） | `UI/Challenge_Window_Highlight.png` | 118×120 | `IMAGE_CHALLENGE_WINDOW_HIGHLIGHT` |
| 返回菜单按钮（复用年鉴同款） | `UI/Almanac/Almanac_IndexButton.png` / `..._Highlight.png` | 164×26 | `IMAGE_ALMANAC_INDEXBUTTON` / `IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT` |

`ResourceManager::GenerateStandardKey` 在加载时从文件名派生键，故上述图已加载可用；`ResourceKeys.h`
里仅缺 `IMAGE_CHALLENGE_*` 的防手滑镜像常量，本次补上（符合仓库约定）。

## 4. 架构

仿照 `AlmanacScene` 的最小结构。`Scene` 基类已封装 `AddTexture` / `RegisterDrawCommand` /
`mUIManager.CreateButton`，新场景只需重写 4 个虚函数。

### 新增 `Game/GameSelectScene.h`
```cpp
#pragma once
#ifndef _GAME_SELECT_SCENE_H
#define _GAME_SELECT_SCENE_H
#include "Scene.h"

class GameSelectScene : public Scene {
private:
    std::shared_ptr<Button> mBackMenuButton;
    std::vector<std::shared_ptr<Button>> mCards;   // 9 张占位卡片
    bool mReadyToSwitchMainMenu = false;
public:
    void OnEnter() override;
    void OnExit() override;
    void Update() override;
protected:
    void BuildDrawCommands() override;
};
#endif
```

### `Game/GameSelectScene.cpp`

- **`BuildDrawCommands()`**
  1. `Scene::BuildDrawCommands()`。
  2. `AddTexture(IMAGE_CHALLENGE_BACKGROUND, 0, 0, 1100.f/1280.f, 600.f/720.f, -1000)` 铺满背景。
  3. 9 张卡片按钮（数据驱动小循环，6+3 排布）：
     - 卡片缩放 `kCardScale ≈ 0.95`（≈112×114）。
     - 列间距 `kPitchX ≈ 150`；第 1 行 6 列起点 `x0 ≈ 115`，`y1 ≈ 150`；第 2 行 3 列 `y2 ≈ 320`，
       左对齐前 3 列。
     - 每张 `SetAsCheckbox(false)`、`SetImageKeys(IMAGE_CHALLENGE_WINDOW,
       IMAGE_CHALLENGE_WINDOW_HIGHLIGHT, IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
       IMAGE_CHALLENGE_WINDOW_HIGHLIGHT)` 得到悬停高亮。
     - 点击回调 **no-op**：`[i](bool){ LOG_INFO("UI") << "card " << i << " clicked"; }`（纯脚手架）。
  4. `RegisterDrawCommand("DrawSelectTexts", …, LAYER_UI + 100)` 一次性画：
     - 顶部标题「选择关卡」，居中于横幅之上（`gameApp.DrawText`）。
     - 9 个占位标签「关卡①…⑨」，画在各卡下方小条中央。
     - （文字坐标为起始值，AutoTest 截图后微调。）
  5. `SortDrawCommands()`。
- **`Update()`**：`Scene::Update()`；若 `mReadyToSwitchMainMenu` → 复位标志 → `SceneManager::SwitchTo("MainMenuScene")`。
- **`OnEnter()`**：`Scene::OnEnter()` + `AudioSystem::PlayMusic(MUSIC_CHOOSEYOURSEEDS, -1)`。
- **`OnExit()`**：`mBackMenuButton.reset()`；`mCards.clear()`；`Scene::OnExit()`。

返回菜单按钮完全照搬 `AlmanacScene`：`CreateButton(Vector(7,560), Vector(162,26))`、
`SetImageKeys(IMAGE_ALMANAC_INDEXBUTTON, IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT, …)`、
`SetText(u8"返回菜单", FONT_FZJZ, 18)`、`SetTextColor({52,51,93,255})`、点击置
`mReadyToSwitchMainMenu = true`。

### 接入

- `ResourceKeys.h`：在 Textures 段补
  `RKEY(IMAGE_CHALLENGE_BACKGROUND)` / `RKEY(IMAGE_CHALLENGE_WINDOW)` / `RKEY(IMAGE_CHALLENGE_WINDOW_HIGHLIGHT)`。
- `GameApp.cpp`：`#include "./Game/GameSelectScene.h"` + 在场景注册段加
  `sceneManager.RegisterScene<GameSelectScene>("GameSelectScene")`。**不**改 `SwitchTo` 默认场景，**不**接主菜单入口。
- CMake：`GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集，新增 `.cpp` 无需改构建文件。

## 5. 数据流与边界

- 场景自包含：只依赖 `Scene` 基类、`mUIManager`、`SceneManager`、`AudioSystem`、`GameAPP::DrawText`，
  与游戏逻辑（Board/GameObject）**零耦合**——无植物/僵尸/存档参与。
- 切场景统一走标志位 + `Update()` 里 `SwitchTo`，不在回调内销毁当前场景。
- 卡片点击回调不持有跨帧状态，纯日志，无副作用。

## 6. 错误处理

- 资源键缺失：`ResourceManager` 取不到纹理时返回占位/空，截图能立刻暴露（验证环节兜底）。
- 9 张卡片用循环生成，避免重复代码导致的坐标手滑。

## 7. 验证

因入口暂不接，AutoTest 无现成路径可达本场景。验证采用**临时重定向启动场景**：

1. 临时把 `GameApp.cpp` 启动处 `SwitchTo("MainMenuScene")` 改为 `SwitchTo("GameSelectScene")`。
2. `cmake --build --preset clang-release` 构建。
3. 跑最小脚本 `autotest/scripts/gameselect_smoke.json`（`wait_frames` → `screenshot` → `quit`），
   工作目录 `build/clang-release/`。
4. 用 Read 工具看截图，核对：背景铺满、标题「选择关卡」在横幅上、6+3 卡片网格、卡片悬停高亮可用、
   左下「返回菜单」按钮。按需微调坐标常量，重复 2–3。
5. **还原**启动场景重定向（`SwitchTo("MainMenuScene")`），保证不进 commit。

`gameselect_smoke.json` 可保留入库（纯数据，未来接入口后仍有用）。

## 8. 验收标准

- clang-release 构建 0 warning / 0 error。
- 截图布局与参考图整体一致：顶部「选择关卡」标题、6+3 卡片网格、左下「返回菜单」按钮、羊皮纸背景。
- 卡片悬停切换到高亮框；点击打印日志、不崩。
- 「返回菜单」点击切回 `MainMenuScene`。
- 启动场景重定向已还原；除三常量 + 注册行外不动既有场景。
