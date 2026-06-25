# 设计：资源清单（manifest）取代运行时目录枚举（跨平台 phase-3）

- 日期：2026-06-25
- 状态：已与主人确认，待落地
- 目标平台（未来）：Android、Linux（桌面）；当前唯一构建/验证平台：x64 Windows

## 背景与动机

跨平台 phase-1（已完成，tip `b3ff1da`）把**所有只读资源**的加载收编到 `SDL_RWFromFile`（经 `FileManager::LoadFileAsBinary`），桌面相对 CWD、Android 自动读 APK assets。phase-1 审查留下唯一未收口的缺口：**两处运行时目录枚举**仍走 `std::filesystem`——它在 Android 上读不了 APK assets（AAssetManager 不提供子目录列举）：

1. `ResourceManager::LoadAllImagesFromPath`（`ResourceManager.cpp:323`）——`fs::directory_iterator` 遍历 `./resources/image/reanim/`，对每个 jpg/jpeg/png 派生 `IMAGE_<大写名>` key 后 `LoadTexture`。
2. `ParticleXMLLoader::LoadFromDirectory`（`ParticleXMLLoader.cpp:10`）——经 `FileManager::GetFilesInDirectory(dir,".xml")` 遍历 `./resources/particles/config`，逐个 `LoadFromFile`。

### 为什么不迁 SDL3

调研结论（2026-06-25）：SDL3 的 `SDL_EnumerateDirectory` 对 APK assets 的枚举（`assets://` 前缀）**只在未发布的 `main` 分支（将来的 3.6.0）**，最新稳定版 3.4.10 与 vcpkg 在库版本（3.4.6）都没有。为一个尚未进任何稳定版的功能迁移整套 SDL（事件结构体、Vulkan 表面、surface 格式、音频/字体签名全是破坏性变更）不划算。主人定调：**目标是"跨平台能读资源"，本质是个清单问题，不是 SDL 版本问题**——用构建期烘焙的文件清单收口这两处枚举即可，零库迁移、现有 SDL2 稳定栈上今天就能用。

## 范围 / 非目标

### 本次范围
- 构建期生成 `resources/manifest.txt`（CMake POST_BUILD）。
- 运行期新增 `FileManager::ListResourceFiles(dir, ext)` 读清单。
- 上述两处枚举点改道至该函数。

### 本次不做
- Android Gradle/NDK 工程、把资源打进 APK assets（future phase）。Android 侧的清单需在打包时对被打入 assets 的资源重新生成——本次只保证运行时代码会去读它。
- `GetFilesInDirectory` / 其余 `std::filesystem` 工具函数（`CreateDirectory`/`IsDirectory`/`DeleteFile` 等，仅桌面存档用）不动。
- AutoTest（TestDriver）的文件 I/O 豁免（开发期桌面专用）。

### 验证手段（无 Android 环境）
桌面即 Android 读取逻辑的试金石：**桌面也走清单路径**（非 `#ifdef` 只在安卓启用），现有 AutoTest 跑通即等于验证了 Android 将走的同一段码。

## 组件 1 — 构建期生成清单（CMake）

### 1a. 新增 `cmake/gen_manifest.cmake`
独立脚本，`cmake -P` 调用，入参 `RES_DIR`（真实资源目录）/ `RES_PARENT`（其父，作 RELATIVE 基准）/ `OUT`（清单输出路径）：
- `file(GLOB_RECURSE _files RELATIVE "${RES_PARENT}" "${RES_DIR}/*")` → 得到 `resources/image/reanim/foo.png` 形态（相对 exe 目录）。
- 排除 `resources/manifest.txt` 自身（避免把清单列进清单）。
- `list(SORT)` 后逐行以 `./` 前缀写出 → `./resources/image/reanim/foo.png`，**与 `directory_iterator` 今日产出的字符串逐字一致**（保证下游加载零差异）。
- `file(WRITE "${OUT}" ...)`。

### 1b. CMakeLists POST_BUILD 挂接
在现有"拷 Shader/spv"那条 POST_BUILD 之后追加一条，对 `$<TARGET_FILE_DIR:PlantsVsZombies>/resources` 跑 `gen_manifest.cmake`，输出 `$<TARGET_FILE_DIR:PlantsVsZombies>/resources/manifest.txt`。
- **直接 glob 紧挨 exe 的真实资源**，绕开"源码树无 `resources/`、CMake 不拷资源、资源实体在 `build/<preset>/`"的现实。
- 每次构建重生成 = 零过期窗口（优于 configure 期 GLOB：后者新增资源得手动 reconfigure 才更新）。
- 前置条件：`$<TARGET_FILE_DIR>/resources` 须已存在（资源本就放在那）——与现状一致，未改变。

## 组件 2 — 运行期读清单（FileManager）

