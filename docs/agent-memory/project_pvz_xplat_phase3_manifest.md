---
name: project_pvz_xplat_phase3_manifest
description: 2026-06-25 跨平台 phase-3 完成(资源目录枚举改走构建期清单)+ SDL3 迁移为 APK 列目录是陷阱的裁决
metadata:
  node_type: memory
  type: project
  originSessionId: ae33515d-37ea-45e5-8f13-7c4e56643e46
---

2026-06-25 完成跨平台 phase-3，commit **c50db1e(未push)**：收口 phase-1 审查留下的最后缺口——两处运行时 `std::filesystem` 目录枚举(`ResourceManager::LoadAllImagesFromPath` / `ParticleXMLLoader::LoadFromDirectory`)在 Android 读不了 APK assets(AAssetManager 不列子目录)。

**方案=构建期烘焙清单**：`cmake/gen_manifest.cmake` 经 POST_BUILD `file(GLOB_RECURSE)` **紧挨 exe 的真实资源目录**(源码树无 resources/、CMake 不拷资源、实体在 build/<preset>/resources)生成 `resources/manifest.txt`(每次构建重生成=零过期)；运行时新增 `FileManager::ListResourceFiles(dir,ext)` 经 `LoadFileAsString`(=SDL_RWops，phase-1 已铺，APK 可读)读清单按名加载。清单行与旧 `directory_iterator` 产出**逐字一致**(`./resources/...`)，桌面行为零变化；**清单缺失桌面回退 std::filesystem、Android 报错**——桌面也走清单路径(非 #ifdef 只在安卓)，使桌面 AutoTest 成为 Android 读取逻辑的试金石。

**核心裁决(再有人提"迁 SDL3 用 SDL_EnumerateDirectory 实现 APK 跨平台"直接引此，免重新调研)**：SDL3 对 APK assets 的枚举(`assets://` 前缀 + `SDL_EnumerateDirectory`)**只在 SDL 未发布的 main 分支(将来的 3.6.0)**——官方 README-android 写"as of SDL 3.6.0"；但 `release-3.6.0` tag **GitHub 404 不存在**，最新稳定版 = **3.4.10**(其 README-android 无 assets://)，上游 vcpkg 最新 sdl3 = 3.4.10、本地 vcpkg-master = **3.4.6**，均无此功能。SDL3 用偶数小版本=稳定(3.2/3.4 已发，3.6 在孵化)。故为一个尚未进任何稳定版的功能去做整套 SDL2→SDL3 破坏性迁移(事件结构体/Vulkan 表面/surface 格式/音频字体签名全变；本仓库 ~33 文件涉 SDL)**不划算**；清单方案**库无关**、现有 SDL2 稳定栈即可用，是正解。官方 codemod 脚本(`rename_headers.py`/`rename_symbols.py`+`SDL_oldnames.h`)只覆盖纯改名，改了参数个数/结构体布局的(RWread→ReadIO 参数变、keysym.sym→key、CreateWindow 去 xy、Vulkan 两函数签名、TTF/Mix 签名、surface 格式)都得手改。

验证：clang-release **0 warn/0 error**；清单 2644 行(image/reanim 2418、particles/config 14，与磁盘精确吻合且不含自身)；AutoTest demo_peashooter 退出 0、run.log 零 ERROR、截图植物/僵尸/豌豆 reanim 正常。spec=docs/superpowers/specs/2026-06-25-resource-manifest-enumeration-design.md。

承接 [project_pvz_xplat_phase1_review](project_pvz_xplat_phase1_review.md)（其"目录枚举×2 留 phase-3 烘焙清单"现已落地）。资产/AutoTest 坑见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)。
