---
name: pvz-cmake-migration
description: CMake+vcpkg 唯一构建系统；四个 preset 统一使用 clang-cl+lld-link，普通任务用 clang-debug 快速迭代并以 clang-release 交付
metadata:
  node_type: memory
  type: project
  originSessionId: 83b50c95-5f75-4a56-8bd0-5051666fb921
---

**2026-08-24 当前构建契约（取代下方迁移初期的 preset/运行目录描述）：**
- `clang-debug`、`clang-release`、`clang-release-noavx2`、`clang-playtest` 四个 preset 全部继承 `clang-base`，统一使用 VS LLVM 的 `clang-cl + lld-link`；旧 `msvc-debug` preset 已删除，不再配置 `cl.exe` 编译器。
- 普通功能、逻辑、UI、资源和存档任务的修改过程中，使用 `clang-debug` 做增量编译、F5 和范围最小的诊断 AutoTest；它保留 `/Ob0 /Od /RTC1 -MTd -Zi`、Debug CRT、断言与 PDB。任务完成后必须整体配置并编译一次 `clang-release`，最终相关回归也以该产物为交付证据，Debug 结果不能替代 Release 结果。
- 性能、内存布局、并发、编译器优化、LTO 或仅在 Release 出现的问题属于优化相关任务，修改、验证与交付全程使用 `clang-release`：Clang `/O2` + AVX2 + fast-math + LTO，并以 `/Z7 -gline-tables-only -gcodeview-ghash` + `/DEBUG:GHASH` 生成与优化机器码匹配的精简外置 PDB。它只保留符号化函数栈、内联关系和源码行所需的信息，不包含变量、变量位置或类型；LLVM 文档明确把 `-gline-tables-only` 定义为可生成符号化回溯的最小信息。
- GHASH 让 LLD 复用 CodeView 哈希以缩短合并；Release EXE 不嵌入调试符号，只保留匹配外置 PDB 所需的约百字节 CodeView 定位记录，`/PDBALTPATH:PlantsVsZombies.pdb` 保证其中没有本机构建绝对路径。`CrashHandler` 的异常地址和调用栈保存合同不变。
- `clang-release-noavx2` 是 Win7/旧环境发生 `0xC000001D` 时的独立发布预设；只把 `PVZ_ENABLE_AVX2` 关掉，仍保留 `/O2`、fast-math、LTO、静态运行时和精简行表 PDB，不能取代默认预设。
- 该开关约束游戏自己的编译单元，不保证最终静态 EXE 逐字节不含 AVX：libjpeg-turbo 等依赖仍可能内置由运行时能力检测保护的多套 SIMD 实现。验收应同时核对游戏 152 条编译命令无 `/arch:AVX2` 与目标 Win7 实机不再触发 `0xC000001D`，不能只对最终 EXE 搜指令助记符。
- `clang-playtest` 不再承担普通快速迭代；仅在主人明确要求 Clang 无 LTO，或确实需要更易断点调试的 Clang 符号布局时使用。
- `clang-release-noavx2`、`clang-playtest` 与 `clang-debug` 的 `resources`/`font` 是指向 `build/clang-release/` 同名实体目录的 NTFS Junction；配置只创建一次联接，不复制资产。Shader、存档与 AutoTest 输出仍按 preset 隔离。
- Visual Studio `launch.vs.json` 使用 `${cmake.binaryDir}` 作为工作目录；所有运行仍从 exe 自己的 `build/<preset>/` 启动。
- `find_package(Vulkan)` 可能优先命中 vcpkg `vulkan-headers`；VMA 头固定从 `$ENV{VULKAN_SDK}/Include/vma/vk_mem_alloc.h` 取得，不能再通过 `Vulkan_INCLUDE_DIR` 间接推导。

2026-08-23 迁移证据：从不存在的 `build/clang-debug` 全新配置并完成 180 步全量构建，CMake 识别 `Clang 22.1.3 with MSVC-like command-line`；实际游戏编译命令为 `clang-cl.exe /Ob0 /Od /RTC1 -MTd -Zi`，实际链接命令为 `lld-link.exe /debug`，生成约 121 MB `PlantsVsZombies.pdb`。游戏与三个测试目标的 Win7 导入审计分别通过，`resources`/`font` Junction 指向唯一 `build/clang-release` 实体。`clang-debug` 与重新配置后的 `clang-release` 各自 3 个 CTest 全部通过，当前桌面可见 `smoke_quit` 两次均退出 0、`status=passed`、Vulkan 正常初始化并 `script finished OK`。三个权威资源生成脚本同步收敛为只写 `build/clang-release/resources`，不再按 Debug preset 重复写同一 Junction。该段只记录当时迁移取证，PDB 生成方式以 2026-08-24 当前契约为准。

