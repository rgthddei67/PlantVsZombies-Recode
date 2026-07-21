---
name: project_pvz_save_location_migration
description: 2026-07-21 Windows 正式存档迁到 Saved Games，并安全迁移旧游戏目录存档
metadata:
  node_type: memory
  type: project
---

2026-07-21 将 Windows 正式存档从随工作目录变化的 `./saves` 迁到系统 Known Folder：`FOLDERID_SavedGames/PlantsVsZombies/saves`（默认 `%USERPROFILE%\Saved Games\PlantsVsZombies\saves`）。Android 继续使用 `SDL_GetPrefPath("PvZ", "PlantsVsZombies")`，Linux 暂不改变 `./saves`。

**迁移契约**：首次真实存档访问才初始化中央目录并扫描旧 `./saves`；每个普通文件按“复制到目标 → 逐字节校验 → 删除源文件”迁移，故支持游戏在 D 盘、用户目录在 C 盘的跨卷场景。目标已有同名同内容文件时删除旧重复档；同名不同内容时绝不覆盖，中央档优先、旧档原地保留并警告。复制/校验/删除失败时源文件保留，读档若中央文件缺失则逐文件回退旧目录；关卡删档同时清理中央和遗留同名档，避免回退复活已删除进度。

**隔离契约**：AutoTest 在进入路径初始化前就短路；`-AutoTestLoadSave` 继续只读当前构建目录的 `./saves/level{N}_data.json`，不读取、不迁移主人的中央存档，保存和删除仍短路。

**实现结构**：`SaveLocation` 隔离 Windows/SDL 平台 API，避免 `ShlObj.h` 的 `DeleteFile`、`CreateDirectory`、`small` 宏污染 `GameInfoSaver`；`SaveMigration` 是纯文件系统迁移器，并由独立 `SaveMigrationTests` 覆盖成功迁移、重复档、冲突档和目标不可创建。`FileManager` 的本机文件系统入口统一用 `std::filesystem::u8path`，使 Windows 非 ASCII 用户名路径可正常读写。

**验证**：`clang-release` 完整编译通过；`SaveMigrationTests` 4 类边界通过；当前桌面可见运行 `demo_peashooter`，窗口标题“植物大战僵尸中文版”、exit 0、`run.log` 14 条命令全部完成且无 ERROR，`state.json` 为 level 1 / GAME / 1 植物 / 1 僵尸，三张截图正常。AutoTest 在中央路径初始化前短路，验证过程中未触碰真实玩家存档。
