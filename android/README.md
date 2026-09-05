# Android ARM64 试玩构建

Android 9（API 28）起，arm64-v8a，横屏，OpenGL ES 3.0。当前是首次移植试玩版，编译通过不代表已经通过真机验收。Windows 仍默认使用 clang-release。

## 构建和目录

在仓库根运行 `./android/build.ps1`。脚本导入 VS host 工具环境，使用 NDK Clang 编译 Android 目标，再用 Gradle 打包带调试签名的 APK；不安装到设备、不启动游戏。

- APK：`android/app/build/outputs/apk/debug/app-debug.apk`
- 原生库与 CMake：`build/android-arm64/`
- APK 资产和 Java 桥暂存：`build/android-package/`
- 本次构建日志：`build/android-tools/`
- SDK、NDK、JDK、Gradle、Gradle 缓存及调试签名：`D:/Android/`
- `-PackageOnly` 只重新同步资产并打包；改过 C++ 后必须执行完整构建。

脚本参数 `-AndroidRoot`、`-VcpkgRoot`、`-VmaInclude` 可覆盖本机路径；存在多个 SDL 源码版本时必须用 `-SdlSource` 指定本次 vcpkg 使用的源目录。APK 的 Java SDLActivity 来自同版本 SDL 源码，不能随意从 SDL3 或其他 SDL2 版本拷入。

本次工具配置为 Microsoft JDK 17、Gradle 8.9、Android Gradle Plugin 8.7.3、SDK platform 35 / build-tools 35.0.0、NDK 27.2.12479018。SDK Manager 位于 `D:/Android/sdk/cmdline-tools/latest/bin/sdkmanager.bat`。没有安装 Android Studio 图形 IDE，也没有修改系统全局 PATH。

## 操作

- 单指点按/拖动对应左键；工具栏独占画面下方空间，不盖住战场。
- **取消**：取消手持植物、铲子、玉米炮瞄准、矿镐或关闭路灯花档位菜单；输入位置临时置于画布外，不误触三叶草卡牌。
- **右键点选**：点击后按钮显示“请点目标”，下一次触摸对应右键，随后恢复普通左键。三叶草切向为“右键点选 → 三叶草卡牌”。不使用两指取消，避免先落下的一指误种植。
- **暂停**或 Android 返回键：复用 Escape 暂停菜单。

## 平台实现

- Android 目标排除 GameMonitor，不提供虚假的 Windows 进程指标；Logger 写 logcat。
- Android 启动强制 GLES，不调用 Vulkan 初始化。由于 Graphics 尚直接持有 Vulkan 实现，首版仍编译相关共享源码及 Volk/VMA；这不是 Android Vulkan 已适配，APK 不需要 Vulkan loader 作为动态链接依赖。
- GLES 使用现有 CPU Batch、预乘 RGBA8、裁剪、泳池、褪色滤镜；共享 330 基线 shader 在编译前转换为 300 es 并明确 highp，不使用桌面 SSBO 430 快路。
- FileManager::OpenRead 规范化 Android 包内相对路径，ResourceManager 和动态 MO3 音乐共用此入口。资源、font 只从 clang-release 权威目录生成暂存内容；暂存与 APK 都是可再生构建产物，不允许手工维护资源副本。
- 切后台清除输入、暂停音频并保存玩家和进行中的战斗；恢复后丢弃墙钟欠账，保留玩家已有暂停/倍速状态。
- SDL 正常保留 Context 时沿用已有资源；如果收到 SDL_RENDER_DEVICE_RESET，当前会记录错误并退出，需要重新启动。尚未实现丢失 Context 后原地重建所有 GPU 资源。

## 自测建议与取证

先检查菜单和字体、选卡、种植、铲除、三叶草切向、暂停及继续，再检查普通关卡、泳池、动态音乐，以及切后台/锁屏恢复和重启读档。帧率、发热及功耗必须由真机测量。

`D:/Android/sdk/platform-tools/adb.exe devices` 可列出设备；明确序列号后使用 `adb -s <serial> install -r <APK>`，出现设备授权提示时在手机确认。使用 `adb -s <serial> logcat -v time` 获取日志；包名为 `org.pvz.recode`。不要清空玩家数据来代替排查升级问题。

本次按主人要求由主人自行验机，不运行游戏或 AutoTest。Android/Windows 编译、GLES shader 离线检查和 APK 静态检查结果记录于交付消息；它们不证明运行期资源加载或触屏验收通过。

2026-09-05 交付检查：最终 Android native 与 Gradle 打包通过；Windows clang-release 增量编译及 378 项 Win7 导入检查通过；4 组 GLES 300 shader 离线编译/链接通过；APK v2 签名有效、三份原生库均为 AArch64、3550 项资源清单和 707 条注册/字体路径按大小写精确核对通过，包含第三方许可证。libmain 的动态依赖不含 libvulkan，符号中有 SDL_main、无 GameMonitor。最终构建日志无编译器 warning/error；没有运行期证据。

相关技能/reference 已审计：现有资源权威目录、manifest/注册/键闭环和截图验收要求保持有效，Android 暂存仅属构建产物，不改变地图或角色制作契约；本次未修改技能，无需 quick_validate。

## Vulkan 后续检查项

哥哥提到的信号可能是 `VK_SUBOPTIMAL_KHR`，目前不能确认。它表示呈现仍可进行，不应被简单理解成必须立即重建整个屏幕。Android 预旋转、surface 的 currentTransform、swapchain 的 preTransform、实际 extent 与同步生命周期需要一起检查，避免按未改变的参数每帧重建；也不能把它作为所有 Android 版本唯一的旋转通知。

现有 VulkanRenderer 在 acquire/present 返回 SUBOPTIMAL 后安排重建，VulkanContext 已设置 `preTransform = caps.currentTransform`。本次没有修改电脑版策略，Android 启动不会执行此分支。未来启用 Android Vulkan 前必须做实际旋转/后台/Surface 重建验证。

参考：[Android Vulkan 预旋转](https://developer.android.com/games/optimize/vulkan-prerotation)、[SDL2 Android 生命周期](https://wiki.libsdl.org/SDL2/README-android)。
