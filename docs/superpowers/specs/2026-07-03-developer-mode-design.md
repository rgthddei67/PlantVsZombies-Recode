# 开发者模式（-develop）设计文档

日期：2026-07-03
状态：已获主人批准

## 目标

为调试与内容开发提供一个游戏内开发者面板，命令行 `-develop` 开启，按 **D** 呼出。功能：

1. 无冷却种植（可开关，默认关）
2. 无视阳光种植（可开关，默认关）
3. 面板内直跳任意关卡（含 1000 生存无尽 / 1001 黑夜无尽，不受存档解锁限制）
4. 选择僵尸类型后点草坪，在点击处 (row, x) 召唤该僵尸

非目标：不做数值编辑器、不做 ImGui、不影响正常玩家路径（无 `-develop` 时零行为变化）。

## 总体结构

### 开关与状态（GameAPP 静态成员）

- `main.cpp` 参数循环加 `-develop` / `-Develop` → `GameAPP::mDevelopMode = true`。
- 作弊开关：`GameAPP::mDevNoCooldown`、`GameAPP::mDevFreePlant`（bool，默认 false）。
  放 GameAPP 静态是刻意选择：功能量小，不值得引 DevManager 单例；生效点（卡片冷却/阳光判定）本来就能直接看到 GameAPP。

### 面板 UI（GameScene 成员函数，照 BeginSurvivalPerkSelect 模式）

- 呼出：`GameScene::Update` 中 `InputHandler::IsKeyPressed(SDLK_d)`，仅当 `GameAPP::mDevelopMode` 为真。
- 守卫（与词条面板同风格的多向守卫）：暂停菜单（mOpenMenu）、轮间选词条（mSurvivalPerkSelectActive）、词条查看面板（mPerkViewActive）、自身已开（mDevPanelActive）任一为真时不响应 D。
- 实现：`mUIManager.CreateMessageBox(...)` 纯色面板（背景留空 + explicitSize），开面板时 `DeltaTime::SetPaused(true)`，关时恢复。
- 面板内容（按钮均复用 IMAGE_BUTTONSMALL/IMAGE_BUTTONBIG）：
  - 【无冷却种植：开/关】切换按钮，文字实时反映状态
  - 【无视阳光：开/关】切换按钮
  - 僵尸行：【◀】【<僵尸中文名>】【▶】循环切换 ZombieType + 【召唤模式】按钮
  - 关卡行：【-】【关卡 N】【+】 + 【进入】按钮
  - 【下一波】按钮：立即触发下一波僵尸（把 Board 的出波倒计时清零/调用既有出波入口，具体以 Board 现有波次推进机制为准），点击后关闭面板恢复时间流动，方便观察
  - 【关闭】按钮
- UI 具体布局（间距、按钮尺寸、行序）由实现时自行裁量，照词条面板的自动量宽排版风格即可，主人只验收功能。
- 切换按钮更新文字的实现方式：点击回调里改状态后关闭并立即重建面板（最简；面板本来就是模态，重建无闪烁问题）。

### 召唤放置模式

- 点【召唤模式】→ 关面板、恢复时间流动、置 `mDevSpawnMode = true` 与当前选中的 `mDevSpawnZombieType`。
- 放置模式下每次左键点草坪：由点击 y 反算最近行（用 `GetZombieSpawnY(row)` 的逆：遍历行取距离最小者，并夹在合法行范围内），调 `Board::CreateZombie(type, row, clickX)`。y 恒由 row 派生，符合"网格绑定生成"既有契约。
- 退出放置模式：按 ESC 或再按 D（再按 D 退出放置模式并重开面板）。
- 放置模式下需吞掉该次点击，不让它触达种植/铲子等既有点击逻辑（在 GameScene 点击分发前判 mDevSpawnMode 优先消费）。
- 放置模式给一个轻量视觉提示：屏幕顶部 DrawTextOnTop 一行"召唤模式：<僵尸名>（ESC 退出）"。

### 直跳关卡

- 关卡号范围：普通关 + 1000/1001。【进入】走与 AutoTest `goto_level` 相同的切关路径，直接进入该关，不检查存档解锁进度。

### 作弊生效点（收费点守卫，不散落 UI）

- 无冷却：卡片冷却判定处视为恒就绪；种植后不进入冷却（或立即重置）。守卫写成 `GameAPP::mDevelopMode && GameAPP::mDevNoCooldown`。
- 无视阳光：阳光"是否足够"判定恒过 + 扣阳光处不扣。同上双条件守卫。
- 双条件的意义：即使存档/其他路径意外置了作弊 bool，无 `-develop` 时也绝不生效。作弊开关不进存档。

## 错误处理与边界

- `-AutoTest` 与 `-develop` 可共存（AutoTest 用 key/click 驱动面板即真实输入路径）。
- 非 GameScene 场景按 D 无效果。
- CreateZombie 的 x 直接用点击 x（逻辑坐标），不做额外夹取——Board 既有代码对任意 x 已容忍。
- ZombieType 循环切换表用 TestDriver.cpp 既有的类型名表同源数据（避免两处维护）；中文名可先直接显示枚举标识符，简陋可接受。

## 测试

- 新增 `autotest/scripts/smoke_develop.json`：`-develop -AutoTest` 启动 → goto_level → key "d" 开面板截图 → click 切换两个作弊开关 → click 进召唤模式 → click 草坪某点 → dump_state 验证：目标僵尸出现且 row/x 匹配；开无视阳光后种植阳光数不变；开无冷却后连续种植成功。
- 手动验证：主人 F5 加 `-develop` 实际把玩。

## 实施顺序

1. main.cpp 参数 + GameAPP 三个静态 bool
2. 收费点守卫（无冷却/无视阳光）
3. GameScene 面板 + 切换按钮
4. 召唤放置模式
5. 直跳关卡 + 下一波按钮
6. smoke_develop.json + 联调
