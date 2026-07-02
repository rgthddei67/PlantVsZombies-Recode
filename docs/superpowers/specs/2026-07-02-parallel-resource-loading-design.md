# 资源加载并行化设计（冷启动 10s → 目标 ~2s）

日期：2026-07-02
状态：主人已批准设计，待实现

## 背景与诊断

主人反馈：开机后**第一次**打开游戏卡约 10 秒；关掉马上再开则明显快很多。

诊断结论（已与主人确认热/冷差异）：

- 资源总量极小：全部 ~36MB（PNG 2490 个合计仅 10.3MB，平均 ~4KB/张；ogg 61 个 17.2MB；reanim 21 个）。10 秒不可能是带宽问题。
- 真正开销是 **~2600 次「打开文件」的固定成本**：冷缓存下每次 open 要走 NTFS 元数据寻址 + Windows Defender 实时扫描（每个文件首次被读都拦截扫一遍）。10s ÷ 2600 ≈ 4ms/文件，是典型「小文件风暴」指纹。第二次快是 OS 文件缓存 + Defender 扫描结果缓存都热了。
- 因此收益主体在**并行「打开+读取」**（把 Defender 扫描与磁盘排队 N 路摊薄），并行解码是顺带收益（热启动也受益）。

现状加载链路（全串行）：`GameApp::LoadAllResources`（GameApp.cpp:192）依次跑 7 个 LoadAll*；每张图走 `SDL_RWFromFile → IMG_Load_RW → SDL_ConvertSurfaceFormat(ABGR8888) → VulkanTexturePool::CreateTextureRGBA8 → mTextures[key]=tex`（ResourceManager.cpp:32-80）。

备选方案裁决：B（资源打包单一 pak）冷启动效果最彻底但侵入构建/读取层大，留作 A 之后仍不满意的后手；C（异步加载画面）不提速只改观感，不做。主人选 A。

## 范围 / 非目标

**做：**
1. 图片「读文件+解码+格式转换」多线程并行，Vulkan 上传保持主线程（方案 A）。
2. `LoadAllResources` 7 阶段计时 + 一行 WARN 汇总日志（冷启动数据的采集通道 + 改前后对比基线）。

**不做（YAGNI）：**
- 声音(61)/音乐/字体/reanim(21)/分割贴图(个位数)的并行化——文件数少，冷启动占比 <5%。
- 资源打包（方案 B）、加载画面（方案 C）。
- `ResourceManager` 对外接口、key 语义、资源格式的任何变化。

## 组件 1 — 并行图片解码助手

`ResourceManager` 内部新增**私有**助手（对外接口零变化）：

```
bool ResourceManager::ParallelDecodeAndUpload(const std::vector<TextureJob>& jobs);
// TextureJob { std::string path; std::string key; }
```

### 并行边界

**worker 线程做**（每张图独立、无共享可变状态）：
1. `SDL_RWFromFile` 打开+读取 —— 冷启动大头，Defender 扫描/磁盘等待被 N 路摊薄
2. `IMG_Load_RW` 解码 —— libpng/libjpeg-turbo 每调用独立实例，线程安全；`IMG_Init(PNG|JPG)` 已在 GameApp.cpp:67-68 早于加载完成
3. `SDL_ConvertSurfaceFormat` → `SDL_PIXELFORMAT_ABGR8888` —— 纯内存操作

**主线程做**（全部非线程安全操作，数据结构零加锁）：
- 去重检查（`mTextures.find`，保留现有「已存在则跳过」语义）
- `CreateTextureRGBA8` Vulkan 上传 —— 已侦察确认内部走 `AllocBindlessIndex` + `vmaCreateImage` + 命令缓冲（VulkanTexturePool.cpp:242-），非线程安全
- `mTextures[key] = tex` 插入
- 失败时打与现有串行版**措辞一致**的 ERROR 日志

### 数据流与保序

1. 主线程把 jobs 放进共享数组，起 `N = clamp(std::thread::hardware_concurrency() - 1, 2, 8)` 个临时 `std::thread`；worker 用**原子计数器**领任务（fetch_add 取下标，无任务队列锁）。注意 `hardware_concurrency()` 可能返回 0 且类型无符号，先转 `int` 再减 1 再 clamp，防回绕。
2. worker 把结果写入 `results[i]`（成功 = 转换好的 `SDL_Surface*`；失败 = 错误信息字符串），置 per-slot 完成标志（condition_variable 通知）。
3. 主线程**严格按原列表顺序** i=0..n-1 等待各槽完成，依次消费：去重 → 上传 → 插入 → 失败记日志。
4. 全部消费完 join 所有线程，返回与串行版语义相同的聚合 success。

