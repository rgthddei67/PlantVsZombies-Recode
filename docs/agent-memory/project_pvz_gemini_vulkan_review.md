---
name: project-pvz-gemini-vulkan-review
description: 2026-06-16 审查 Gemini 对 PvZ Vulkan 渲染代码提的 6 条建议——逐条验证+实测，结论=全部不改
metadata:
  node_type: memory
  type: project
  originSessionId: 347550e8-4501-4c40-b5bd-fbcc08577560
---

2026-06-16：主人把渲染部分代码发给 Gemini，Gemini 回了 6 条 Vulkan 改进建议。逐条对照真实代码+实测后，**结论：6 条全部不改**（4 条是 Gemini 缺上下文的误判，2 条真实但实测可忽略）。根因：Gemini 只看到"渲染部分"，看不到 VulkanContext / VulkanBuffer 内部 / 缓存容量 / 每帧预算重算。

逐条验证（文件:行 为当时位置）：
1. **DestroyTexture 的 vkDeviceWaitIdle + LRU 淘汰→延迟删除队列**：真实存在（VulkanTexturePool.cpp:312 + Graphics.cpp:1103 在文字 LRU 淘汰里调）。但 `TEXT_CACHE_MAX_SIZE=1024`（Graphics.h:787），PvZ 屏上永不到 1024 串→淘汰路径**实测从未触发**。休眠。
2. **整池 reset→单 CB reset**：非问题。每个 PerFrame 独占池且只含 1 个 CB（VulkanRenderer.cpp:73-82），两者等价。
3. **文字同步上传→异步+占位符**：症状真（UploadPixels submit+wait 同步，VulkanTexturePool.cpp:231-233），但 Gemini 的占位符药方错（每帧变的数字会闪空）；正解是 glyph atlas（已在 backlog 延迟）。
4. **OnSwapchainRecreated Fence/Semaphore 残留**：非问题。RecreateSwapchain 先 vkDeviceWaitIdle 再重建（VulkanContext.cpp:376），renderFinished 信号量销毁重建，mFrameIdx 复位——已正确。
5. **溢出 slot 加权 4~8×+快速衰减**：非问题。premise 错——切片每帧从固定 usable 预算重算（Graphics.cpp:1646-1711），不累积，无"内存膨胀"可防；现 1.5×（:1969-1971）已 1-2 帧自收敛。
6. **回读强制 HOST_COHERENT**：非问题/反而劣化。现用 HOST_ACCESS_RANDOM（VMA 保证 host-visible）+ 显式 vmaInvalidateAllocation（VulkanBuffer.cpp:43-46, VulkanRenderer.cpp:314），是更快的正确写法；强制 coherent 会把回读推离 HOST_CACHED 变慢。截图套件本来就绿。

**实测（measure-first，主人选的路）**：临时探针（PROFILE_SCOPE 包 UploadPixels submit+wait + GetOrCreateTextTexture 未命中计数/计时）+ 最坏现实场景脚本（开 mShowZombieHP + 25 铁桶屏内被 15 豌豆持续打）。clang-release 跑出：
- 文字未命中 0.02–0.13 次/帧，TEX.miss_total（TTF+转换+上传合计）**0.01–0.02 ms/帧**，单次未命中 ~0.1ms——可忽略。
- 唯一多 ms 的 upload（5.18ms/帧 avg 那一窗）是**关卡加载时 sprite 纹理批量同步上传**，非每帧文字；加载后掉到 ~0.00-0.01ms。
- 关键架构论证：动态文字只属于屏内单位，屏内内容被 9×5 网格硬上限（~45 格，~0.26ms/帧 3800FPS）；真正的重压（11000 僵尸）全在屏外、血量文字被视口剔除。所以"重 GPU + 可见动态文字"在本游戏结构上不可达。

探针/脚本是临时的，验证完已全删，工作树干净（rebuild exit 0）。

**后续(同会话)：主人选择把 ① 当"防埋雷"实现了**（虽实测休眠），**已合并+push 到 master**（merge commit 0b14016，--no-ff；feature 分支已删）。原 2 commit：spec(a9e676a) + 实现(bcd6c97)。VulkanTexturePool 改**帧计数延迟删除队列**：DestroyTexture 入队(移出可见集合+打帧号,不再 vkDeviceWaitIdle,槽位暂不归还防 descriptor 覆盖)；Graphics::BeginFrame 在 renderer->BeginFrame 等过 slot fence 后调 BeginFrameTick(帧号+1,回收已过 FRAMES_IN_FLIGHT 帧的项)；Shutdown 仍同步(vkDeviceWaitIdle+FlushAllDeletions)。安全性靠 renderer 既有"复用 slot 前 wait fence"不变量(tick T 提交→T+2 必 retired)，机制=帧计数法(主人选,非 fence 法,避免 renderer↔pool 耦合)。验证关键=临时把 TEXT_CACHE_MAX_SIZE 压到 8 逼出每帧淘汰+msvc-debug(校验层 validation=1)跑满屏铁桶血量显示→0 VUID/'image in use' 错误+截图血量正确+干净退出;clang-release+smoke_gameplay 均 exit 0。机制选型/设计在 docs/superpowers/specs/2026-06-16-deferred-texture-deletion-design.md。教训:GPU 资源生命周期改动必须开校验层+逼出目标路径验,光 smoke 不够。
关联：[project_pvz_draw_view_hp_text_cull](project_pvz_draw_view_hp_text_cull.md)（血量文字视口剔除）、[project_pvz_glyph_atlas_text_followup](project_pvz_glyph_atlas_text_followup.md)（③的正解 glyph atlas 仍是延迟候选）、[project_pvz_premultiplied_alpha](project_pvz_premultiplied_alpha.md)（UploadPixels 预乘 alpha 契约）。
