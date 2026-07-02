# 资源加载并行化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 冷启动资源加载从 ~10s 降到 ~2s：图片「打开+解码+转格式」多线程并行，Vulkan 上传保持主线程；另加七阶段计时日志作为改前/改后对比基线。

**Architecture:** `ResourceManager` 内部新增私有助手 `ParallelDecodeAndUpload(jobs)`：N 个临时 `std::thread` 用原子计数器领任务做纯解码，主线程**严格按原列表顺序**消费（去重→上传→插入→日志），与串行版语义逐位一致。前置重构把 `LoadTexture` 拆成线程安全的 `DecodeImageFile`（.cpp 匿名命名空间）与主线程的 `UploadDecodedTexture`（私有成员）。

**Tech Stack:** C++17 `<thread>/<atomic>/<mutex>/<condition_variable>`、SDL2_image、VMA/Vulkan（不动）、既有 Logger。

**Spec:** `docs/superpowers/specs/2026-07-02-parallel-resource-loading-design.md`（主人已批准）

## Global Constraints

- 本项目**无单元测试框架**：每个 Task 的验证 = clang-release 全量构建 **0 warning / 0 error** + AutoTest 脚本回归（这是项目既定验收惯例）。
- `ResourceManager` **对外接口、key 语义、日志文案零变化**；错误日志措辞必须与改前逐字一致。
- Vulkan 上传（`CreateTextureRGBA8`）、`mTextures` 读写、日志输出**只准在主线程**；worker 线程只做 `SDL_RWFromFile`/`IMG_Load_RW`/`SDL_ConvertSurfaceFormat`。
- 线程数 `clamp(int(hardware_concurrency()) - 1, 2, 8)`——**必须先转 int 再减**（无符号 0-1 回绕）。
- Task 1（计时日志）必须**单独成 commit 并保留基线 exe 副本**，供主人采集改前冷启动数据。
- AutoTest 工作目录 = `build\clang-release`；脚本在仓库根 `autotest\scripts\`（从 build 目录引用为 `..\..\autotest\scripts\...`）。计时 WARN 行输出在 **exe stdout/stderr**，不在 run.log——运行时必须 `1>` `2>` 重定向捕获。
- commit 尾部：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。

## 复用命令块

**【命令A：导入 VS 环境 + 全量构建 clang-release】**（每个 Task 的构建步骤都指这个）

```powershell
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --preset clang-release
cmake --build --preset clang-release
```

预期：`Build succeeded`，0 warning / 0 error（clang 是警告零基线的权威配置）。

**【命令B：AutoTest 回归】**（`<脚本名>` 换成实际脚本）

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\<脚本名>.json -Seed 42 1> autotest_stdout.txt 2> autotest_stderr.txt
$LASTEXITCODE   # 必须为 0
Pop-Location
```

