---
name: pvz-gpu-instancing-reanim
description: PvZ 完成的下一档 perf 工作 — GPU instancing for reanim sprites。从 BatchVertex × 6 + SSBO mat4 路径切换到 InstanceRecord × 1 + gl_InstanceIndex 路径。实测 -1.39 ms wall / -14% frame time / 98.4→114 FPS @ 11000z；视觉 ✓；toggle 保留作 fallback
metadata:
  node_type: memory
  type: project
  originSessionId: 8f66491c-a2c5-4e86-b687-b8c8adff5e35
---

# GPU Instancing for Reanim Sprites — 已完成 (2026-05-23 → 2026-05-24)

**起点：** [pvz-precomputed-animation](project_pvz_precomputed_animation.md) 评估放弃后用户没立刻停手，问"GPU instancing 方向值不值得起"。读完架构发现 ~80% 前置工作已就绪（bindless textures / SSBO matrix / 持久映射 VBO / 并行 record/replay 都已在 phase 6 cleanup 时完成），重写 ROI 估算 → plan 2-3 周 / ~2.3-2.8 ms wall。用户选 PROCEED → 写 plan → subagent-driven execution。**最终实测 -1.39 ms wall（介乎 plan 估算下限以下）→ MARGINAL but KEEP by inspection。**

**Why:** 推翻了 [pvz-perf-optimization](project_pvz_perf_optimization.md) POST-PHASE-3 STOP 决定与 [pvz-precomputed-animation](project_pvz_precomputed_animation.md) 的"放弃此方向"结论。GPU instancing 路径是当前架构对剩余 trig + mat4 + 6-vertex inflation + write traffic 的真正可攻击点。完成后 plan 写的"不要主动碰 GPU instancing"这条**已经作废**。

**How to apply:** 工作已完成且 merged 到 working tree。`m_useInstancePath` toggle + slow-path fallback **保留**作 future A/B re-test 与 safety net。**不要主动重新评估这个方向**——除非未来 reanim 资源结构改变（更多子动画 / 帧索引变非均匀 / 等）或 perf 需求显著上移（144 FPS+ / 20000+ 僵尸）。

**2026-07-23 当前结构更新：** `InstanceRecord` 由原始 48 B 增至 56 B，新增打包的逐实例矩形 `clipMinXY/clipMaxXY`；`BatchVertex` 为 52 B，携带相同字段。所有 `PushClipRect/PopClipRect` 均无 flush，fragment shader 用 `gl_FragCoord` 裁剪，水路 `PushClipBottom` 只是全宽矩形别名。同场景 2 只水中僵尸从 `21 draw + 4 scissor` 降为 `19 draw + 0 scissor`；伴舞出土、图鉴窗和粒子区域 Clip 也不再切实例批次。无裁剪哨兵走片元快路径，默认实例与 `-NoInstance` 均由可见 AutoTest 验证。详见 [project_pvz_shader_clip_rect](project_pvz_shader_clip_rect.md)。

**2026-07-23 复合 Animator 收口：** 默认 `DrawInternalInstanced` 已按“父轨道本体→overlay→glow→该轨道附件”递归整棵附件树；不可见父轨道仍可作为附件锚点，隐藏且无附件的轨道继续在 trig 前早退。`hasChildren` 不再令射手身体等父 Animator 回退逐顶点路径，任意深度附件都写入同一实例流；CPU 仍负责动画插值与附件定位。矩阵慢路径只由 `-NoInstance` 显式启用，继续作为 A/B 与 safety net。`smoke_animator_recursive_instancing` 可见验证根/子同步 glow 与轨道状态，`smoke_plant_squish` 默认/`-NoInstance` 双跑验证递归缩放、渐隐及兜底；静止/压扁图两路径最大通道差 1，来自 RGBA8 量化。

---

## 最终实测结果（11000-zombie warmed stress test, same-session A/B）

