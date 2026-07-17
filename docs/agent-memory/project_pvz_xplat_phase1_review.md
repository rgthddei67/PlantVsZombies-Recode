---
name: project_pvz_xplat_phase1_review
description: 2026-06-25 跨平台 phase-1(代码可移植)审查通过 + 遗留 phase-3 目录枚举缺口与技术约束
metadata:
  node_type: memory
  type: project
  originSessionId: ed8bcdb4-c450-4444-8fd6-d25ba1193735
---

跨平台化第一阶段(代码可移植) `feature/cross-platform-portability` 审查(plan Task 6)完成。spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-24-cross-platform-portability*`。**2026-06-25 主人选 Option1：FF 合并 master(tip=b3ff1da)+push origin；feature 分支已删。**

**已落地 commits**：Tasks 1-5 共 7 commit(CrashHandler/main `system()` 平台隔离 → FileManager 读取换 SDL_RWops → ResourceManager 图/字/音换 `_RW`(freesrc=1) → shader+年鉴 info.txt 收编 → GameInfoSaver `GetSaveRoot()` 桌面`./saves`/Android `SDL_GetPrefPath`)；审查期补 2 commit：`66efa31`(FileManager.h `<fstream>` 下沉到 .cpp,include-what-you-use)、`b3ff1da`(pugixml `load_file`×2 收编)。

**验证(强制全量重编,非 "no work")**：clang-release & msvc-debug 各 `--clean-first` 重编 0 warning/0 error；AutoTest demo_peashooter/smoke_small_sun/almanac_click/smoke_gameplay/smoke_quit 全 exit0、run.log 无 ERROR、截图渲染如常。桌面零行为变化=本阶段验收红线。

**审查发现 spec 漏勘的"绕过 SDL_RWops"资源读取路径**(都在启动主路径,非死代码)：
- ① pugixml `doc.load_file()`×2(`ResourcesXMLConfigReader::LoadConfig` 读资源总清单、`ParticleXMLLoader::LoadFromFile` 读粒子配置)——内部走 C `fopen` 绕过 RWops。**本次已收编**改走 `FileManager::LoadXMLFile`(底层 SDL_RWFromFile)。
- ② **目录枚举 ×2 留 phase-3 未改**：`ResourceManager::LoadAllImagesFromPath`(默认枚举 `./resources/image/reanim/` 全 PNG,`GameApp.cpp:197` 启动调用) + `ParticleXMLLoader::LoadFromDirectory`(via `FileManager::GetFilesInDirectory`→`std::filesystem::directory_iterator`,`ParticleConfig.cpp:14`)。

**关键技术约束(再有人提"统一用SDL"时引此)**：SDL_RWops / Android AAssetManager **无目录列举 API**——`SDL_RWFromFile` 只能按名读单个文件。所以"统一用SDL"治不了枚举；phase-3 正解=build 期**烘焙资源文件名清单**(manifest),运行时读清单再逐个按名 `SDL_RWFromFile`。这与 spec 自列的第三阶段"资源打包进 APK assets"是同一件事。注:spec 误称 `GetFileSize`/`GetFilesInDirectory` "仅桌面存档路径用",实际粒子加载在用 `GetFilesInDirectory`,该非范围理由前提是错的。

**其它**：`FileManager::FileExists`/`GetFileSize` 仍用 `std::ifstream`,但都是死代码(零调用方),且 spec 明列 `GetFileSize` 本次不动,故未改;`FileExists` 是 APK 隐患(ifstream 看不到 APK assets),phase-3 若有人拿它 gate 资源加载会在 Android 误判 false。`SDL_LoadBMP`(CursorManager) 是虚惊——SDL2 里它本就是 `SDL_LoadBMP_RW(SDL_RWFromFile(...),1)` 宏,APK 安全。

资源/AutoTest 坑见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)；构建系统见 [project_pvz_cmake_migration](project_pvz_cmake_migration.md)。