产物在 `build\clang-release\autotest\out\<脚本名>\`（PNG 用 Read 工具直接看）；计时 WARN 行在 `autotest_stdout.txt` / `autotest_stderr.txt` 里 grep `资源加载`。

## File Structure

| 文件 | 动作 | 职责 |
|---|---|---|
| `PlantVsZombies/GameApp.cpp` | 修改 `LoadAllResources()`（:192-215） | 七阶段计时 + 一行 WARN 汇总（Task 1） |
| `PlantVsZombies/ResourceManager.h` | 修改 private 区（:60-64 附近）+ include | 新增 `TextureJob` 结构、`ParallelDecodeAndUpload`、`UploadDecodedTexture` 私有声明（Task 2/3） |
| `PlantVsZombies/ResourceManager.cpp` | 修改 :32-80（LoadTexture）、:195-237（两 LoadAll）、:312-350（FromPath）+ 顶部 include | 解码/上传拆分（Task 2）；并行助手 + 三调用点（Task 3） |

不新建文件：助手是 ResourceManager 的内部实现细节，跟随现有单文件组织。

---

## Task 1：七阶段计时日志（基线 commit）

**Files:**
- Modify: `PlantVsZombies/GameApp.cpp:192-215`（`GameAPP::LoadAllResources`）+ 顶部 include 区

**Interfaces:**
- Consumes: 现有 7 个 `resourceManager.LoadAll*()`（签名不变）
- Produces: 启动日志一行 `LOG_WARN("Startup") << "资源加载 X.Xs: 图片 ... / 动画 X.X"`——Task 4 与主人冷启动采数都 grep 这行的 `资源加载` 前缀

- [ ] **Step 1: 改写 LoadAllResources 加计时**

`GameApp.cpp` 顶部 include 区补上（若已有则跳过）：

```cpp
#include <chrono>
#include <cstdio>
```

把 `GameAPP::LoadAllResources`（:192-215）整体替换为：

```cpp
bool GameAPP::LoadAllResources()
{
	ResourceManager& resourceManager = ResourceManager::GetInstance();
	bool resourcesLoaded = true;

	using Clock = std::chrono::steady_clock;
	auto timedPhase = [](auto&& fn, double& outSec) {
		const auto t0 = Clock::now();
		const bool ok = fn();
		outSec = std::chrono::duration<double>(Clock::now() - t0).count();
		return ok;
	};

	double tImg = 0, tReanimImg = 0, tParticle = 0, tFont = 0, tSound = 0, tMusic = 0, tReanim = 0;
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllGameImages(); }, tImg);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllImagesFromPath(); }, tReanimImg);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllParticleTextures(); }, tParticle);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllFonts(); }, tFont);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllSounds(); }, tSound);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllMusic(); }, tMusic);
	resourcesLoaded &= timedPhase([&] { return resourceManager.LoadAllReanimations(); }, tReanim);

	// Release 编译期裁掉 INFO 以下，这行是采集玩家冷启动耗时的唯一通道，故用 WARN。
	const double total = tImg + tReanimImg + tParticle + tFont + tSound + tMusic + tReanim;
	char summary[256];
	std::snprintf(summary, sizeof(summary),
		"资源加载 %.1fs: 图片 %.1f / reanim图 %.1f / 粒子 %.1f / 字体 %.1f / 音效 %.1f / 音乐 %.1f / 动画 %.1f",
		total, tImg, tReanimImg, tParticle, tFont, tSound, tMusic, tReanim);
	LOG_WARN("Startup") << summary;

	if (!resourcesLoaded)
	{
		LOG_ERROR("GameApp") << "资源加载失败！";
		return false;
	}

	// 所有 reanim 与其部件纹理就绪后，构建图集页（消除批渲染的 32 纹理单元抖动）
	resourceManager.BuildReanimAtlases();

	return true;
}
```

注意：`resourcesLoaded &=` 的七连调用顺序、失败分支、`BuildReanimAtlases()` 调用与原文完全一致，只是每个调用被 `timedPhase` 包了一层。

- [ ] **Step 2: 构建**

跑【命令A】。预期 0 warning / 0 error。

- [ ] **Step 3: AutoTest 验证计时行出现**

跑【命令B】用 `smoke_quit`（加载全部资源后退出，最快）。预期 `$LASTEXITCODE`=0。然后：

```powershell
Select-String -Path build\clang-release\autotest_stdout.txt, build\clang-release\autotest_stderr.txt -Pattern '资源加载'
```

预期：恰好一行，格式如 `资源加载 1.8s: 图片 0.1 / reanim图 1.2 / ...`（热启动数值仅供格式确认）。

- [ ] **Step 4: 留存基线 exe**

```powershell
Copy-Item build\clang-release\PlantsVsZombies.exe build\clang-release\PlantsVsZombies_baseline.exe
```

（资源在 exe 旁边按相对路径读，同目录副本可直接双击运行；主人之后用它采改前冷启动数据。）

- [ ] **Step 5: Commit**

```powershell
git add PlantVsZombies/GameApp.cpp
git commit -m @'
perf(startup): 资源加载七阶段计时+WARN汇总行（冷启动基线采集通道）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

## Task 2：LoadTexture 拆分为 解码/上传 两半（行为不变重构）

**Files:**
- Modify: `PlantVsZombies/ResourceManager.h`（private 区，`LoadTiledTextureGL` 声明 :64 附近）
- Modify: `PlantVsZombies/ResourceManager.cpp:32-80`（`LoadTexture`）+ 文件顶部

