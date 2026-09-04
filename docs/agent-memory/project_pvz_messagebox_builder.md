---
name: project_pvz_messagebox_builder
description: 2026-07-04 GameMessageBox 流式 Builder 化简 ✅合master未push——9参构造/隐式规则/friend槽位戳全消灭
metadata:
  node_type: memory
  type: project
  originSessionId: d3765641-51c5-4361-94ca-bc781aa1f8c1
---

2026-07-04 GameMessageBox Builder 化简完成（623f078..cb5fec6，master 未 push）。`GameMessageBox::Builder`（嵌套类，头文件内联+Show() 在 .cpp）替代 9 参构造：`.Panel(w,h)`=纯色面板（原"空key+非零explicitSize"暗号）、`.Checkbox(pos,size,cb,initChecked)`=复选框（原 CHECKBOX0 纹理嗅探+friend 按槽位戳 SetChecked）、`.Button` 尾参默认 IMAGE_BUTTONSMALL/autoClose=true。全部 7 个调用点迁完；`UIManager::CreateMessageBox` 与两个 friend 声明已删。

**foot-gun**：
- 2026-09-04 起 Builder 默认模式是 `STANDARD_DIALOG`，不再用空 key 或 `IMAGE_MESSAGEBOX` 暗示背景类型；`.Background()` 和 `.Panel()` 必须继续显式切换模式，否则会误走默认自适应布局。
- `smoke_develop.json` 曾整体失效（exit 仍 0！）：面板热键 D→RSHIFT 后脚本没跟上，且 dump_state 不断言，须看 dump 内容才能发现；已修（kKeyNames 补 rshift + 脚本改按 rshift）。**跑 smoke_develop 必须带 `-develop` 启动参数**，否则面板不开、脚本照样绿。
- 循环内动态加按钮/文字的站点（词条选择/查看、开发者面板）不能纯链式：`GameMessageBox::Builder builder{pos};` 局部变量 + 循环里 builder.Button/Text（用花括号初始化避 most vexing parse）。

新增验证脚本：`pause_menu_shot.json`（ESC 暂停菜单截图）、`mainmenu_options_shot.json`（点 (740,501) 开主菜单选项）。第二刀候选（未做）：title/message 统一进 TextConfig、删 Draw 里 (-230,-180) 魔法偏移。spec/plan 在 docs/superpowers/{specs,plans}/2026-07-04-messagebox-builder*。

## 2026-08-22 所有权后记

Builder 调用接口不变，但 `Show()` 现在把普通 `GameMessageBox` 注册给当前场景 `UIManager`，不再创建 UI 类型 GameObject。`Close()` 立即失活并只记录关闭请求；UIManager 在 Button/Slider 更新结束后解除旧框控件并释放自身所有权，`ClearAll()` 也会先断开控件和 owner，因此外部共享引用不会误操作后续场景。回调内重建新框的安全性来自 UIManager 的遍历后清理阶段，不再依赖 GOM 帧末销毁。

绘制契约同步固定为“普通 Button/Slider → 活动 GameMessageBox”；弹框自有控件保持
`skipDraw`，由 `GameMessageBox::Draw()` 在背景和文字之后自行提交。这样模态框稳定盖住
其他 UI，调用方只需禁用背景控件的输入，不应再把背景控件从绘制路径删掉。

## 2026-08-28 浮动 Tooltip 与独立命中区

`Button::SetHitBounds` 允许命中矩形独立于绘制矩形；`ContainsPoint` 与命中仲裁中心读取该区域，
而复选框贴图仍保持原尺寸。`GameMessageBox::Builder::Checkbox` 可附带说明文字和整行命中尺寸，
`.TooltipPanel(maxSize,fontSize)` 配置浮动说明框。活动控件悬停时按文字实际宽度绘制，位置跟随逻辑
鼠标，靠近场景边缘自动换侧并夹紧；移出命中区立即不再绘制。未配置说明的既有 MessageBox 行为不变。

控制台专项通过 `consoleTooltipVisible/Text/X/Y` 锁定实文、移动和消失；通用回归仍需覆盖
`mainmenu_options_shot` 与 `pause_menu_shot`，防止 Builder 默认路径受新可选配置影响。

## 2026-09-04 默认 Box 原版分件自适应

只有 Builder 未调用 `.Background()` / `.Panel()` 的默认 Box 走 `STANDARD_DIALOG`：使用
`dialog_topleft/topmiddle/topright`、`centerleft/centermiddle/centerright`、
`bottomleft/bottommiddle/bottomright` 和 `dialog_header` 十张原版素材。上下段横向平铺，
中段横纵平铺并裁切末块，不拉伸边缘。宽高按标题、UTF-8 换行正文和按钮行测量，
所有标准按钮在控件注册前改为框内相对排布，整个矩形以 Builder 传入的中心点对称绘制。
PC 素材的 `dialog_centerright` 比上下右角少 3 列透明阴影，绘制时要与 `topright`
共用左侧内沿；按各自宽度贴右会让暗槽在上下接缝处向右凸出。

`.Background()` 仍保留主菜单选项/暂停菜单的旧纹理、旧坐标；`.Panel()` 仍保留词条和开发者面板的纯色尺寸。
`adaptive_messagebox` 锁定长文扩宽/换行/增高和十分件资源；`adaptive_messagebox_confirm`
锁定实际重开确认框的 480×386 短文基准。`mainmenu_options_shot`、`pause_menu_shot` 继续是两条旧背景回归。