| 指标 | A (instance OFF, `-NoInstance`) | B (instance ON, default) | Δ wall |
|---|---|---|---|
| **total / frame** | **10.16 ms (98.4 FPS)** | **8.77 ms (114 FPS)** | **-1.39 ms / -14%** ✓ |
| 6.Draw_submit(par-record) | 3.79 ms | 2.82 ms | **-0.97 ms (-26%)** ← 主要收益来源 |
| 2.GOM_Update(serial) | 2.37 ms | 1.92 ms | -0.45 ms（stable side-effect，待解释） |
| 3.Collision_Update | 1.59 ms | 1.57 ms | -0.02 ms（噪声） |
| 7.Draw_replay(serial) | 0.06 ms | 0.07 ms | +0.01 ms（噪声） |

**GATE 严格判定 = MARGINAL**（-1.39 ms 紧贴 -1.5 ms KEEP 门槛 just below）。**Plan 写 MARGINAL → "evaluate by visual artifacts"** → 视觉 PASS（11000z 渲染正常 + cherry bomb stress 无 overflow / artifact）→ **DECISION = KEEP**。

---

## 关键 plan 修订（写 plan 时不知道、实施期才发现）

1. **`Texture::atlasPage` 是 `const Texture*` 不是 bindless slot index** — plan 原写 `rec.texSlot = image->atlasPage;` 错。正确：`const Texture* bindTex = image->atlasPage ? image->atlasPage : image; rec.texSlot = bindTex->id;`

2. **instance pipeline 需要独立的 `instanceSetLayout`**（set=0 binding=0 InstanceRecord SSBO），与现有 `matrixSetLayout` 分离 — plan 原想用 set=0 binding=1 共享 layout 走不通（descriptor 写入冲突）。Shader 改 `binding=1` → `binding=0`。

3. **`RecordCmd` 必须从 12 B 增长到 16 B（加 `instOffsetAtCmd`）** — plan 原写"reuse vertOffsetAtCmd 字段"的方案行不通，因为 batch path 的 SetBlend 和 instance path 的 SetBlend 共享同一 `cmds` 流，无法靠一个字段区分。**static_assert 必须改成 16**。

4. **spv 路径在 `Shader/spv/` 子目录** — plan 原写 `"Shader/reanim_inst.vert.spv"`，实际项目走 `<ShaderSource>` ItemGroup → vcxproj `<Target Name="CompileShaders">` 自动输出到 `Shader/spv/`。

5. **vcxproj wiring 极简** — plan 原写 "duplicate the `<CustomBuild>` rule blocks"，实际只需在 `<ShaderSource>` ItemGroup 加 2 个项即可（项目已经有 Target 自动调 glslc）。

6. **Task 0 探针 `PROFILE_SCOPE` 不能放进 worker 路径** — `Animator::DrawInternal` 在 worker thread 上跑 165k tracks/frame，`Profiler::Add` 用 `std::map<string, double>` 是 NOT thread-safe。**修订成 `static std::atomic<int64_t> + RAII AtomicNsScope` 累加器**。Plan Task 0 设计阶段没意识到这条线程安全约束。

---

## 架构变化（永久保留）

新增基础设施（全部 modular，slow-path 不受影响）：