**Interfaces:**
- Produces（Task 3 依赖，签名必须一致）:
  - `.cpp` 匿名命名空间：`struct DecodedImage { SDL_Surface* surface = nullptr; std::string error; };` 与 `DecodedImage DecodeImageFile(const std::string& filepath)` —— **线程安全**（不碰成员、不打日志，SDL error 是 per-thread TLS）
  - 私有成员：`const Texture* ResourceManager::UploadDecodedTexture(SDL_Surface* converted, const std::string& key, const std::string& filepath)` —— 主线程专用，接管 `converted` 所有权（内部 free）

- [ ] **Step 1: .cpp 顶部加匿名命名空间助手**

在 `ResourceManager.cpp` 的 `ResourceManager* ResourceManager::instance = nullptr;`（:11）之后插入：

```cpp
namespace {
	// worker 线程可安全调用：只做 打开→解码→转 ABGR8888，不碰共享状态、不打日志。
	// 失败时 surface==nullptr，error 携带与旧 LoadTexture 逐字一致的日志文案，由主线程统一输出。
	struct DecodedImage {
		SDL_Surface* surface = nullptr;
		std::string error;
	};

	DecodedImage DecodeImageFile(const std::string& filepath) {
		DecodedImage out;
		SDL_RWops* rw = SDL_RWFromFile(filepath.c_str(), "rb");
		if (!rw) {
			out.error = "LoadTexture 无法打开图片: " + filepath;
			return out;
		}
		SDL_Surface* surface = IMG_Load_RW(rw, 1);   // freesrc=1
		if (!surface) {
			out.error = "LoadTexture 无法加载图片: " + filepath + " - " + IMG_GetError();
			return out;
		}
		// 统一转换为 RGBA，解决对齐与源格式不确定问题
		SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
		SDL_FreeSurface(surface);
		if (!converted) {
			out.error = "LoadTexture 无法转换图片格式: " + filepath;
			return out;
		}
		out.surface = converted;
		return out;
	}
}
```

- [ ] **Step 2: 头文件声明 UploadDecodedTexture**

`ResourceManager.h` private 区，`LoadTiledTextureGL` 声明的下一行加：

```cpp
	// 把已解码的 ABGR8888 surface 上传 Vulkan 并插入 mTextures（接管 converted 所有权）。
	// 仅主线程调用；上传失败仍按原语义插入空 Texture 并返回其指针。
	const Texture* UploadDecodedTexture(SDL_Surface* converted, const std::string& key, const std::string& filepath);
```

- [ ] **Step 3: 重写 LoadTexture + 新增 UploadDecodedTexture**

把 `ResourceManager.cpp` 的 `LoadTexture`（原 :32-80）整体替换为：

```cpp
const Texture* ResourceManager::LoadTexture(const std::string& filepath, const std::string& key) {
	std::string actualKey = key.empty() ? filepath : key;
	auto it = mTextures.find(actualKey);
	if (it != mTextures.end()) {
		return &it->second;
	}

	DecodedImage decoded = DecodeImageFile(filepath);
	if (!decoded.surface) {
		LOG_ERROR("ResourceManager") << decoded.error;
		return nullptr;
	}
	return UploadDecodedTexture(decoded.surface, actualKey, filepath);
}

const Texture* ResourceManager::UploadDecodedTexture(SDL_Surface* converted, const std::string& key, const std::string& filepath) {
	Texture tex;
	tex.width = converted->w;
	tex.height = converted->h;

	if (mTexturePool) {
		pvz::VulkanTexture* vkt = mTexturePool->CreateTextureRGBA8(converted->w, converted->h, converted->pixels);
		if (vkt) {
			tex.vkTex = vkt;
			tex.id = vkt->bindlessIndex;
		}
		else {
			LOG_ERROR("ResourceManager") << "LoadTexture VulkanTexturePool 上传失败: " << filepath;
		}
	}
	SDL_FreeSurface(converted);

	mTextures[key] = tex;
	return &mTextures[key];
}
```

