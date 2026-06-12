# AutoTest 脚本自动驾驶套件 — 设计文档

**日期**: 2026-06-12
**状态**: 已获主人认可
**目标**: 让 Claude 实现「改代码 → msbuild 编译 → 拉起游戏跑测试脚本 → 收截图/状态 JSON → Read 验证」全自动闭环，主人不再参与编译和游戏内截图。

## 背景与可行性结论

调查确认项目对自动化高度友好：

- 进关卡已是程序化路径：`SetGlobalData("EnterLevel", N)` + `SwitchTo("GameScene")`（MainMenuScene.cpp:36 即此用法）。
- 种植/出怪有真实玩法接口：`Board::CreatePlant(type, row, col)`（Board.h:171）、`Board::CreateZombie(type, row, x)`（Board.h:159）、`Board::SetSun`。存档恢复路径（`CreatePlantWithID`/`CreateZombieWithID`）证明"绕过 UI 直接构造游戏状态"是引擎一等公民。
- 选卡可程序化：`ChooseCardUI::ToggleCardSelection(Card*)`（ChooseCardUI.h:35）+ `GameScene::ChooseCardComplete()`，只差一个"按 PlantType 找 Card"的辅助方法。
- 截图差一小步：交换链镜像目前仅 `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`（VulkanContext.cpp:315），加 `TRANSFER_SRC_BIT` 后可在 `VulkanRenderer::EndFrame` present 之前回读。
- exe 自包含：`x64\{Debug,Release}\` 内 exe + resources + saves 齐全，可直接命令行拉起。
- Claude 的 Read 工具能直接显示 PNG 图片 —— 截图落盘即完成视觉验证闭环。

主人已同时授权：解除构建限制，Claude 可直接 `msbuild PlantsVsZombies.sln /p:Configuration=Release /p:Platform=x64`（全量约 30s），不必核对产物时间戳。CLAUDE.md 须同步更新。

## 方案选型

三个候选中选定 **A. 引擎内脚本自动驾驶**（主人拍板）：

- **A. 脚本自动驾驶（选定）**：`-AutoTest script.json` 启动参数，脚本声明命令序列，游戏自动执行后退出。确定性强、换场景只改 JSON 不重编译。
- B. 交互式 IPC 命令通道：更灵活但进程管理复杂、时序不可复现；将来可在同一命令分发核心上叠加。
- C. 零代码改动（调试器 MCP + PrintWindow 抓窗）：免编译但慢、脆，Vulkan 窗口 GDI 抓图可能黑屏。

## 架构

### 1. TestDriver（新增 `Game/AutoTest/TestDriver.h/.cpp`，单例）

- `main.cpp` 解析 `-AutoTest <脚本路径>`，可选 `-Seed N` 固定 `GameRandom` 种子保证可复现。
- 挂钩点：`GameAPP::Run` 主循环 `sceneManager.Update()` 之后调 `TestDriver::Update()`。未激活时一个 bool 判断直接返回，零开销，不影响正常游戏。
- 顺序执行 JSON 命令队列；wait 类命令控制推进；每条命令默认超时 10 秒（可被脚本字段覆盖），超时即失败退出。

### 2. 命令集 v1

JSON 结构：`{ "commands": [ { "op": "...", ...参数 }, ... ] }`。

| op | 参数 | 实现 |
|---|---|---|
| `goto_level` | `level` | `SetGlobalData("EnterLevel")` + `SwitchTo("GameScene")` |
| `choose_cards` | `cards`（PlantType 名数组） | 内部轮询等待选卡界面就绪（IntroStage 到位、ChooseCardUI 存在），逐张 `FindCardByType` + `ToggleCardSelection`，再 `ChooseCardComplete()`；脚本无须关心开场动画时长 |
| `set_sun` | `value` | Board 现有 `AddSun/LoseSun/GetSun`，新增 `SetSun(int)` 直接设值 |
| `plant` | `type`, `row`, `col` | `Board::CreatePlant`（真实玩法路径，含 Cell 占用） |
| `spawn_zombie` | `type`, `row`, `x` | `Board::CreateZombie` |
| `wait_seconds` | `value` | 按 DeltaTime 累计游戏时间（非墙钟） |
| `wait_frames` | `value` | 帧计数 |
| `wait_state` | `state`（BoardState 名） | 轮询 `Board::mBoardState` 直到匹配，受超时约束 |
| `screenshot` | `name` | 见截图管线；落盘 `./autotest/out/<脚本名>/<name>` |
| `dump_state` | `name` | 遍历 EntityManager：僵尸+植物的 type、row、x、y、HP（含头盔/护盾）、当前动画 track 名与速度 → JSON 落盘 |
| `quit` | — | `GameAPP::SetRunning(false)` |

类型名→枚举映射：PlantType/ZombieType 用字符串名解析（建一张名字表；新类型加表即可）。

退出码约定：0 = 脚本全部执行成功；非 0 = 失败（解析错误、命令失败、超时），便于 Claude 在命令行直接判断。失败时已产出的截图/JSON 保留供事后分析。

### 3. 截图管线（唯一 Vulkan 改动，两处）

1. `VulkanContext.cpp:315`：`sci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT`（普遍支持，零成本）。
2. `VulkanRenderer` 增加 `RequestCapture(path)`：`EndFrame` 中在 barrier→PRESENT_SRC / present **之前**，将当前 swapchain image 拷贝到 host-visible buffer（barrier 到 TRANSFER_SRC_OPTIMAL → vkCmdCopyImageToBuffer → barrier 回 PRESENT_SRC 流程内），submit 后等 fence，BGRA→RGBA swizzle，用已有依赖 SDL2_image 的 `IMG_SavePNG` 写盘。

约束：present 之后镜像归显示引擎所有，不可回读——捕获点必须在 present 前。截图帧允许一次 `vkDeviceWaitIdle` 级别的停顿（测试场景不在乎单帧卡顿）。

预乘 alpha 管线无影响：swapchain 是最终合成图像（不透明），回读即所见。

### 4. 输出与目录约定

- 脚本：`./autotest/scripts/*.json`（纯数据，不进 vcxproj）。
- 产物：`./autotest/out/<脚本名>/`（PNG、state JSON），目录按需创建。
- 日志：沿用 Logger 控制台输出；TestDriver 用 `LOG_INFO("AutoTest")` 标记每条命令的开始/结果，Claude 从 stdout 读执行轨迹。

### 5. 最小侵入钩子（既有类仅加只读/转发方法）

- `GameScene::GetBoard()` → `Board*`
- `ChooseCardUI::FindCardByType(PlantType)` → `Card*`
- 其余逻辑全部在新文件 `Game/AutoTest/` 内。

### 6. CLAUDE.md 同步更新

- 删除"不要用 MSVC-Debug-MCP 构建"禁令段与 build_status 时间戳注意事项。
- 新增：msbuild 命令行构建方法；AutoTest 套件用法（启动参数、脚本格式、输出位置）。

## 错误处理

- 脚本解析失败：启动即报错退出，退出码非 0。
- 命令执行失败（找不到卡牌类型/格子被占/Board 不存在）：`LOG_ERROR("AutoTest")` 写明哪条命令第几号失败，退出码非 0。
- 任何 wait/轮询超过超时：同上。
- 截图失败（usage 不支持/IO 错误）：记错误但不中断后续命令（视觉产物缺失由 Claude 读输出目录时发现）。

## 测试与验收

套件自验 = 一个 demo 脚本 `autotest/scripts/demo_peashooter.json`：
1. 进关卡 1 → 选豌豆射手+向日葵 → 设阳光 2000 → 种豌豆射手(2,1) → 放普通僵尸(2, x=900)；
2. 三个时间点截图（种植后、子弹飞行、僵尸受击）+ `dump_state`；
3. 验收标准：退出码 0；PNG 非全黑且尺寸正确（Claude 直接 Read 目检）；state.json 含 1 植物 + 1 僵尸条目且字段齐全。

## 不做（YAGNI）

- 交互式 IPC 通道（将来可叠加在命令分发核心上）。
- 图像 diff/像素断言 —— Claude 直接看图比脆弱的像素比对更可靠。
- Headless 渲染 —— 窗口正常弹出即可，测试机就是开发机。
- 鼠标/键盘事件模拟 —— 一律走编程接口。