2026-08-24 精简 PDB 证据：同一 `PlantsVsZombies.exe` Ninja 目标此前完整 PDB 的链接及后处理为
96.779 秒，本次最小行表 + GHASH 为 52.046 秒，缩短约 46%；PDB 从 74,108,928 字节降至
55,205,888 字节。`llvm-pdbutil` 确认项目 LTO 模块保留 6424 条函数记录、局部变量记录为 0，
`llvm-symbolizer` 可将 EXE 地址还原为 `GameScene::Update` 与 `GameScene.cpp:2035`。`llvm-readobj`
确认 EXE 没有 `.debug` 符号节，仍有 x64 栈展开所需 `.pdata`；CodeView 定位数据仅 44 字节，
PDB 路径为相对文件名 `PlantsVsZombies.pdb`。Release 全量构建与 Win7 378 项导入审计通过，
CTest 3/3 通过。

2026-06-13 完成（4edb6c8 接入 → **a14a26c 统一**，主人主动要求"搞2套太乱"）：
.sln/.vcxproj/.filters 已删，**CMake 是唯一构建系统**。
- VS 开发走「打开文件夹」模式；F5 配置在根 `launch.vs.json`（cwd=x64\Release + -Debug 变体）。
- MSVC-Debug-MCP 的 build_solution 族已不可用；Debug/Operate 族在文件夹模式继续可用。
- x64\Release 仅作资源+运行时工作目录，旧 exe/pdb 已删，别再当产物引用。
- spv 拷贝坑：`cmake -E copy_directory` 拷的是目录**内容**，目标须写全 `Shader/spv`（2133ec1，
  当时错拷平铺被 x64\Release 现成 shader 掩盖，AutoTest 没暴露）。

- Presets：`msvc-debug` / `msvc-release`(/O2 AVX2 fast-math LTCG) / `clang-release`(-O3 -march=native -flto, lld)，
  Ninja + `x64-windows-static` + MT/MTd，toolchain 指 `D:/PVZ/vcpkg-master`。
- **构建环境坑**：必须先 `VsDevCmd.bat -arch=x64`，且 `...\Microsoft Visual Studio\Installer`（vswhere）要在 PATH，
  否则 VsDevCmd 内部报错（非致命但易误判）。
- **vcpkg 特性坑（实测两连击）**：默认特性集 ≠ 经典安装时的特性——缺 `sdl2[vulkan]` → 运行时
  "Vulkan support not configured in SDL"；缺 `sdl2-image[libjpeg-turbo]` → "JPEG images are not supported"。
  新加依赖时对照旧 ClangRelease 手抄 lib 名单可反推所需特性。
- manifest 重编原因：ABI 哈希含编译器版本+端口版本，与经典安装不同则缓存未命中；编一次后进二进制缓存，
  后续 preset configure 秒装。
- shader 输出名坑：CMake `NAME_WE` 会把 `batch.vert.glsl` 剥成 `batch`（与 .frag 撞名），须只剥 `.glsl`。
- 产物 `build\<preset>\`，只拷 Shader 不拷 resources（主人要求省磁盘）；运行用 `x64\Release` 当工作目录。
- 源文件收集已改 `GLOB_RECURSE CONFIGURE_DEPENDS`（commit 1e8b7b9）：新增 .cpp 自动入编，CMake 侧零维护；
  不编译的文件须加 REMOVE_ITEM 排除名单（现有：AttachmentSystem.cpp）。
- **PS5.1 编码坑**：用 PowerShell Get-Content/-replace/Set-Content 改 UTF-8 中文文件会 GBK 乱码+丢换行，
  改这类文件只能用 Edit/Write 工具。
- 2026-07-17 许可证汇总补齐 SDK 侧依赖：`gen_third_party_licenses.cmake` 除 vcpkg copyright 外，
  必须传 `VMA_HEADER=<VulkanSDK>/include/vma/vk_mem_alloc.h`，从文件头提取上游 MIT 原文；
  `CMakeLists.txt` 配置期缺 VMA 直接失败，避免发布包静默漏声明。根项目 MIT 署名为 `2026 rgthddei67`。
