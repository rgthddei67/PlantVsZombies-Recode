# Android 移植首次审计

日期：2026-09-05。源码基线：`a41e18668c9ce3916b3b3239d4f64017c7205457`。

目标是扩展和学习，设备由主人提供：ARM Android 12 模拟器、骁龙 835 Android 9 真机。本次是源码与构建入口静态审计，没有编译 Android、连接设备或验证性能。设备的实际 ABI、图形版本及扩展尚未读取。

## 结论

项目已有 SDL2、资源清单和 Android 存档路径基础，适合继续移植；目前仍不能直接生成可用 APK。优先保留游戏逻辑与 SDL2，在平台入口、构建、图形、资源和输入边界进行适配。建议第一版以 OpenGL ES 3.0 为候选基线，先探测设备能力，不把现有 Vulkan 1.2 要求强加给旧手机。此方案尚未实施。

## 当前证据与处理顺序

| 优先级 | 当前源码证据 | 移植工作 |
|---|---|---|
| P0 构建 | `CMakeLists.txt:177` 建立 executable；197 起无条件传 `/utf-8 /W3 /sdl /EHsc`，Clang 分支仍用 clang-cl 参数、AVX2 和 Windows 链接选项；`CMakePresets.json` 基础配置是 x64-windows-static | 保留 Windows 预设，增加 Android NDK 工具链与 ARM 目标，按平台隔离编译/链接选项；Android 游戏产物为供 Activity 加载的共享库。不能仅关闭 AVX2 就认为完成 ARM 支持 |
| P0 启动与 APK | `main.cpp:1` 定义 SDL_MAIN_HANDLED，入口是普通 main；仓库文件检索未找到 Gradle/AndroidManifest/Android 工程 | 用版本匹配的 SDLActivity 接入 SDL_main，增加 Gradle、Manifest、依赖共享库加载和 APK assets 构建入口 |
| P0 图形 | `GameApp.cpp:197` 请求桌面 Core profile；`Renderer/OpenGLRenderer.cpp:61` 起拒绝低于桌面 3.3 的 Context，并要求 Core profile；`Shader/opengl` 使用 330/430 core | 在现有 CPU Batch 基础上增加 GLES Context、头文件/函数兼容和 GLSL ES 300 着色器；逐项检查纹理格式、FBO、读回与混合，不只改版本字符串。第一版不依赖 SSBO 快路 |
| P0 Vulkan 构建耦合 | `Renderer/VulkanContext.cpp:181,300` 要求 loader/device 至少 1.2；CMake 强制查找 Vulkan/VMA/glslc，SPIR-V 目标为 vulkan1.2 | 初期 Android GLES 构建应能不编译 Vulkan 后端。须梳理 Graphics 对 Vulkan 类型/实现的直接依赖，不能只删 find_package。后续真机能力符合要求再接入 Vulkan |
| P0 依赖 | vcpkg 清单包含 SDL2/image/ttf/mixer、libopenmpt、volk、glm、JSON、pugixml；YY-Thunks 已有 Windows x64 条件 | 逐个验证 Android triplet、PIC、运行库和加载顺序，尤其 libopenmpt 自定义 overlay；未进行交叉编译，不能宣称依赖全部可用 |
| P0 资产 | `FileManager.cpp:30` 使用 SDL_RWFromFile；ListResourceFiles 使用 manifest；当前 POST_BUILD 从可执行目录生成清单 | APK 打包从 `build/clang-release/resources` 和同级 font 的权威资产生成暂存内容，包含所用 Shader 和清单，不能维护另一份手改资产。核对路径大小写及 `./` 前缀在实际 APK 中的解析 |
| P1 音乐读取缺口 | `Game/AdaptiveMusicPlayer.cpp:50` 的 ReadBinaryFile 直接用 ifstream，182 起读取 MO3；失败后代码回退 OGG | 改走现有二进制资源读取接口；回退 OGG 不算动态音乐验收通过。SDL2_mixer 的普通音效读取已走 RWops，但仍须实际测试声音及后台暂停 |
| P1 触屏 | `UI/InputHandler.cpp:11` 的事件 switch 仅处理键盘、鼠标；鼠标按下/释放不更新事件位置，只由 motion 更新坐标 | 明确单指点击/拖动/取消契约、补齐事件坐标、消除触摸与 SDL 合成鼠标重复输入。现有 ScreenToLogical 可复用，但需检查窗口坐标与 drawable 像素换算；取消选择不能只依赖右键 |
| P1 生命周期 | `GameApp.cpp:519` 主循环显式处理 QUIT 和窗口尺寸变化；源码检索未见 SDL_APP_* 处理 | 增加后台暂停、清除按住状态、恢复计时、音频暂停和返回键处理；根据 SDL 与设备行为处理 Surface/Context 重建并验证资源恢复 |
| P1 存档及取证 | `SaveLocation.cpp:28` 已用 SDL_GetPrefPath；AutoTest 仍从普通文件流读脚本并向相对目录输出 | 复用存档 schema，确认路径失败时不写不可写 CWD；Android 脚本、截图和日志另设可写位置及 adb 拉取方式。Windows 崩溃处理已隔离，Android 需 logcat/原生崩溃取证 |