| 组件 | 文件 | 作用 |
|---|---|---|
| `InstanceRecord` struct (56 B) | Graphics.h | per-sprite 数据：tA-tD pre-multiplied affine + tx/ty + atlas UV + texSlot + colorRGBA8 + packed ClipRect |
| `reanim_inst.{vert,frag}.glsl` | Shader/ | GPU instance pipeline shaders。vert 用 `gl_VertexIndex 0..5` 展开 6 顶点 + `gl_InstanceIndex` 读 SSBO |
| `pipeInstAlpha` / `pipeInstAdd` | Graphics.cpp PIMPL | instance pipelines，独立 set=0=`instanceSetLayout`（InstanceRecord SSBO），共用 set=1=bindless textures |
| `instanceSetLayout` + `instancePool` | Graphics.cpp | 独立 descriptor set layout 与 pool（不污染 batch 的 matrix layout） |
| `PerFrame::instBuf` + `instSet` + `instCursor` | Graphics.cpp | 8 MB 初始、grow-on-demand 的持久映射 host-visible buffer + descriptor set |
| `VkWorkerSlice::instPtr/instBaseIdx/instCap/instCount` | Graphics.h | worker slice 加 instance 字段 |
| `m_prevSliceInstDemand` + 加权切片 | Graphics.cpp BeginParallelRecord | 复用现有 vbo/ssbo 加权切片算法，三路独立切 |
| `RecordCmd::instOffsetAtCmd` (12 → 16 B) | Graphics.h | SetBlend/DeferredText/DeferredGlyphRun 同时记 vbo + inst 边界；ClipRect 不再进入命令流 |
| `AppendReanimInstance` + `FlushInstances` | Graphics.cpp | 主线程串行 path + worker tl_record path |
| `EmitInstanceDrawRange` + `bindInstForBlend` | Graphics.cpp ReplayAndEndParallel | replay 时 emit instance segments；batch-first-then-instance ordering（PvZ z-order shadow→reanim 天然契合）|
| `DrawInternalInstanced` | Animator.cpp | 默认递归整棵附件树构 InstanceRecord；父子按轨道原序交错，trig 与附件定位仍在 CPU |
| `m_useInstancePath` toggle + `-NoInstance` flag | Graphics + main.cpp + GameApp | A/B switch 保留 |

**slow-path（原 `DrawInternal` 走 `DrawTextureMatrix`）完整保留** — 仅 `m_useInstancePath=false`（启动 `-NoInstance` flag）时强制整棵附件树走 slow path 做 A/B baseline；默认路径不再因 `hasChildren=true` 回退。

---

## 关键 insight & lessons learned

1. **架构友好度 >> 工程量估算精度** — 写 plan 时低估了 phase-6 cleanup 已经完成多少 GPU 工作（bindless / SSBO / 持久映射 / 并行 record）。这个项目对 GPU instancing 的前置投资让"2-3 周"估算大致准确（实际 ~1 工作日的 session time 因为基础设施齐备）。

2. **GATE A 数据 vs 最终 ROI 的关系** — Task 0 GATE A 第二条 (`trig+mat4 ≥ 10 ms CPU sum/frame`) 实测 ~6-7 ms 略低于门槛但用户选 PROCEED。最终 -1.39 ms wall < plan 估算 -2.3 to -2.8 ms（即工程出来 50% 收益没拿到）。**lesson：trig + mat4 alone 的占比是真实收益的合理 lower bound，但不是 upper bound** — write traffic 和 framework cost 收益要靠跨 task 测量验证。

3. **GOM_Update 神秘 -0.45 ms 收益** — 出现在两次独立测量中（Task 5 vs Task 4 -0.53; same-session A/B -0.45），不太可能纯噪声。三个候选 hypothesis：(a) RecordCmd-stream 长度变短改善 worker cache footprint；(b) GPU 异步完成更快让下帧 acquire 不阻塞；(c) 写带宽减少减少 LLC 压力间接帮助 Update。**未深究，留作未来 perf 兴趣项。**

4. **Same-session A/B 是 perf measurement 唯一可信源** — 跨 task baseline 漂移高达 ~0.65 ms wall（Task 4 baseline 3.98→4.30 不是真实回归）。**[pvz-parallel-update-phase2](project_pvz_parallel_update_phase2.md) 的"A/B 同 session 做"铁律** 在这次实施再次得证。所有未来 perf claim 必须 A/B same session 同 build 同 startup。

5. **Plan 设计阶段未读够代码导致的修订面**：写 plan 时只看 Animator.cpp + Graphics.h，没深读 Graphics.cpp::InitializeVulkan。实施 Task 2 时碰到 6 个 plan 修订点。**lesson：未来大型 plan 在 Task 0 之外加一个 Task -1: 读关键文件结构后修订 plan**。

6. **RecordCmd 12→16 B 是隐藏的代价** — 看似简单的 "add a field" 改动，实际让每个 worker 在 SetBlend 边界多写 4 B，165k cmds / frame ≈ 660 KB extra write。理论上有 ~0.05 ms wall cost，实测吸进噪声不可见。

