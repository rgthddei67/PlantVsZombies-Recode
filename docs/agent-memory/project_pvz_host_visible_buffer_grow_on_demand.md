---
name: project_pvz_host_visible_buffer_grow_on_demand
description: PvZ 启动占 ~890MB 内存的根因与修复——host-visible 逐帧渲染缓冲改 grow-on-demand
metadata:
  node_type: memory
  type: project
  originSessionId: d3c895c2-8e52-42ad-b661-168a09408a6e
---

2026-06-26 commit 17c3a1d(master,**未push**)：修复"游戏一启动啥也没干就占 ~800-890MB 内存"。

**根因(measure-first,第一假设被证伪)**：不是纹理！全部 2527 张图解码成 RGBA32 仅 **54MB**(PvZ 素材分辨率低,最大背景图才 1280×720=3.5MB)。真凶=`Graphics.cpp` 三个**持久映射 host-visible 缓冲**启动即按 11000 僵尸压测规模固定分配:VBO 128MB+SSBO 32MB+Inst 64MB,×FRAMES_IN_FLIGHT(2)=**448MB**,主菜单也全额常驻(`VulkanBuffer` hostVisible=MAPPED_BIT→系统内存 committed)。实测基线 WorkingSet 891MB/Private 934MB。

**修复=grow-on-demand(无上限,主人拍板)**：启动只分 (16+4+8)MB×2帧=56MB(`*_BYTES_INIT`);某帧写入溢出(`fr.overflowWarned`,serial/worker 共用)→`EndFrame` 把全局 `desired*Cap` 翻倍(去了 `*_MAX` 封顶)→下次该帧 `BeginFrame` 增长到位。实测普通关卡 891→476MB(**-415MB/-47%**)。

**关键约束/教训**：
- **唯一安全增长点=`Graphics::BeginFrame` 内**:此处 `renderer->BeginFrame` 刚等过本帧 fence,GPU 不再引用本帧缓冲→可安全 Destroy+重建。游标每帧归零,缓冲不跨帧存数据,重建零损失。
- **必须 create-then-swap(非 Destroy→Create)**:无上限后 desiredCap 可能 OOM,先建新的、成功才 `buf=std::move(fresh)`(旧 buf 析构 vmaDestroyBuffer);失败保留旧缓冲走既有 drop 路径而非崩。这是去封顶的**强制搭档**。
- 重建后须重写 **ssbo(fr.matrixSet)/inst(fr.instSet) 描述符**;**vbo 无需**(顶点缓冲每帧 replay 现读 `fr.vbo->Handle()`,见 Graphics.cpp:753/1827)。各帧自有描述符集,fence 等过后重写安全。
- 无封顶合理性:每帧绘制需求由玩法天然有界,×2 收敛到真实需求 ≤2 倍;desiredCap 只能 INIT×2^n 不来自外部数据→无失控分配。
- 旧 `*_BYTES_PER_FRAME` 常量散布 13 处(cap 检查/worker 切片/CheckBatch 剩余空间)全换 `fr.*Cap`,改名后 grep 清残留(含 Graphics.h:768 注释)。

**验证手法(可复用)**：① PowerShell `Start-Process -PassThru`+轮询 `WorkingSet64` 取稳态峰值测内存;② **逼出 grow 路径**=临时把 INIT 改极小(256K/64K/128K)重建,普通场景被迫连续增长,看截图全部渲染正确反推 grow+描述符重写无误;③ 突发尖峰 `spike_grow.json`(4→瞬间 104 僵尸,untracked 在 autotest/scripts)极小 INIT 下渲染正确零崩溃。相关见 [project_pvz_perf_optimization](project_pvz_perf_optimization.md)(压测 11000z 的容量来历)、[reference_pvz_batch_instance_zorder](reference_pvz_batch_instance_zorder.md)(双队列)、[project_pvz_premultiplied_alpha](project_pvz_premultiplied_alpha.md)(UploadPixels 预乘)。

---

**2026-06-27 code-review(max workflow)修复 4 处缓冲缺陷(working-tree 未提交)**——审查 17c3a1d 的 max 多智能体审查发现并修：
- **#7 共用 overflowWarned 过度分配**：单旗 `fr.overflowWarned` 同时管"日志节流"和"翻倍决策",任一缓冲溢出→`EndFrame` 三缓冲全 ×2(inst 是瓶颈时把 VBO 也拖大,部分抵消省内存)。改=`PerFrame` 加 `vboOverflow/ssboOverflow/instOverflow` 三旗(overflowWarned 仅留日志节流),各溢出点(FlushBatch 拆 vbo/ssbo / FlushInstances / BeginParallelRecord 按 equal*Bytes==0 / worker 切片聚合)按缓冲归因,`EndFrame` 只翻倍真正溢出的那个。
- **#6 首帧大尖峰连续丢帧**：×2 逐帧爬升,大跳变要连续几帧才收敛(其间丢绘制)。改=`VkWorkerSlice` 加 `vboDemand/ssboDemand/instDemand` **真实需求计数**(`SliceHasRoom`/inst 推入处累加"想写"量含被拒绝的),`ReplayAndEndParallel` 聚合成帧级 `frameVboDemand` 等→`EndFrame` `growTo=max(cap×2, demand×1.25)` **一步到位**。顺带把负载均衡权重 `m_prevSlice*Demand` 从旧"溢出→cap×1.5 估计"升级成真实 demand。
- **#8 容量只增不减**：`desired*Cap` 一次重场景顶高后整会话不回收。改=`GrowBufferIfNeeded`→`ResizeBufferIfNeeded`(`cap==desired` 才早退,支持缩),`EndFrame` `considerShrink`:连续 `kShrinkIdleFrames=600` 帧占用 <desired/4 且无溢出→缩到 `max(INIT, 窗口峰值×2)`。缩容走与 grow 同一 create-then-swap+重写描述符路径。
- **#9 OOM 警告每帧刷屏**：`GrowBufferIfNeeded` 分配失败 LOG_WARN 未节流,持续 OOM 每帧×3缓冲刷 WARN(Release 不裁)卡主线程。改=sticky `growFailWarned` 成员,仅"进入失败态"报一次,成功复位。
- **验证**：用②逼小 INIT(32K/16K/64K)+并行预留区也缩(8K/4K/16K)+临时 LOG,实测 GROW 按缓冲独立(vbo 32→64→128KB / ssbo 16→32KB 各自走)、SHRINK 41 次后两张截图渲染零损坏(描述符 grow/shrink 双向都验)。底层 GPU 生命周期(fence 后扩容/create-then-swap/描述符重写)审查认定健全,问题全在更高层的扩容策略语义。