行为对照检查点（reviewer 逐条核）：①去重先行返回既有条目 ②三种解码失败的 ERROR 文案逐字一致 ③上传失败仍插入空 Texture 且返回非空（调用方视为成功——这是原语义，保留）④surface 全路径无泄漏。

- [ ] **Step 4: 构建**

跑【命令A】。预期 0 warning / 0 error。

- [ ] **Step 5: AutoTest 回归**

跑【命令B】用 `demo_peashooter`。预期 `$LASTEXITCODE`=0；Read 输出目录截图与改前一致（豌豆射手/僵尸/UI 正常渲染）；`autotest_stdout.txt`/`autotest_stderr.txt` 无新增资源加载 ERROR。

- [ ] **Step 6: Commit**

```powershell
git add PlantVsZombies/ResourceManager.h PlantVsZombies/ResourceManager.cpp
git commit -m @'
refactor(resource): LoadTexture 拆分为线程安全解码+主线程上传两半（行为不变，为并行铺路）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

## Task 3：ParallelDecodeAndUpload 助手 + 三个调用点改造

**Files:**
- Modify: `PlantVsZombies/ResourceManager.h`（include 区 + private 区）
- Modify: `PlantVsZombies/ResourceManager.cpp`（顶部 include；新增助手实现；改写 `LoadAllGameImages` :195-216、`LoadAllParticleTextures` :218-237、`LoadAllImagesFromPath` :312-350）

**Interfaces:**
- Consumes: Task 2 的 `DecodeImageFile` / `UploadDecodedTexture`（签名见 Task 2 Produces）
- Produces: 私有 `struct TextureJob { std::string path; std::string key; std::string failMsg; };` 与 `size_t ParallelDecodeAndUpload(const std::vector<TextureJob>& jobs);`（返回成功条目数，key 已存在跳过也算成功）

- [ ] **Step 1: 头文件加声明**

`ResourceManager.h` include 区补 `#include <vector>`（现在靠传递包含，显式化）。private 区 `UploadDecodedTexture` 声明之后加：

```cpp
	// 并行图片加载：worker 做 打开+解码+转格式，主线程严格按 jobs 原顺序做 去重/上传/插入/日志，
	// 与逐个调 LoadTexture 的串行语义逐位一致。failMsg 非空时该条目失败会额外记一行 ERROR。
	// 返回成功条目数（含"key 已存在跳过"的条目）。
	struct TextureJob {
		std::string path;
		std::string key;
		std::string failMsg;
	};
	size_t ParallelDecodeAndUpload(const std::vector<TextureJob>& jobs);
```

- [ ] **Step 2: .cpp 加实现**

`ResourceManager.cpp` 顶部 include 区补：

```cpp
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
```

（`<algorithm>` 已有。）在 `UploadDecodedTexture` 实现之后插入：

```cpp
size_t ResourceManager::ParallelDecodeAndUpload(const std::vector<TextureJob>& jobs) {
	if (jobs.empty()) return 0;

	const size_t n = jobs.size();
	std::vector<DecodedImage> results(n);
	std::vector<uint8_t> done(n, 0);
	std::mutex doneMutex;
	std::condition_variable doneCv;
	std::atomic<size_t> nextJob{ 0 };

	// hardware_concurrency 可能返回 0 且类型无符号，必须先转 int 再减 1，防回绕
	const int hc = static_cast<int>(std::thread::hardware_concurrency());
	const size_t threadCount = static_cast<size_t>(std::clamp(hc - 1, 2, 8));

	auto workerFn = [&]() {
		for (;;) {
			const size_t i = nextJob.fetch_add(1, std::memory_order_relaxed);
			if (i >= n) break;
			DecodedImage decoded = DecodeImageFile(jobs[i].path);
			{
				std::lock_guard<std::mutex> lock(doneMutex);
				results[i] = decoded;
				done[i] = 1;
			}
			doneCv.notify_all();
		}
	};

	std::vector<std::thread> workers;
	const size_t spawn = std::min(threadCount, n);
	workers.reserve(spawn);
	for (size_t t = 0; t < spawn; ++t) {
		workers.emplace_back(workerFn);
	}

	// 主线程严格按原列表顺序消费：key"先到先得"覆盖语义、日志顺序与串行版一致；
	// 等第 i 槽期间 worker 已在解码后续项（流水线重叠），无"全解码完再上传"的峰值内存。
	size_t successCount = 0;
	for (size_t i = 0; i < n; ++i) {
		DecodedImage decoded;
		{
			std::unique_lock<std::mutex> lock(doneMutex);
			doneCv.wait(lock, [&] { return done[i] != 0; });
			decoded = results[i];
			results[i].surface = nullptr;
		}

		// 与串行 LoadTexture 相同的去重语义：已存在则跳过（仍算成功）
		if (mTextures.find(jobs[i].key) != mTextures.end()) {
			if (decoded.surface) SDL_FreeSurface(decoded.surface);
			++successCount;
			continue;
		}

		if (!decoded.surface) {
			LOG_ERROR("ResourceManager") << decoded.error;
			if (!jobs[i].failMsg.empty()) {
				LOG_ERROR("ResourceManager") << jobs[i].failMsg;
			}
			continue;
		}

		UploadDecodedTexture(decoded.surface, jobs[i].key, jobs[i].path);
		++successCount;
	}

	for (auto& w : workers) w.join();
	return successCount;
}
```