7. **Plan 阶段对 `Animator::DrawInternal` worker-thread 调用路径不熟悉** — Task 0 PROFILE_SCOPE 设计撞 thread-safety 红线（`std::map` 并发写），实施中我作为协调者修了它。**lesson：超过 phase-2 之后的项目，所有 perf-related code 都假定可能在 worker thread 上跑**。

---

## 工程进度（本次 session）

8 个 Task + GATE A/B：
- Task 0: profile baseline + child-animator audit → `frac=0%`（全部 11000 zombie 都是 no-child）+ trig/mat4 数据
- Task 1: shaders + InstanceRecord struct
- Task 2: instance pipelines + vcxproj wiring（独立 `instanceSetLayout`）
- Task 3: instance buffer + descriptor set + slice carving
- Task 4: AppendReanimInstance + FlushInstances + ReplayAndEndParallel 扩展（最复杂 task，~300 行）
- Task 5: `DrawInternal` fork → `DrawInternalInstanced` ← **GATE B 测量点**
- Task 6: cherry bomb stress + fallback 验证 → 110 FPS 视觉 ✓
- Task 7: `m_useInstancePath` toggle + `-NoInstance` flag → same-session A/B (-1.39 ms)
- Task 8: cleanup + memory writeback（本文件）

每 Task subagent-driven 流程（implementer + inline review by orchestrator）。Subagent model 选择：mechanical tasks 用 haiku（Task 0/2/7）、shader/PIMPL/replay tasks 用 sonnet（Task 1/3/4/5）。Plan 写在 `docs/superpowers/plans/2026-05-23-gpu-instancing-reanim.md` 且 commit 进 70d0f85。

**改动统计：** 585 行 inserts / 42 deletions / 8 modified + 2 new shader files. **已合并到 master commit `388a845` 并 push 到 origin/master**（user 授权 assistant 执行）。本地 feature branch `perf/gpu-instancing-reanim` 已删除（已 ff-merge）。

---

## 后续候选 perf 方向（如果未来需要继续）

1. **去掉 trig 在 CPU 的部分** — DrawInternalInstanced 仍在 CPU 算 4×cosf/sinf（GATE A 教训：插值精度短弧/长弧 ambiguity 不让人 lerp trig'd values）。Vertex shader 接收 (kx, ky, sx, sy) 而非 (tA, tB, tC, tD)，在 GPU 算 trig — 可省 ~0.5 ms wall。但复杂度 +30%。

2. **GOM_Update 拆段 profile** — 神秘 -0.45 ms 受益源未明，如果搞清楚 hypothesis 可能能复现到其他 path 拿额外收益。

3. **指令-level pipeline batching** — 当前每个 worker slot 内每个 SetBlend 边界都重 bind pipeline + descriptor set。如果未来 reanim glow/overlay 多了，pipeline switch overhead 会涨。可以重排 instance segments 按 BlendMode 分桶 emit，但会破坏 PvZ 的 z-order 假设（shadow→reanim→glow 需要严格顺序）。

4. **DrawInternalInstanced 的 trig 共享子轨道** — 当前每个 track 独立算 trig，如果一个 reanim 有 N 个 track 共享 kx/ky（不太常见，但可能），可以缓存 trig 结果。微优化。

不要主动启动这些 — 等用户兴趣或需求驱动。当前 114 FPS / 11000z 是原版 100 FPS 的 1.14x、原版 30 FPS 的 3.8x，**绝对超饱和**。

---

## 与既有 memory 的关系