设计不变量：
- **按原顺序消费** ⇒ key 冲突「先到先得」的覆盖/去重语义、错误日志出现顺序、返回值聚合，与现串行版逐位一致；回归只需比截图与日志。
- **worker 不打日志** ⇒ 失败信息随 result 带回主线程再打，绕开 Logger 线程安全审计，且日志顺序确定。
- **流水线重叠** ⇒ 主线程等第 i 槽时 worker 已在解码后续项；不做「全解码完再统一上传」，避免峰值同时持有 2490 张 surface。
- 主线程消费完一槽即 `SDL_FreeSurface`；若中途退出（不应发生），join 前统一释放未消费槽，防泄漏。

### 覆盖的调用点（三处，共用同一助手）

| 调用点 | 改造 | 规模 |
|---|---|---|
| `LoadAllGameImages`（ResourceManager.cpp:195） | 非分割条目（columns<=1 && rows<=1）收集为 jobs 走助手；分割贴图条目保持串行 `LoadTiledTextureGL` | ~38 张 |
| `LoadAllImagesFromPath`（ResourceManager.cpp:312） | 清单过滤出 jpg/jpeg/png 后整批收集为 jobs（key 派生逻辑不变，在主线程算好放进 job） | 2490 张，主力 |
| `LoadAllParticleTextures`（ResourceManager.cpp:218） | 同 LoadAllGameImages | 少量 |

每个调用点内部：先收集本阶段全部 jobs，一次调用助手（一批线程起停一次）；分割贴图等特殊条目在批前/批后按原顺序串行处理即可（它们与普通条目之间无 key 依赖）。

单条目失败的语义与现状一致：记 ERROR、该阶段返回 false，但**继续加载其余条目**、程序照常尝试后续阶段。

## 组件 2 — 阶段计时日志

`GameApp::LoadAllResources` 给 7 个阶段各套计时（`std::chrono::steady_clock`），全部结束后汇总**一行**：

```
LOG_WARN("Startup") << "资源加载 9.8s: 图片 8.9 / reanim图 X / 粒子 0.3 / 字体 X / 音效 0.4 / 音乐 X / 动画 0.2";
```

- 用 WARN 是因为 Release 编译期裁掉 INFO 以下，而主人玩 clang-release——这一行是采集冷启动真实数据的唯一通道。每次启动固定一行，不构成噪音；主人若不喜欢可后续降为超阈值才打。
- 该行在**改前基线版**与**改后版**各采一次冷启动数据（见验收）。
- **实现顺序约束**：计时日志必须**先于并行化单独成 commit**——先出基线二进制给主人采冷启动数据，再落并行化；否则「改前」数字无从谈起。

## 风险

- **机械硬盘玩家**并行随机读可能不升反降：线程数上限 8 保守可控；2490 个小文件在 NTFS 上大概率物理相邻，实际劣化空间有限。不为此加配置项（YAGNI），若真有玩家反馈再说。
- **SDL_image 线程安全**：仅并行纯解码调用；`IMG_Init` 早已完成；不在 worker 碰任何 SDL 全局状态（窗口/渲染器/事件）。
- **异常/提前返回路径**：助手内 join 必达（RAII 或作用域收尾），不留游离线程。

## 验收标准

1. **构建**：clang-release 全量 0 warning / 0 error。
2. **正确性回归**：AutoTest 跑 `demo_peashooter.json` + 一个夜间脚本（如 `smoke_small_sun`），`$LASTEXITCODE`=0，截图与改前一致，run.log 无新增资源加载 ERROR。
3. **热启动耗时**：改前/改后各跑数次，读 WARN 汇总行对比（Claude 可自测）。
4. **冷启动耗时**（关键数字，需主人配合）：改前基线版重启电脑跑一次记录汇总行；改后版再重启跑一次。预期图片阶段从 ~9s 量级降到 ~2s 量级。若冷启动无明显改善，回到方案 B（资源打包）重新评估。