## 可以保留的基础

- 已有 SDL2 窗口、事件及音频基础，不需要为移植先升级 SDL3。
- FileManager、ResourceManager 的大部分资源读取已使用 SDL_RWops；清单枚举避免依赖 APK 内目录的普通 filesystem 遍历，但不代表所有读取路径均已覆盖。
- Windows CrashHandler 的系统头与实现已放在 `_WIN32` 条件下，main 中的 chcp/cls 也已隔离。
- Android 存档目录分支已存在，游戏逻辑、对象所有权和存档格式不应因平台适配另建一套。
- 逻辑画布与 letterbox 逆变换已存在；先保持现有 1100×600 逻辑坐标，横屏适配显示区域，验证清楚后再讨论移动 UI 重排。

## 分阶段验收

1. **工具链和设备探测**：定位 SDK/NDK/JDK、模拟器 adb；读取设备 ABI、Android API、GLES 版本和 Vulkan 能力。首选 arm64-v8a，但按设备实际支持选择。当前 PATH 只找到 java，未找到 adb/gradle，未设置 Android 环境变量，默认 `%LOCALAPPDATA%/Android/Sdk` 不存在；这些有限检查不代表其他目录没有安装工具。
2. **最小 Android 启动程序**：可安装 APK，SDLActivity 进入原生代码，创建 GLES Context、显示画面并输出 GPU 信息；两种设备都实际运行。此时尚不算游戏移植完成。
3. **游戏第一帧**：交叉编译共享代码和依赖，加载真实资源，显示主菜单；逐项验证字体、透明混合、裁剪、资源键与声音。不能以空纹理/空 Animator 掩盖加载失败。
4. **可玩一关**：选卡、种植、收集、铲除、暂停/返回、存档和继续游戏；必须用真机触屏检验。涉及具体植物等行为改动时再按对应技能实施。
5. **连续运行**：记录帧时间和内存，验证切后台/锁屏/恢复及音频。根据实际瓶颈优化，不预先承诺骁龙 835 的 FPS。

修改共享渲染实现后，应保留 Windows clang-release 的相关可见回归，并按后端改动范围验证默认 Vulkan、NoInstance、OpenGL；Android 另留安装结果、logcat、状态与真机截图。仅本次审计文档无需构建和 AutoTest。

## 审计边界与技能检查

本次没有改变运行行为、玩法、地图或资产。检查了 `.agents/skills/` 的路由及地图技能/reference 中权威资源路径、注册闭环和截图验收要求；它们仍适用于现有 Windows 流程，与本报告不冲突。没有修改技能，因此无需 quick_validate。待 Android 实现并验收后，再将实际的新构建/打包/验证契约补入项目指南与相关技能；不能提前将本方案记为已完成。

## 官方参考

- [SDL2 Android 入口与工程说明](https://wiki.libsdl.org/SDL2/README-android)：SDLActivity 加载 SDL 与游戏原生库。
- [Android OpenGL ES](https://developer.android.com/develop/ui/views/graphics/opengl/about-opengl)：GLES 能力仍取决于设备实现，不能仅凭系统版本判定。
- [Android 原生引擎与 Vulkan 兼容建议](https://developer.android.com/games/develop/vulkan/native-engine-support)：旧设备应保留 GLES 路径，不能假设 Vulkan 版本和扩展满足引擎要求。

历史定位参考为 `docs/agent-memory/project_pvz_xplat_phase1_review.md` 与 `project_pvz_xplat_phase3_manifest.md`；以上结论均按本次源码复核，不沿用历史测试结论。