- 推翻 [pvz-perf-optimization](project_pvz_perf_optimization.md) POST-PHASE-3 "STOP decision" 与"GPU instancing 1-2 周 / ~2.8 ms 不要主动启动"——已启动并完成，实测 -1.39 ms。
- 推翻 [pvz-precomputed-animation](project_pvz_precomputed_animation.md) 末尾"GPU instancing 仍是未来下一档候选但不主动启动"——已完成。
- 印证 [pvz-parallel-update-phase2](project_pvz_parallel_update_phase2.md) "A/B 同 session 内做"铁律。
- 印证 [pvz-perf-optimization](project_pvz_perf_optimization.md) phase-2/3 教训"GATE 必须看 end-to-end / total/frame，子桶辅助"——本次 GATE B 严格按 plan 阈值 total/frame ≤ 8.5 ms 判定（MARGINAL inspection-based KEEP）。
- 印证 [collaboration-style](feedback_collaboration_style.md) "measure-first / 保留 fallback / user builds in VS / honest framing"。

---

## Postscript: 2026-05-26 ship 后 fix — glow + Z-order bug

ship 后 2 天 user 报 GlowingTimer>0 时严重渲染 bug：other plants 跟着变亮、部分 plants 透明不绘制、shadow 消失。同 session 内 fix 完成。**两类独立 bug 由一个 commit 引入但表现耦合**：

### Bug 1: `m_currentBlendMode` 变量过载（blend leak）
- `m_currentBlendMode` 同时承担两个不变量：(a) 实例队列当前堆的 blend（FlushInstances 选 pipeline）和 (b) DrawTexture/DrawTextureMatrix 的 per-vert `bm` 默认值。
- `AppendReanimInstance` 写 (a) 时污染 (b) — glow（Add）写完后 ShadowComponent 读 `m_currentBlendMode=Add` → bm=1 → shadow 走 pipeBatchAdd 不可见。
- **Fix**: 新增 `m_queuedInstanceBlend` 专门承载 (a)；`AppendReanimInstance` main 路径不再触碰 `m_currentBlendMode`；worker 路径用 save/restore `tl_blend`（镜像 `RecordDrawTextureMatrix` 行 1897-1951 的已验证模式）。
- **Flush 次数完全保持与原代码一致** — 不是 save/restore-then-double-flush（第一版 fix 失败的方案），而是变量重命名 + 不修改全局态。

### Bug 2: serial fallback 下 batch / instance 双队列不保序（Z-order + body 消失）
- 主线程 serial path 写 `m_batchVertices`（lawn/shadow/UI）与 `m_batchInstances`（plant body/glow）独立。
- `AppendReanimInstance` 在 blend 变化时 **mid-frame** 触发 `FlushInstances` — 这时 `m_batchVertices` 里早积累的 batch quads 要等到 EndFrame 才 flush。cmd buffer 顺序：plant body 先 vkCmdDraw → 之后 lawn 才 vkCmdDraw → **lawn 反盖 plant body 不可见**。
- 同根 Z-order：植物/僵尸压在菜单上面。
- worker 模式没这个 bug — `emitUpTo` 在 replay 时按 SetBlend 段交错 emit batch + instance，per-slot 严格保序。
- **Fix**: 主线程双向 cross-flush — `AddVertices` 前 `if(!m_batchInstances.empty()) FlushInstances()`；`AppendReanimInstance` 主线程分支前 `if(!m_batchVertices.empty()) FlushBatch()`。worker 不动。

### Why: lesson learned
原 commit `388a845` 设计 instance path 时只 review 了 worker replay 的保序逻辑（emitUpTo），漏审 **serial fallback 路径**。Serial fallback 走的是「两个独立 mid-frame flush 队列」结构，本来在 batch-only 时代不存在保序问题（batch 内只有一个队列），加 instance 后立刻产生 dual-queue 跨队列保序问题。

### How to apply
- 改动文件：`Graphics.h`（加成员）+ `Graphics.cpp`（4 处编辑，全在 main thread 和 worker 路径，不动 replay）。
- 未提交 commit — user 在 VS 验证后会自己决定何时 commit。
- **未来在本仓库设计 dual-queue / mid-frame flush 时要警觉**：任何与已有队列的相对顺序敏感的新路径，serial fallback 必须显式 cross-flush（worker 模式有 emitUpTo 兜底，但 serial 没有等价机制）。这条作为 **[feedback-dual-queue-order-preservation](feedback_dual_queue_order_preservation.md)** 记入。
