---
name: project-pvz-win7-yy-thunks
description: Windows 7 x64 系统 API 兼容层、YY-Thunks overlay port 与链接后 PE import 门禁
metadata:
  node_type: memory
  type: project
---

# Windows 7 x64 系统 API 兼容层（2026-08-11）

## 背景与结论

Windows 7 玩家在进入 `main()` 前收到 `PlantsVsZombies.exe - 无法找到入口`，具体为 KERNEL32 中缺少 `CopyFile2`。对当时 `clang-release` EXE 的完整导入表复核还发现 `CreateFile2` 与 `GetSystemTimePreciseAsFileTime`；三者最低都从 Windows 8 才原生提供。项目继续支持 Windows 7 SP1 x64，因此不能只在单个调用点加源码特判，也不能仅修截图中的第一个入口。

补齐系统 API 后，原 AVX2 发布版曾出现启动期 `unknown software exception (0xC000001D)`；这是非法指令而非缺入口。硬件声称支持 AVX2 仍不能证明 Win7 已为进程启用 AVX/XState，因此新增 `clang-release-noavx2` 作为同配置回退版，排除整个项目源码的全局 `/arch:AVX2`。该回退版的玩家视频已确认不再出现非法指令弹窗，而是继续执行到 SDL 加载 Vulkan 时，因系统 `vkEnumerateInstanceExtensionProperties` 返回 `VK_ERROR_OUT_OF_HOST_MEMORY (-1)` 正常失败退出；后者是独立的 loader/驱动环境问题。

现由仓库内 `cmake/vcpkg-ports/yy-thunks` overlay port 固定 YY-Thunks 1.2.2（MIT）。官方 Lib/Objs 资产都用 SHA512 固定：Clang/LLD 使用 `Lib/6.1.7600.0/x64` 的替代 import libraries，MSVC `link.exe` 使用 Objs 包的 `YY_Thunks_for_Win7.obj`。链接 subsystem 固定为 6.01；新系统仍调用原生 API，Win7 缺失时由 YY-Thunks 动态探测并走旧 API 或内部回退。

## 构建契约

- `CMakePresets.json` 的 base preset 指定 `VCPKG_OVERLAY_PORTS=${sourceDir}/cmake/vcpkg-ports`，`vcpkg.json` 在 Windows x64 声明 `yy-thunks`。
- LLD 不能直接链接聚合 obj，否则会产生重复符号；必须把整套替代 import library 目录用 `target_link_directories(BEFORE ...)` 放在 WinSDK 前。
- MSVC 不能复用 Lib 包内生成物中的聚合 obj；官方 Objs 包的 `YY_Thunks_for_Win7.obj` 才是 `link.exe` 手动集成入口。曾用错 Lib 包 obj，最小目标以未解析 `ProcessPrng` 明确失败，修正后通过。
- `pvz_win7_compat` 必须链接到游戏以及所有 Windows 测试 EXE；`/SUBSYSTEM:CONSOLE,6.01` 不能回升。
- `cmake/assert_win7_imports.ps1` 在每个 EXE 链接后解析 `llvm-readobj --coff-imports`，按名字和 ordinal 与 YY-Thunks 的 `Config/x64/6.1.7600.txt` 核对。没有解析到 imports 或出现任何 Win7 不存在的直接入口都必须令构建失败；禁止维护人工放行表。

## 能力边界

该层只消除 Windows 装载器对 Windows 8+ 系统 API 的静态硬依赖。最低 GPU 仍要求 Vulkan 1.2、项目使用的 bindless descriptor indexing 能力与 8192 个 update-after-bind combined image sampler。CPU 指令集与 YY-Thunks 无关：默认 `clang-release` 仍要求 x64 + AVX2；`clang-release-noavx2` 只把项目源码降到 x64 基线，并保留其余 Release 设置。Vulkan 函数装载与 1.2 回退仍由 [SDL2 + Volk 动态 Vulkan loader](project_pvz_volk_dynamic_loader.md) 负责，不能用 YY-Thunks 替代。

## 当前验证证据

- 修改前审计旧 EXE 会精确失败于 `KERNEL32.dll!CopyFile2`、`CreateFile2`、`GetSystemTimePreciseAsFileTime`。
- `clang-release` 全量构建成功；游戏、`SaveMigrationTests`、`SaveSchemaTests` 分别通过 378、111、93 个直接 imports 的 Win7 x64 审计，PE OS/subsystem version 均为 6.1。
- `ctest --test-dir build/clang-release --output-on-failure` 为 2/2 通过；当前桌面可见运行 `demo_peashooter.json` exit 0、14 条命令完成、`script finished OK`，状态与三张截图正常。
- MSVC 最小 `SaveMigrationTests` 目标使用官方 Win7 obj 链接成功，206 个 imports 通过门禁，`ctest -R save-migration` 1/1 通过。
- `clang-release-noavx2` 全量构建成功；游戏 152 条编译命令均无 `/arch:AVX2`，同时保留 `/O2`、fast-math 与 LTO；游戏 EXE 378 个 imports 通过门禁，PE OS/subsystem version 均为 6.1，CTests 2/2 通过；当前桌面可见运行 `demo_peashooter.json` exit 0、14 条命令完成、`script finished OK`，状态与三张截图正常。
- `.agents/skills/` 没有构建、Windows 兼容、YY-Thunks 或 Vulkan/Volk 专项技能；本次无相关技能契约可更新。