### 2a. 新增 `FileManager::ListResourceFiles(const std::string& dir, const std::string& extension = "")`
签名与 `GetFilesInDirectory` 一致（drop-in）：
- 经 `LoadFileAsString("./resources/manifest.txt")`（=`SDL_RWFromFile`，APK 可读）读清单，**首次读后静态缓存**（资源加载在启动期单线程，函数局部 `static` + C++11 线程安全初始化）。
- 解析为行集合；对查询 `dir` 去尾斜杠归一后，筛选"**父目录字符串 == 归一后的 dir**（非递归，匹配 `directory_iterator` 语义）且扩展名匹配（`extension` 空=全部；非空=大小写不敏感后缀比较）"的行。
- 返回行**逐字原样**（即 `./resources/.../foo.png`），下游加载与今日 byte-identical。

### 2b. 桌面兜底
- 清单读取失败（`LoadFileAsString` 返回 `""`）时：
  - `#if !defined(__ANDROID__)` → 回退 `std::filesystem`（复用 `GetFilesInDirectory` 的遍历逻辑），桌面永不退化。
  - `#if defined(__ANDROID__)` → 返回空 + `LOG_ERROR`（APK 内必须有清单；缺失是打包错误，应当响亮失败，语义同今日"目录不存在"）。

## 组件 3 — 两处枚举点改道

### 3a. `ResourceManager::LoadAllImagesFromPath`
- 删除 `fs::exists/is_directory` 预检与 `fs::directory_iterator` 循环；改为 `for (auto& fullPath : FileManager::ListResourceFiles(directory))`。
- **保留**其内既有的扩展名判断（`.jpg/.jpeg/.png` 小写比较）、`IMAGE_<大写 stem>` key 派生、`LoadTexture`、`loadedCount==0` 返回 false 的逻辑——一字不改，只换"文件名从哪来"。
- 故对 `ListResourceFiles` 用空 `extension`（取目录下全部文件），扩展名过滤仍由本函数自己做（`image/reanim` 里混有 `.gif`，今日即被跳过，行为须保持）。

### 3b. `ParticleXMLLoader::LoadFromDirectory`
- `fileManager.GetFilesInDirectory(directory, ".xml")` → `FileManager::ListResourceFiles(directory, ".xml")`。
- 其余（空则 `LOG_WARN` 返回 false、逐个 `LoadFromFile`）不变。

## 数据流自洽校验

| 流向 | 路径来源 | 桌面 | Android（未来） |
|---|---|---|---|
| 列资源目录 | `ListResourceFiles` | 读 `./resources/manifest.txt`（POST_BUILD 生成），缺失回退 filesystem ✓ | 读 APK 内 `resources/manifest.txt` ✓ |
| 按名读资源 | 清单行（`./resources/…`）| `SDL_RWFromFile` 相对 CWD（phase-1 已就位）✓ | `SDL_RWFromFile` → APK assets ✓ |

清单行 == `directory_iterator` 今日产出 == 加载器入参，三者逐字相等 → 桌面行为零变化。

## 错误处理

- 清单缺失：桌面回退 filesystem；Android 返回空 + `LOG_ERROR`。
- `LoadAllImagesFromPath`：`loadedCount==0` 仍 `LOG_ERROR` 返回 false（既有语义保留）。
- `ParticleXMLLoader`：列表空仍 `LOG_WARN` 返回 false（既有语义保留）。
- 单文件加载失败：各自既有 `LOG_*` + 标记失败路径不变。

## 测试 / 验证（桌面回归）

1. **构建**：`clang-release` 0 warning / 0 error；`msvc-debug` 通过。POST_BUILD 后 `build/<preset>/resources/manifest.txt` 存在且列出 reanim 图与粒子 xml。
2. **AutoTest**（工作目录 = `build/<preset>/`）：
   - `demo_peashooter.json`——reanim 图照常加载（截图比对，僵尸/植物贴图正确）。
   - 一个触发粒子的场景（如樱桃炸弹/豌豆命中）——粒子特效照常出现。
3. 退出码 0、`run.log` 无新增 ERROR，且能看到清单路径被使用（非回退）。

通过即证明：清单路径在桌面跑通（=Android 将走的同一段码），两处枚举已不再依赖 `std::filesystem` 读资源。

## 后续阶段（本设计之外，备忘）

- Android 打包：把资源打进 APK assets 时，对被打入的资源重新生成 `manifest.txt` 一并打包；运行时 `ListResourceFiles` 即从 APK 读它。
- 若将来 SDL ≥ 3.6.0 进入稳定版且本项目已上 SDL3，可选用 `SDL_EnumerateDirectory("assets://…")` 替代清单——但清单方案库无关、可长期保留，非必须替换。
