# 延迟纹理删除（fence-safe deferred deletion）设计

日期：2026-06-16
分支：`feature/deferred-texture-deletion`

## 背景与动机

`VulkanTexturePool::DestroyTexture` 当前用 `vkDeviceWaitIdle`（整 GPU 停顿）来保证销毁一张
可能仍被 in-flight 帧引用的纹理是安全的。该调用在 LRU 文字缓存淘汰路径（`Graphics.cpp` 中
`GetOrCreateTextTexture`，缓存上限 `TEXT_CACHE_MAX_SIZE=1024`）里被命中。

2026-06-16 实测（见 [[project_pvz_gemini_vulkan_review]]）：现实场景缓存永不到 1024 条，该
`vkDeviceWaitIdle` **从未触发**，是休眠的。但**一旦把缓存上限调小**（例如为省显存降到
128/256），它会在每帧文字churn 时变成每帧整 GPU 停顿——埋雷。本设计提前拆雷：把每帧路径的
同步销毁改成基于帧计数的延迟回收队列，消除 `vkDeviceWaitIdle`。

## 安全性不变量

`VulkanRenderer::BeginFrame` 在复用某 slot 前会 `vkWaitForFences` 等该 slot 上一次提交的
fence；slot 每 `FRAMES_IN_FLIGHT`(=2) 帧轮回一次。所以 tick T 提交的帧，到 tick T+2 的
BeginFrame 时其 GPU 工作必然已 retired。延迟回收正是利用这个**已有**不变量，用"等 2 帧再 free"
替代"vkDeviceWaitIdle 停整个 GPU"。

## 设计

### 数据
`VulkanTexturePool` 新增：
- `uint64_t mFrameTick = 0;`（单调帧计数）
- `struct PendingDeletion { std::unique_ptr<VulkanTexture> tex; uint64_t tick; };`
- `std::vector<PendingDeletion> mPendingDeletions;`

帧间隔常量引用 `VulkanRenderer::FRAMES_IN_FLIGHT`（pool .cpp include `VulkanRenderer.h`），
保持单一真相源。

### DestroyTexture（改为入队，不再 wait idle）
1. 同样的合法性校验（idx 越界 / `mTextures[idx].get()!=tex` → early return）。
2. **不** `vkDeviceWaitIdle`、**不** 立即销毁、**不** 立即 `FreeBindlessIndex`。
3. `mPendingDeletions.push_back({ std::move(mTextures[idx]), mFrameTick });`
   —— `mTextures[idx]` 即刻置空（逻辑上已销毁，查不到），GPU 句柄 + bindlessIndex 随
   unique_ptr 存活在队列里。bindless 槽位**暂不**归还，避免新纹理 `WriteDescriptor` 覆盖
   仍被 in-flight 帧采样的 descriptor。

### BeginFrameTick（每帧一次，Graphics::BeginFrame 调）
1. `++mFrameTick;`
2. 遍历 `mPendingDeletions`，对 `mFrameTick - tick >= FRAMES_IN_FLIGHT` 的项：
   - `vkDestroyImageView` + `vmaDestroyImage`
   - `FreeBindlessIndex(tex->bindlessIndex)`（**此时**才归还槽位）
   - 从队列移除（swap-and-pop 或 erase-remove）。

调用点：`Graphics::BeginFrame` 中 `m_vk->renderer->BeginFrame(...)` 成功**之后**
（此刻该 slot 的 fence 已被等过，回收判定最稳）。

### Shutdown（teardown 仍同步）
`vkDeviceWaitIdle` → 销毁 `mPendingDeletions` 全部（无视帧龄）→ 再销毁 `mTextures` 剩余项 →
销毁 sampler/layout/pool/fence 等（原逻辑不变）。保证程序退出前零泄漏。

## 行为影响
- 每帧路径（LRU 淘汰）：消除 `vkDeviceWaitIdle`，无停顿。
- 场景切换（`ClearTextCache`/`ClearPinnedTextCache` → 逐条 DestroyTexture）：纹理多留 ~2 帧后
  回收；4096 个 bindless 槽位足够，`m_textGeneration` 代际号已使旧句柄失效，无可见影响。
- VRAM：被删纹理多占用约 2 帧，可忽略。
- 公开行为/截图：不变。

## 验证
1. clang-release 构建 exit 0。
2. 跑切场景的 smoke 脚本（`smoke_gameplay` / `smoke_screenshot` 等），自然驱动
   ClearTextCache → 延迟队列，确认无崩溃、无 Vulkan validation error、截图正确。
3. （可选，强校验）临时把 `TEXT_CACHE_MAX_SIZE` 调小并 churn 文字逼出 LRU 淘汰，确认延迟队列
   在每帧淘汰下无 use-after-free、画面正确；验证后还原。

## YAGNI / 非目标
- 不做 fence-based 精确回收（多一层 renderer↔pool 耦合，帧计数法在固定 FIF=2 下已足够安全）。
- 不动 teardown 同步语义。
- 不碰加载期 sprite 同步上传（另一回事，非本次范围）。
