---
name: project_pvz_warnings_cleanup
description: PvZ 编译警告清零已完成 + 警告验证须用 clang、运行须从 build/<preset>/ 的操作要点
metadata:
  node_type: memory
  type: project
  originSessionId: 6014a21d-bcaa-4665-9a45-6f4dd085d5a3
---

2026-06-13 完成编译警告清零(`docs/TODO-warnings-cleanup.md` 的 TODO 4 类 + 执行中发现的 18 处，
主人确认全清)。clang-release clean 重建 **0 warning / 0 error**，三 preset 全 0 error，
AutoTest exit 0 行为不变。17 文件改动、净 -13 行，**未 commit（等主人）**。

关键操作要点(以后会话避坑)：

- **警告归零验证必须用 clang-release**：MSVC 默认配置**不报** `-Wnonportable-include-path`/
  `-Wreorder-ctor`/`-Wunused-*`/`-Wswitch` 这些诊断（主人明确指出"msvc 默认没有 Warn"）。
  原 TODO 评估漏列 18 处正因为只看了某个不全的日志。`cmake --build --preset clang-release`
  后 `Select-String build.log -Pattern "warning:"`。

- **运行 AutoTest 的工作目录是 `build/<preset>/`，不是 CLAUDE.md 写的 `x64/Release`**。
  `x64/Release` 是 CMake 迁移前 vcxproj 的**陈旧产物**（满是 .obj，且无 Shader/）。
  CMake 构建会把 font/resources/Shader/saves 全拷到 `build/<preset>/` 旁边 exe，
  从那里运行才齐全：`Push-Location build\clang-release; .\PlantsVsZombies.exe -AutoTest <abs.json> -Seed 42`。
  （CLAUDE.md 的 Build/Run/AutoTest 节已于本次更正：运行目录改 build/<preset>/、构建脚本改为
  Installer 先进 PATH 再导 VsDevCmd 以消除 `'vswhere.exe' is not recognized` 噪音。）

- **大小写 only 改名**（如 `GameAPP.h`→`GameApp.h`）：git 索引可能早已是目标大小写、
  只磁盘残留——`git mv` 报 "not under version control"，要用普通 `mv` 两步（临时名中转）
  强制翻转，改完 `git status` 反而干净。

- **reorder-ctor 主人偏好**：改**类内默认成员初始化器**而非重排初始化列表；带参构造函数
  改函数体内赋值。见 [feedback_collaboration_style](feedback_collaboration_style.md)。

- **-Wswitch 修法**：补显式 `case ...: break;`（带注释说明为何不处理）而非 `default:`，
  保留未来新增枚举值仍触发警告的保护。

构建命令/preset 详见 [project_pvz_cmake_migration](project_pvz_cmake_migration.md)；AutoTest 用法见 [project_pvz_autotest_suite](project_pvz_autotest_suite.md)。
