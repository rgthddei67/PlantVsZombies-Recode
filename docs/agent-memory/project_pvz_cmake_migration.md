---
name: pvz-cmake-migration
description: 2026-06-13 CMake+vcpkg(manifest) 迁移并已统一（.sln/.vcxproj 已删，CMake 唯一构建系统）；坑=vcpkg 特性集/构建环境/copy_directory
metadata:
  node_type: memory
  type: project
  originSessionId: 83b50c95-5f75-4a56-8bd0-5051666fb921
---

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
