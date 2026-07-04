# GameMessageBox Builder 化简设计（方案 A）

日期：2026-07-04
状态：已获主人批准

## 背景与问题

`GameMessageBox` 现状三大痛点：

1. **9 参构造函数**（pos, message, buttons, sliders, texts, title, backgroundImageKey, scale, explicitSize），且 `UIManager::CreateMessageBox` 包装层把 `scale`/`backgroundImageKey` 顺序调换、背景默认值不一致（空串 vs `IMAGE_MESSAGEBOX`）。每个调用点都要手写 3 个 config vector（sliders 几乎恒空）。
2. **隐式规则藏在实现里**：
   - "空 backgroundImageKey + 非零 explicitSize" ⇒ 画纯色面板；
   - "texture == IMAGE_OPTIONS_CHECKBOX0/1" ⇒ 按 checkbox 处理；
   - 非 explicitSize 模式背景有硬编码 `(-230,-180)` 偏移。
3. 调用点共 7 处：GameScene（暂停菜单 / 重开确认 / 退出确认 / 生存词条选择 / 词条查看面板 / 开发者面板）+ MainMenuScene（选项菜单）。

## 目标

- 调用点不再手写 config vector、不再数 9 参顺序。
- 把上述隐式规则显式化为命名方法（`.Panel()` / `.Checkbox()`）。
- **`GameMessageBox` 本体零改动**（Draw/Start/析构/9 参构造全部保留）——风险收敛为"新增 Builder + 机械迁移调用点"。

## 设计

### Builder 类

在 `UI/GameMessageBox.h` 内新增嵌套类 `GameMessageBox::Builder`（实现放 `GameMessageBox.cpp`，不新开文件）。内部字段即攒下的构造参数；`Show()` 时组装并创建对象。

### API

```cpp
GameMessageBox::Builder(Vector pos)              // 唯一必填：中心位置

// 背景三选一；一个都不调 = 默认 IMAGE_MESSAGEBOX 纹理（现 UIManager 默认行为）
.Panel(float w, float h)                         // 纯色面板：内部翻译为 空key + explicitSize(w,h)
.Background(const std::string& key)              // 指定纹理，原始尺寸 × scale
.Background(const std::string& key, Vector size) // 指定纹理 + 显式尺寸，以 pos 居中

// 内容，均可选、可多次调用
.Title(const std::string&)
.Message(const std::string&)
.Text(Vector pos, float fontSize, const std::string& text, glm::vec4 color)
.Text(Vector pos, float fontSize, const std::string& text, glm::vec4 color, const std::string& font)
.Button(const std::string& text, Vector pos, Vector size, float fontSize,
        std::function<void()> cb,
        const std::string& texture = IMAGE_BUTTONSMALL, bool autoClose = true)
.Checkbox(Vector pos, Vector size, std::function<void()> cb)   // 内部填 CHECKBOX0 纹理
.Slider(Vector pos, Vector size, float min, float max, float init,
        std::function<void(float)> cb, bool integerOnly = false)
.Scale(float)                                    // 默认 1.0

.Show()  // 创建对象（走 GameObjectManager，LAYER_UI），返回 shared_ptr<GameMessageBox>
```

约定：

- Builder 各方法返回 `Builder&`，支持链式；`Show()` 为终结方法。
- `.Panel()` 与 `.Background()` 后调覆盖先调（最后一次生效），不做互斥报错——与现构造语义一致。
- `.Checkbox` 的 size 传 `Vector::zero()` 时沿用现有"按 NormalButton 处理"路径。

### UIManager 收窄

7 个调用点迁完后，`UIManager::CreateMessageBox` 的公开 9 参形态删除，逻辑内联进 `Builder::Show()`（或保留为私有辅助）。两层 API 参数顺序不一致的问题随之消失。

## 迁移范围

| 调用点 | 文件 |
|---|---|
| 暂停菜单 OpenMenu | Game/GameScene.cpp |
| 重开确认 OpenRestartMenu | Game/GameScene.cpp |
| 退出确认 OpenQuitMenu | Game/GameScene.cpp |
| 生存词条选择 | Game/GameScene.cpp |
| 词条查看面板 | Game/GameScene.cpp |
| 开发者面板 | Game/GameScene.cpp |
| 选项菜单 | Game/MainMenuScene.cpp |

## 非目标（留待第二刀）

- title/message 统一进 TextConfig、删 `(-230,-180)` 魔法偏移、checkbox 拆独立配置——即原方案 C 的内部重构部分。
- 行为/像素级外观改动：本次迁移应逐像素等价。

## 验证

1. `cmake --build --preset clang-release` 零警告。
2. 现有 AutoTest 冒烟脚本回归（暂停菜单、词条选择/查看、开发者面板均有覆盖脚本）。
3. 关键界面截图与迁移前对比，应一致（纯调用方式改写）。
