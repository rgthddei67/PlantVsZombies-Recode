# CMake + vcpkg 迁移设计（2026-06-13）

## 目标
为 PlantsVsZombies 增加 CMake 构建（与现有 .sln/.vcxproj 并行共存，旧工程零改动），
使用 vcpkg manifest 模式管理依赖，并提供 clang-cl 配置。

## 决策（已与主人确认）
1. **并行保留**旧 VS 工程，新增 CMake 文件，验证稳定后再考虑删除旧工程。
2. **vcpkg manifest 模式**：根目录 `vcpkg.json`，triplet `x64-windows-static`，
   toolchain `D:/PVZ/vcpkg-master/scripts/buildsystems/vcpkg.cmake`。
   Debug/Release 库由 vcpkg toolchain 按 `CMAKE_BUILD_TYPE` 自动匹配（debug/lib vs lib）。
3. **独立 build 目录**：`build/<preset>/`。只拷贝 Shader spv，**不拷 resources/font**
   （运行时工作目录设为 `x64\Release`，资源都在那里）。
4. `clang-release` 保留 `-march=native`（自用，不分发）。
5. Win32 配置不迁移（项目 x64 only）。

## Presets（生成器 Ninja）
| Preset | 编译器 | 对应旧配置 |
|---|---|---|
| `msvc-debug` | cl.exe | Debug\|x64：/MTd、/utf-8、4MB 栈 |
| `msvc-release` | cl.exe | Release\|x64：/O2 /arch:AVX2 /fp:fast LTCG /MT、_HAS_ITERATOR_DEBUGGING=0 |
| `clang-release` | clang-cl | ClangRelease\|x64：-O3 -march=native -flto（lld-link）+ 同套宏 |

运行时库：`CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded$<$<CONFIG:Debug>:Debug>`。

## 依赖
vcpkg.json：sdl2、sdl2-image、sdl2-ttf、sdl2-mixer、glm、nlohmann-json、pugixml。
Vulkan 用系统 SDK（`find_package(Vulkan)`，`VULKAN_SDK=D:\VulkanSDK`）。
全部走 imported targets，淘汰旧 ClangRelease 手抄静态库名单。
附加系统库：version setupapi imm32 winmm（+ legacy_stdio_definitions）。

## Shader
自定义命令复刻 glslc：4 个 GLSL → `Shader/spv/*.spv`（增量），post-build 拷到
`build/<preset>/Shader/`。

## 验收
三个 preset 构建通过；`msvc-release` 产物以 `x64\Release` 为工作目录跑
`demo_peashooter.json` AutoTest（exit 0 + 截图正常）作为行为等价验证。