- [ ] **Step 3: 改写三个调用点**

`LoadAllGameImages`（原 :195-216）替换为：

```cpp
bool ResourceManager::LoadAllGameImages() {
	bool success = true;
	const auto& infos = GetGameImageInfos();
	std::vector<TextureJob> jobs;
	jobs.reserve(infos.size());
	for (const auto& info : infos) {
		if (info.columns <= 1 && info.rows <= 1) {
			// 普通图片：收集为并行任务（与分割贴图的 key 不相交，先后顺序互不影响）
			jobs.push_back({ info.path, GenerateStandardKey(info.path, "IMAGE_"),
			                 "加载游戏图片失败: " + info.path });
		}
		else {
			// 分割贴图（个位数）：保持串行
			if (!LoadTiledTextureGL(info, "IMAGE_")) {
				LOG_ERROR("ResourceManager") << "加载分割贴图失败: " << info.path;
				success = false;
			}
		}
	}
	if (ParallelDecodeAndUpload(jobs) != jobs.size()) {
		success = false;
	}
	return success;
}
```

`LoadAllParticleTextures`（原 :218-237）替换为：

```cpp
bool ResourceManager::LoadAllParticleTextures() {
	bool success = true;
	const auto& infos = GetParticleTextureInfos();
	std::vector<TextureJob> jobs;
	jobs.reserve(infos.size());
	for (const auto& info : infos) {
		if (info.columns <= 1 && info.rows <= 1) {
			jobs.push_back({ info.path, GenerateStandardKey(info.path, "PARTICLE_"),
			                 "加载粒子纹理失败: " + info.path });
		}
		else {
			if (!LoadTiledTextureGL(info, "PARTICLE_")) {
				LOG_ERROR("ResourceManager") << "加载粒子分割贴图失败: " << info.path;
				success = false;
			}
		}
	}
	if (ParallelDecodeAndUpload(jobs) != jobs.size()) {
		success = false;
	}
	return success;
}
```

`LoadAllImagesFromPath`（原 :312-350）替换为（保留原注释块）：

```cpp
bool ResourceManager::LoadAllImagesFromPath(const std::string& directory) {
	// 文件名来自构建期烘焙的清单（FileManager::ListResourceFiles 经 SDL_RWops 读，APK 可读），
	// 不再用 std::filesystem 枚举，使该路径在 Android 上也能列举 APK assets。
	// 返回的全路径与旧 directory_iterator 逐字一致，下游加载零差异。
	std::vector<std::string> entries = FileManager::ListResourceFiles(directory);

	std::vector<TextureJob> jobs;
	jobs.reserve(entries.size());
	for (const std::string& fullPath : entries) {
		std::string ext = FileManager::GetFileExtension(fullPath);
		// 转换为小写以统一判断
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
			// stem = 文件名去扩展名（ext 已小写但长度不变，故可直接按长度裁剪）
			std::string fileName = FileManager::GetFileName(fullPath);
			std::string key = fileName.substr(0, fileName.size() - ext.size());
			// 将 key 转换为大写
			std::transform(key.begin(), key.end(), key.begin(), ::toupper);
			jobs.push_back({ fullPath, "IMAGE_" + key, std::string() });
		}
	}

	const size_t loadedCount = ParallelDecodeAndUpload(jobs);

	if (loadedCount == 0) {
		LOG_ERROR("ResourceManager") << "在目录 " << directory << " 中没有找到任何 JPG/PNG 图片";
		return false;
	}

	return loadedCount == jobs.size();
}
```

