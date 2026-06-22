# 编译警告清零（✅ 2026-06-13 已完成）

clang-release clean 重建 **0 warning / 0 error**；msvc-release / msvc-debug 亦 0 error；
`demo_peashooter.json` AutoTest exit 0、state 与基线一致（行为不变）。

## TODO 原列 4 类（已修）

1. **`-Wnonportable-include-path`**：磁盘文件名 `GameAPP.h`→`GameApp.h`。
   实为工作区大小写脱节——git 索引早已是 `GameApp.h`，只磁盘残留大写；
   `git mv` 会报 "not under version control"，需用普通 `mv` 两步（临时名中转）。
   另：仓内有 5 处 include 写的是 `GameAPP.h`（GameApp.cpp/main.cpp/AnimatedObject.cpp/
   TestDriver.cpp/GameMessageBox.cpp），已统一为 `GameApp.h`（原 TODO "全仓不用改 include" 判断有误）。
2. **C4477/C4267（CrashHandler.cpp）**：`strlen`→`static_cast<DWORD>`、
   `Rsp`(DWORD64)→`reinterpret_cast<void*>` 配 `%p`。
3. **`-Wreorder-ctor` ×3**：按主人指示改**类内默认成员初始化器**（删构造函数初始化列表）：
   - `ThreadPool.h`、`ParticleXMLConfig.h/.cpp`：标量/ValueRange 默认值就近写在成员声明处。
   - `ColliderComponent.h`：构造函数带参，改函数体内 `this->x = x` 赋值（成员本就有类内默认值）。
4. **`-Wunused-const-variable` ×2**：删 `BUTTON_SPACING`、`BOTTOM_MARGIN`（GameMessageBox.cpp）。

## 执行中发现 TODO 漏列、本次一并清掉的 18 处（主人确认全清）

clang 报但 MSVC 默认不报，故原评估遗漏。**警告归零验证必须用 clang-release。**

- `-Wunused-lambda-capture` ×8：lambda 多捕获了没用的 `this` → 改 `[]`
  （CardComponent.cpp、GameScene.cpp ×2、MainMenuScene.cpp ×5）。
- `-Wunused-variable` ×5：CursorManager `width`/`height`、CardSlotManager `plantMgr` ×3 → 删。
- `-Wswitch` ×3：补显式 `case` 而非 `default`（保留未来新枚举值仍触发提醒的保护）：
  - CardDisplayComponent `CardState::Cooling`（冷却由 UpdateCooldown 单独处理）
  - GameScene `IntroStage::FINISH`（入场动画结束，无需处理）
  - GameScene `PromptStage::NONE`（无激活提示，无需处理）
- `-Wunused-but-set-variable` ×1：CardSlotManager `cardComp` → 改为不绑定的存在性判断 `if (selected->GetComponent<CardComponent>())`。
- `-Wunused-private-field` ×1：CardSlotManager.h `slotSpacing`（全仓仅声明处出现）→ 删。

改动共 17 文件，净 -13 行；已 commit。