语义对照（reviewer 核）：原版逐文件 `LoadTexture` 失败→`allSuccess=false` 不记额外日志（failMsg 传空串复现）；`loadedCount==0` 的 ERROR 与返回 false 保留；成功返回值 = 全部成功，与原 `allSuccess` 等价。

- [ ] **Step 4: 构建**

跑【命令A】。预期 0 warning / 0 error（特别留意 clang 的 `-Wunused-*`：Task 2 引入、本 Task 才使用的符号此时应全部被引用）。

- [ ] **Step 5: AutoTest 回归（两脚本）**

跑【命令B】依次用 `demo_peashooter` 与 `smoke_small_sun`（夜间关卡，覆盖另一批资源）。预期各自 `$LASTEXITCODE`=0；Read 两组截图与改前一致；stdout/stderr 无新增资源加载 ERROR；grep `资源加载` 计时行仍恰好一行。

- [ ] **Step 6: 热启动耗时对比（初步收益确认）**

```powershell
Push-Location build\clang-release
1..3 | ForEach-Object {
  .\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json 1> warm_new.txt 2> warm_new_err.txt
  Select-String -Path warm_new.txt, warm_new_err.txt -Pattern '资源加载'
}
1..3 | ForEach-Object {
  .\PlantsVsZombies_baseline.exe -AutoTest ..\..\autotest\scripts\smoke_quit.json 1> warm_base.txt 2> warm_base_err.txt
  Select-String -Path warm_base.txt, warm_base_err.txt -Pattern '资源加载'
}
Pop-Location
```

预期：新版热启动「图片+reanim图」耗时 ≤ 基线（解码并行的顺带收益）；若新版反而更慢，停下诊断再提交。

- [ ] **Step 7: Commit**

```powershell
git add PlantVsZombies/ResourceManager.h PlantVsZombies/ResourceManager.cpp
git commit -m @'
perf(startup): 图片读取+解码多线程并行（主线程保序消费+上传），治冷启动小文件风暴

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

## Task 4：验证收口 + 主人冷启动采数交接

**Files:** 无代码改动（验证与交接）

**Interfaces:**
- Consumes: Task 1 的基线 exe（`build\clang-release\PlantsVsZombies_baseline.exe`）、计时 WARN 行

- [ ] **Step 1: msvc-debug 也过一遍构建**

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

预期 0 error（debug 配置可编译，F5 调试不被破坏）。

- [ ] **Step 2: 残留检查**

```powershell
git status --short   # 应干净（除 build 产物）
```

Grep 确认 `ResourceManager.cpp` 中旧串行循环无残留死代码、`GameApp.cpp` 无临时调试输出。

- [ ] **Step 3: 向主人交接冷启动采数流程**

向主人报告并请求执行（两次重启，各一次）：
1. **改前基线**：重启电脑 → 双击 `build\clang-release\PlantsVsZombies_baseline.exe` → 进主菜单后退出 → 把控制台/日志里 `资源加载 ...` 那行发我。
2. **改后**：再重启 → 双击 `build\clang-release\PlantsVsZombies.exe` → 同样把那行发我。

判定：图片阶段（图片+reanim图 合计）应从 ~9s 量级降到 ~2s 量级。**若冷启动无明显改善，按 spec 回到方案 B（资源打包）重新评估**——不要在方案 A 上继续加码。

- [ ] **Step 4: 收尾**

拿到主人数据且达标后：删除 `PlantsVsZombies_baseline.exe` 副本；本计划完成，走 superpowers:finishing-a-development-branch（若在 worktree/分支上）或直接留在 master 等主人指示 push。
