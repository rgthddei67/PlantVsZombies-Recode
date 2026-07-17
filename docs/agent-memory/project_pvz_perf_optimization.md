---
name: pvz-perf-optimization
description: "PvZ C++ perf log — LATEST(2026-06-24): 11000 zombies @ 168 FPS / 5.94 ms fully CPU-bound, STOP reaffirmed; history covers atlas/parallel-update/GPU-instancing experiments & verdicts"
metadata:
  node_type: memory
  type: project
  originSessionId: 7d6bf991-0204-4927-adcf-b4a0fd3c7e0b
---

# PvZ 4400-zombie performance optimization (active, started 2026-05-15)

Goal: 4400 zombies ran ~25-30 FPS with low CPU% / low GPU% / low RAM — classic
single-thread serial bottleneck. Driving this as a measure-first investigation.

**Why:** user explicitly wants the bottleneck found by evidence, not guesswork.
**How to apply:** continue the experiment→measure→decide loop; never pile a fix on an
unverified hypothesis. Always ask for STEADY-STATE (warmed-up) profile numbers — the
user's first measurement is often a cold-load reading that runs ~10ms slow and misleads.

## ⭐ LATEST AUTHORITATIVE BREAKDOWN + STOP REAFFIRMED (2026-06-24)
**读本文件先看这一段——下方 2026-05-23 的 "POST-PHASE-3 DRAW BREAKDOWN" 与
"NEXT POTENTIAL LEVER: GPU INSTANCING" 两段已 STALE(GPU instancing 早已落地
commit 388a845;分解数字也全变),仅作历史保留。**

用户实测 11000 僵尸 `-Profile` 稳态(`clang-release`,O2):
**total 5.94 ms / 168.5 FPS,完全 CPU-bound**(A 0.02 + B.SceneUpdate 3.24 +
C.SceneDraw 2.66 = 5.92 ≈ total;**Present 0.14ms / replay 0.03ms → GPU 不是瓶颈**)。

| 桶 | ms | % | 并行性 |
|---|---|---|---|
| 6.Draw_submit(par-record) | 2.37 | 40% | ✅ 已并行(动画插值+建 InstanceRecord),**最大块但减不了线程只能减工作量** |
| 2.GOM_Update | 1.85 | 31% | 阶段A并行≈0.72 + **2d.PhaseB_serialUpdate 1.13 纯串行** |
| 3.Collision_Update | 1.33 | 22% | detectRow 按行并行 + 分桶/回调串行(已优化过) |
| 杂(replay/overlay/present/sort/input) | ~0.3 | 5% | 串行 |

**本会话纠正的两个错误假设(诚实留痕):** 我起手猜 ①`7.Draw_replay` 串行瓶颈 ②GPU-bound——
实测 replay 0.03ms、Present 0.14ms,**两个全错**,是 CPU-bound。

**Amdahl 主线(下次若重启必读):** 最大桶 par-record 已吃满多核,加线程无用;
真正卡帧的是**串行地板** 2d(1.13)+collision串行残渣+杂 ≈ 2.5-3ms → **FPS 天花板约 330**。
collision 还有隐藏负载不均:detectRow 只按活跃行派发,PvZ 仅 5-6 行 → 16 核机器上最多 6 路并行。

**STOP 决定(用户 2026-06-24 明确选"够了,停在这",延续 2026-05-23 STOP):**
168 FPS@11000z 是远超真实玩法(生存最多几百僵尸)的人造负载,边际收益趋零。
**若将来重启,按 ROI 排序的候选(均须先量再动):**
1. par-record(2.37,最大块)实例级视口剔除——**ROI 完全取决于压测里出屏僵尸占比**,
   若从右边缘外排队涌入则收益大,若全堆可见草坪内(纯 overdraw)则无用;动手前先量在屏/出屏实例数。
2. 2d.PhaseB_serialUpdate(1.13,Amdahl 串行地板)——obj->Update() 的 AI/移动/啃食/射击,
   有副作用难并行,delicate;治本路线=查清哪类对象 Update 最贵+能否挪无副作用部分进阶段A。
3. collision——已 6 路并行,串行残渣多属固有,**低 ROI,不建议动**。

**编译器:** 本会话核实并保持 **O2**(非 O3)——已有 march=native+flto+fp:fast 把 O3 上行空间提前吃掉,
且 GPU 不是瓶颈编译级别杠杆小;CLAUDE.md Build 段的 stale `-O3` 已修为 O2(commit e14a68c)。

## Instrumentation (TEMPORARY scaffolding — remember to remove later)
Added `PlantVsZombies/Profiler.h` (header-only, RAII `PROFILE_SCOPE`, FlushBatch
counter, prints a `FRAME PROFILE` block every 60 frames to the Debug console).
Wired into Scene.cpp, GameObjectManager.cpp, GameApp.cpp, Graphics.cpp. This is
diagnostic only and should be ripped out once optimization is done.

## DONE & KEPT — per-reanim texture atlas (the real win so far)
Implemented and verified (user confirmed visuals correct, no corruption):
- `GLTexture` extended with `atlasPage`/`aU0..aV1` (ResourceManager.h)
- `ResourceManager::BuildReanimAtlases()` — groups each reanim's part textures,
  shelf-packs into a 4096² page via FBO blit, writes atlas UVs back
- `Graphics::DrawTextureMatrix` (Animator's only render path) remaps UV to atlas sub-rect
- called at end of `GameApp::LoadAllResources`
Result: FlushBatch 976→4 /frame; FPS ~30 → ~40 steady-state; removed a scaling cliff.
161 source textures → 1 atlas page. SSBO confirmed ACTIVE (4 flushes for ~90808
sprites ⇒ matrix limit ~32768).

## RULED OUT (do not retry — proven by experiment)
- **Flush count is NOT the FPS bottleneck.** 976→4 flushes barely moved the cold
  frame time. Atlas still helped (driver-sync + scaling), but GL submit count is not it.
- **Buffer orphaning (B): catastrophic** at high flush count — adding `glBufferData`
  orphan per FlushBatch made Draw_submit 10ms→364ms (~680MB/frame realloc churn at
  976 flushes). Reverted. Lesson: orphan only viable with few flushes.
- **`reserve()` on m_batchVertices/m_batchMatrices is MOOT.** `FlushBatch` uses
  `.clear()` which retains capacity; only the cold first frame reallocs. Don't add it.
- **C (SSBO matrix-limit unclamp) already exists** in code (Graphics.cpp:58-71,
  raises to 32768) and is active. Nothing to do there.
- vsync: user turned it OFF in the main menu (default false). SwapWindow cost is NOT vsync.

## REAL remaining bottlenecks (steady-state, ~25ms/frame @ 40fps)
1. **`6.Draw_submit` ~11ms — serial CPU command building.** GameObjectManager::DrawAll
   serial loop → Animator::Draw → ~90808 × (mat4 multiply + build 6 BatchVertex).
   Single-threaded. Fix direction: PARALLELIZE submit (PrepareForDraw is already
   parallel at only ~0.9ms, so the thread pool works — the serial Draw funnel is the problem).
2. **`9.SwapWindow` ~15-17ms — GPU overdraw.** ~90k large alpha-blended overlapping
   quads from 4400 stacked zombies. Fix direction: off-screen culling / overdraw reduction.
3. `2.GOM_Update` ~2.6ms serial game logic (secondary).

## CORRECTION 2026-05-16: prior baseline is STALE; NOT vsync-capped
The old "~25ms/40fps, SwapWindow 15-17ms" baseline predates the
`95e448d 修复垂直同步问题` commit (and others) — DO NOT compare across sessions.
Current verified steady-state truth (vsync OFF, user tested both ways, variance
tiny): game is genuinely **CPU-bound at ~60 FPS (16.5ms)**, NOT vsync-capped.
The Profiler's "9.SwapWindow(vsync)" label is a hardcoded string, not proof vsync
is on; SwapWindow ≈ 2.5ms is real present cost. My earlier "vsync-capped 60"
read was WRONG and retracted. Real current #1 bottleneck = `6.Draw_submit`
~9ms serial (~55% of frame). Other phases: GOM_Update 2.4ms, SwapWindow 2.5ms,
Draw_prepare(par) 0.83ms. Original goal (4400z off 25-30fps) effectively met.

## EXPERIMENT 1 (IMPLEMENTED & MEASURED 2026-05-16 — KEEP)
RESULT: Draw_submit ~11ms → 9.0ms (−2ms / −18%), consistent across vsync on/off.
Hypothesis direction confirmed but MAGNITUDE over-estimated: implied ~22ns/mat4
mul ⇒ glm IS using SSE on this build (not scalar as I assumed). The redundant
identity multiply was real & safely removed (zero downside, also speeds
PushTransform) but is NOT the dominant submit cost. Decision: KEPT.

## EXPERIMENT 1 — original note (IMPLEMENTED 2026-05-16, AWAITING MEASUREMENT)
Hypothesis: Animator::Draw does PushTransform(identity); transform-stack top is
identity for all ~90808 sprites/frame, so `m_transformStack.back() * pivotTransform`
in DrawTextureMatrix's batch branch is a wasted serial mat4×mat4 ~90k×/frame —
likely a big chunk of `6.Draw_submit` ~11ms (MSVC glm = scalar, no SIMD by default).
Change (one variable): added parallel `m_transformIsIdentity` flag stack in
Graphics.h; maintained in ctor/Push/Pop/SetIdentity/Translate/Rotate/Scale;
DrawTextureMatrix batch branch skips the multiply when top is identity (also
PushTransform skips identity*identity). `IsIdentityMat` exact-compares; only
false-negatives possible (extra multiply, never wrong result). Verified the 7
m_transformStack write sites are the only ones — flag can't desync. Immediate-mode
& other Draw* paths intentionally untouched (keep experiment single-variable).

NEXT: user builds in VS, runs WARMED-UP 4400-zombie test, pastes steady-state
`FRAME PROFILE` block + startup `[Graphics] ... SSBO=? m_matrixBatchLimit=?`.
Compare `6.Draw_submit` & total/frame vs before (~11ms / ~25ms@40fps);
`9.SwapWindow` should be unchanged (control). Then DECIDE: keep+next experiment,
or revert if no win.

## EXPERIMENT 2 (IMPLEMENTED 2026-05-16, AWAITING MEASUREMENT)
Goal: cut serial `6.Draw_submit` ~9ms (the real #1 bottleneck, ~55% of the
~16.5ms CPU-bound 60fps frame). Single conceptual variable: relocate per-command
CPU work (atlas UV remap, NormalizeColor, blend resolve, 6-vertex construction)
from the SERIAL Animator::Draw into the ALREADY-PARALLEL PrepareCommands; serial
path becomes only BindTexture + AddMatrix + index patch + 6-vertex memcpy.
Changes:
- Graphics.h: new `PreparedSpriteCmd{mat4 matrix; BatchVertex verts[6]; GLuint texId;}`
  + public `AppendPreparedSprites(const PreparedSpriteCmd*, size_t)`.
- Graphics.cpp: AppendPreparedSprites — per-cmd BindTexture/AddMatrix/insert6/patch/
  CheckBatch, SAME flush-boundary order as DrawTextureMatrix (behavior-preserving);
  uses m_transformIsIdentity fast-path (Exp 1) so identity stack skips the multiply.
- Animator.h: per-Animator `std::vector<PreparedSpriteCmd> mPreparedCmds` (capacity
  reused across frames).
- Animator.cpp PrepareCommands: after CollectDrawCommands, build mPreparedCmds
  (pure CPU, per-object, no shared/GL state — safe under threadpool), then frees
  mCachedCommands to halve transient memory.
- Animator.cpp Draw: if mPreparedCmds non-empty → AppendPreparedSprites + return;
  else original fallback fully intact (no Push/PopTransform needed on fast path).
Behavior-preserving: identical verts/matrices/order/flush semantics. Pre-existing
benign texIndex-staleness-on-AddMatrix-flush is replicated, NOT introduced.
Transient mem cost ~30MB steady (capacity-stable) — accepted, will measure.

NEXT: user builds in VS, runs WARMED-UP 4400-zombie test, pastes steady-state
`FRAME PROFILE`. EXPECTED: `6.Draw_submit` ~9ms → ~3-4ms; `5.Draw_prepare(par)`
rises slightly (more work, but parallel); total/frame drops (toward 80-100fps if
hypothesis holds); `9.SwapWindow` unchanged (control). Then DECIDE: keep / iterate
(bulk-append, reduce per-cmd overhead) / revert if no win.

## EXPERIMENT 2 RESULT: FALSIFIED & REVERTED (2026-05-16)
Measured: Draw_submit 9.05→8.88ms (≈noise), Draw_prepare(par) 0.83→1.51ms
(+0.68 = the moved work), total 16.58→17.13ms (slightly WORSE), FlushBatch
4.0/frame unchanged (no semantic regression). Hypothesis WRONG: the per-command
math (NormalizeColor/blend/6-vert build) was only ~0.7ms total, NOT the 9ms.
Moving it to parallel even ADDED a 336B/cmd read to the serial path. Conclusion:
serial Draw_submit is NOT CPU-math-bound. Fully reverted (Exp 1 kept). All
PreparedSpriteCmd/AppendPreparedSprites/mPreparedCmds removed — verified zero refs.

## NEW HYPOTHESIS (to test with cheap probe, IN PLACE 2026-05-16)
Serial `6.Draw_submit` ~9ms is dominated by the SERIAL ~30MB/frame accumulation
WRITE into m_batchVertices/m_batchMatrices (6×push_back BatchVertex + push_back
mat4, ×~90811 cmds) — memory-bandwidth bound, not compute.
PROBE: `static constexpr bool kProbeSkipBatchAccum` at top of Graphics.cpp
(currently false). When true: AddMatrix returns 0 / AddVertices returns early —
keeps ALL per-cmd compute, skips only the accumulation write. Probe ON tell:
FlushBatch calls=0 + blank screen (batch stays empty, FlushBatch early-returns).
DECISION RULE:
 - Draw_submit collapses (≈9→2-3ms) ⇒ confirms memory-bound ⇒ real fix =
   parallel preallocated-offset batch fill (prefix-sum per-object ranges,
   threads write global buffers directly, serial = few memcpy+upload).
 - Draw_submit ≈unchanged ⇒ cost is per-cmd call/cache-miss traversal of 90811
   cmds across scattered objects ⇒ different fix (SoA/flatten / fewer objects).
MEASURE PROTOCOL: same build/session A/B to kill cross-session confound —
Run A flag=false (clean post-revert baseline, expect ~Exp1 ~9ms), Run B
flag=true. Both warmed-up steady-state. Compare Draw_submit A vs B only.

## PROBE RESULT: CONFIRMED memory-bound (2026-05-16, same-session A/B)
Run A (flag=false): Draw_submit 9.44ms, total 16.65ms/60.1fps, FlushBatch 4/~90808.
Run B (flag=true, skip accum write): Draw_submit 3.86ms, FlushBatch 0/~0 (probe
active, screen blank — expected). ⇒ ~5.6ms (60%) of serial Draw_submit IS the
~30MB/frame push_back accumulation into m_batchVertices/m_batchMatrices. Residual
~3.9ms = per-cmd compute/traversal. Probe flag restored to false in Graphics.cpp.

## PLAN WRITTEN — awaiting execution (2026-05-16)
`D:\PVZ\PlantsVsZombies\PlantVsZombies\docs\superpowers\plans\2026-05-16-parallel-batch-fill.md`
Design A (lower-risk, measure-gated): per-object parallel-built BatchVertex/mat4
fragment in PrepareCommands; serial = one bulk AppendObjectBatch per object
(~5000 iters) instead of per-command (~90811), with matrixIndex rebase + texIndex
patch + 32768 flush-boundary + multi-texture homogeneity fallback. Task 4 is a
measurement GATE; Design B (cross-core preallocated-offset fill) is a SEPARATE
conditional follow-up plan, only if A proves bandwidth-bound not push_back-bound.
NOTE: Tasks 1-3 are atomic — do NOT measure between them (Task1 alone is slower
because PrepareCommands clears mCachedCommands before Draw is rewired). Measure
only at Task 4. Probe (kProbeSkipBatchAccum) must NOT be removed until Design A
is final (Task 5) — it's the channel for any Design B work.

## DESIGN A IMPLEMENTED + REVIEWED — awaiting Task 4 measurement (2026-05-16)
Branch `perf/parallel-batch-fill` (in-place, user builds from working dir).
Plan Tasks 1-3 applied as one atomic change via subagent-driven flow:
implementer → spec review (✅ compliant) → code-quality review (CHANGES
REQUESTED) → 4 fixes applied → code-quality re-review (✅ APPROVED).
Files: Animator.h (mFragVerts/mFragMatrices/mFragTexId/mFragHomogeneous=false),
Animator.cpp (PrepareCommands builds fragment in parallel; Draw fast path +
intact fallback), Graphics.h/.cpp (AppendObjectBatch: per-object bulk append,
flush-boundary-correct, matrixIndex rebase + texIndex patch). Fixes made:
degenerate >limit path now assert+skip (was silently corrupt), size_t pre-flush
compare, mFragHomogeneous default false, doc fixed. Deliberately NOT changed
(rationale verified): BlendMode::None map (CollectDrawCommands never emits None),
probe retained (plan Task 5), reserve-after-clear (benign). NOT committed
(commits user-driven). NEXT = Task 4 GATE: user builds in VS, runs warmed-up
4400-zombie test, visual check (FlushBatch must stay ~4/~90808, no artifacts),
pastes steady-state FRAME PROFILE. Compare 6.Draw_submit vs 9.44ms baseline;
decision rule in plan (≤3ms keep+Task5 / 3-6ms partial / ~unchanged ⇒ Design B
follow-up plan). Record result here.

## DESIGN A: FALSIFIED & REVERTED (2026-05-16)
Measured (warmed-up, 4400z): 6.Draw_submit 9.44→9.08ms (≈noise),
5.Draw_prepare(par) 0.85→1.44ms (+0.59 moved work), total 16.65→17.44ms
(slightly WORSE), FlushBatch 4/~90808 unchanged (no visual regression).
⇒ Decision rule "≈unchanged" branch: serial Draw_submit is RAW MEMORY
BANDWIDTH bound (per-object bulk resize+sequential-write costs the same as
90811 push_backs), NOT push_back overhead. Per-object bulk concat cannot win.
Reverted via `git checkout -- ` the 4 files back to commit `2f131d8 优化Step 1`
(verified: m_transformIsIdentity=9 → Experiment 1 KEPT & is committed; Design A
gone). NOTE: the diagnostic probe `kProbeSkipBatchAccum` was ALSO uncommitted
and got dropped by that revert — must be RE-ADDED next session for any Design B
measurement (full block is documented above under "NEW HYPOTHESIS / PROBE").

## STATE FOR NEXT SESSION (2026-05-16 end)
- Code = commit `2f131d8 优化Step 1` (atlas + Experiment 1, the only kept wins).
  Working tree clean except unrelated `.claude/settings.local.json` + untracked
  `docs/` (the plan is preserved there).
- Branch: `perf/parallel-batch-fill` (in-place; no commits made by assistant).
- Plan file kept: docs/superpowers/plans/2026-05-16-parallel-batch-fill.md.
  Its Task-4 decision rule fired "≈unchanged ⇒ write the SEPARATE Design B plan".
- INDICATED NEXT STEP = Design B: true cross-core preallocated-offset fill —
  parallel phase prefix-sums per-object sprite counts → each object gets a
  disjoint range in preallocated global m_batchVertices/m_batchMatrices →
  threads write their slots directly (no serial 30MB write at all) → serial =
  just the GL upload + segmented draw. Handles 32768 matrix-flush segmentation +
  clipped/non-animated objects as segment boundaries. This is the only design
  that removes the bandwidth-bound serial write (Design A proved it's bandwidth,
  not overhead). Re-add the probe first to re-baseline, then write Design B plan.

## PARALLEL-DRAW LANDED (commit 629f973 多线程优化) + REGRESSION FIXED (2026-05-16)
The cross-core parallel record/replay Draw path shipped: `GameObjectManager::DrawAll`
splits objects into worker slots → each `GameObject::Draw(g)` runs on a WORKER
THREAD → main thread `ReplayAndEndParallel`. Threshold: serial fallback if
<200 objects or parallel disabled.
**HARD CONSTRAINT (will recur):** every Graphics entry that touches GL or a
shared cache MUST have an `if (tl_record) {...}` worker guard (defer or skip).
All `DrawXxx` have it; `AcquireTextTexture` was the ONE that didn't →
`CardDisplayComponent::DrawSunCost` called it on a worker thread → GL calls with
no context + concurrent `m_pinnedTextCache` writes → sun-cost numbers rendered
as garbage/other textures. FIX (applied, not committed): added `tl_record` guard
to `AcquireTextTexture` (skip on worker) + pre-warm `mSunTextCache` on main
thread in `CardDisplayComponent::Start()`. When auditing future worker-thread
Draw code, check for any other un-guarded GL/shared-state call sites.

## BUILD CONFIG FIX (2026-05-16) — Release|x64 had NO explicit `<Optimization>`
vcxproj Release|x64 ClCompile was missing `<Optimization>` entirely. Applied:
added `<Optimization>MaxSpeed</Optimization>` (/O2); removed stray
`<StructMemberAlignment>16Bytes</>` (/Zp16) — grep proved zero
SIMD/GLM_FORCE_*/alignas/__m128 in codebase so it was pure noise; switched
LTCG `UseFastLinkTimeCodeGeneration`→`UseLinkTimeCodeGeneration` (full /LTCG).
AVX2/SDLCheck/GS/PDB/Win32-leftover deliberately untouched. Backup:
`PlantsVsZombies.vcxproj.20260516_174138.bak`.
**OPEN QUESTION — do NOT assume prior numbers are invalid.** The static rule
"no `<Optimization>` ⇒ cl defaults to /Od" is commonly true, BUT counter-
evidence in this very project: Experiment 1 measured ~22ns/mat4 mul (see line
~71) which is optimized-codegen fast, NOT /Od scalar. `WholeProgramOptimization=true`
(/GL) + LTCG likely supplied optimization at link time regardless. So whether
the 16.65ms/9.44ms baseline was on an unoptimized binary is UNVERIFIED.
RESOLUTION is empirical: next session, user does a full **Rebuild** (not
incremental — /O2 + LTCG changed), re-runs the warmed-up 4400z baseline.
If total/Draw_submit drop a lot ⇒ prior baseline was stale, re-baseline
everything before any Design B work. If ≈same ⇒ /GL+LTCG already optimized,
prior numbers stand. Either way, judge Design B against the FRESH post-fix
baseline, not the pre-fix one.

## WORKER-SLICE OVERFLOW FIX (2026-05-17, applied, NOT committed, awaiting VS verify)
Symptom: big scenes (many zombies + particles) spam `[Graphics] Worker slot 0
overflowed slice (vbo 111210/111211, ssbo 18535/20479)`. NOT a crash — overflow
path drops that slot's further draws + warns once/frame (sprites flicker).
**Three distinct overflow messages exist — diagnose by which one + its numbers:**
`Frame buffer overflow`(Graphics.cpp FlushBatch, global VBO/SSBO exhausted) /
`BeginParallelRecord: insufficient...`(main thread ate buffer pre-parallel) /
`Worker slot N overflowed slice`(per-slot static slice full). User's was #3.
ROOT CAUSE (confirmed by numbers): double imbalance —
(a) `GameObjectManager.cpp:174-176` dispatches objects to slots as **equal-OBJECT-COUNT
contiguous render-order chunks**; per-object quad cost varies wildly (bg quad=6
verts vs particle emitter=1000s), so slot 0's render band is far heavier;
(b) `BeginParallelRecord` carved the 56MB parallel arena into **equal byte slices**
(`vboUsable/numWorkers`). slot 0 needs >1/12, overflows its ~4.7MB slice while
total demand is only ~17/56MB (your own measured headroom, code comment Graphics.cpp).
So it's IMBALANCE, not capacity — bumping VBO size is the wrong lever.
KEY CONSTRAINT for future: `ThreadPool::WorkerLoop` (ThreadPool.h:65-67)
hardcodes equal index-based `[start,end)` split; you CANNOT rebalance object→slot
chunks from the Dispatch lambda without modifying the shared ThreadPool primitive
(also used by PrepareForDraw) or a decoupled slot-range table. And replay
(`ReplayAndEndParallel`) requires each slot = one contiguous render-order range
emitted in slot order — so naive `i % numWorkers` round-robin would BREAK draw
order (transparency/layering). Don't propose it.
FIX APPLIED (minimal, correct): keep object→slot mapping unchanged; make
`BeginParallelRecord` size each slot's slice **proportional to that slot's
previous-parallel-frame occupancy** (per-slot granularity = naturally available).
Floor each slot to ~88KB (vbo)/~32KB (ssbo) so a newly-hot slot survives 1 frame;
elastic remainder split by weight; Σ ≤ usable by construction (never touches the
post-parallel reserve). Overflowed slot's true demand is unmeasurable (dropped) →
recorded as cap×1.5 fast-attack ⇒ converges in 1-2 frames. No history / numWorkers
change / can't-fit-floor ⇒ pure equal split (= exact old behavior, safe regression
floor). Render order + replay contract untouched, zero extra memory.
Files: Graphics.h (m_prevSliceVboDemand/m_prevSliceSsboDemand vectors),
Graphics.cpp (BeginParallelRecord weighted prefix-sum split replacing equal split;
ReplayAndEndParallel captures per-slot demand before m_numActiveWorkers=0).
NOT committed (commits user-driven). VERIFY: user runs big-scene in VS, confirms
the `Worker slot N overflowed slice` line stops (or prints ≤1 frame on a sudden
spike then self-quiets as weights converge), visuals correct (no missing sprites).

## PHASE-1 PARALLEL UPDATE: FALSIFIED & REVERTED (2026-05-23)
场景升级到 ~10000 僵尸后 total/frame ~15ms (66fps)，主线程瓶颈在 `2.GOM_Update`
~7ms (47%)。按 5 天前写好的 phase-1 plan（拆 Animator::Update 成 AdvanceFrame
并行段 + FireFrameEvents 串行段）执行完 Task 0-5 + GATE A/B，实测**负改善**
（GOM_Update +0.87ms / total +0.82ms）。诊断埋点（2c/2c2/2d）揭示根因：
- plan §"设计核心"假设 "Animator 帧推进占 Update 80%" → 实测只占 **~12%**
  (0.85/7.58)，AdvanceFrame 并行总工作 0.80ms（dispatch 开销仅 0.05ms）
- 阶段 B 串行 6.67ms ≈ A 基线 6.63ms — 拆出 AdvanceFrame 几乎没让 phase B 减少
- 总成本 0.85 (par) + 6.67 (ser) = 7.52ms vs 原 6.63ms，多 0.89ms 是拆分本身的
  开销（额外函数调用 / 两次进入 Animator / 函数内联失效 / 子节点递归遍历两次）
真正的耗时不在"推帧"（标量算术），在 phase B：FireFrameEvents 回调 + 业务逻辑 +
GameObject 组件遍历 + 子节点递归——其中绝大部分**本质上必须串行**（音频/spawn/RNG）。
REVERT 完成：`git restore` 回 097df77 + 重新只取消 6 处 PROFILE_SCOPE 注释。
详情数据/教训见 [pvz-parallel-update-phase1](project_pvz_parallel_update_phase1.md)。
**教训给下次：** 任何 perf plan 起手第一件事必须 profile 拆段拿数据；不要预先
假设瓶颈占比。dispatch 开销在此项目里几乎免费，未来 perf 设计可放心 dispatch 数千次。

## PHASE-2 PARALLEL UPDATE: SUCCESS & COMMITTED (2026-05-23, commit 292f68e)
基于 phase-1 教训重写设计：把**整个 Animator::Update** 并行（不是 AdvanceFrame），
帧事件 callback 入 per-worker `mDeferredEventBuffers[slot]`，主线程串行 drain。
实测 11000 zombie / 60-frame steady-state：

| 段 | A 基线 (强制 fallback=原码) | B 阶段二 (并行) | Δ |
|---|---|---|---|
| total/frame | 14.43 ms (69.3 FPS) | 10.99 ms (91.0 FPS) | **-3.44 ms** ✓ |
| 2.GOM_Update | 6.52 ms | 3.38 ms | -3.14 ms |
| 2c.DrainEvents | — | **0.00 ms** | drain 免费 |
| 2d.PhaseB_serial | — | **2.53 ms** | ← 新瓶颈 |
| 1.Particles_Update | 9.26 ms | 6.16 ms | **-3.10 ms** ← 意外间接受益 |

**关键 insight：**
1. `2c.DrainEvents=0ms` 证伪了 spec §9 担心的 `std::function` heap alloc 隐藏成本。
2. `2d.PhaseB_serial=2.53ms` 是 phase B 剩余串行（GameObject 组件遍历 + UpdateGlowingEffect + 自动销毁判断），印证 phase-1 教训"phase B 大部分是组件遍历"。
3. Particles_Update 顺带 -3.10ms 是间接受益，本 plan 没动粒子（未深究原因，下次有兴趣可 profile）。
4. **vs phase-1 教训印证：** phase-1 拆 AdvanceFrame（占 Update 12%）= 负改善；phase-2 拆整 Animator::Update（占 Update 66%）= 大胜。**拆对位置是一切。** dispatch 0.05ms 几乎免费这条结论也再次成立。

详情/未来方向/Task 1 audit 结果见 [pvz-parallel-update-phase2](project_pvz_parallel_update_phase2.md)。

## PHASE-3 COMPONENT-UPDATE SKIPPING: SUCCESS & COMMITTED (2026-05-23)
phase-2 后 `2d.PhaseB_serial` 本是新瓶颈，但 phase-3 期间发现 phase-2 内存里这个数 (2.53ms) 已 stale；同 HEAD 实测仅 1.43ms。phase-3 设计 = `Component::NeedsUpdate()` virtual（默认 false）+ `GameObject::mUpdatableComponents` 缓存视图，让 `GameObject::Update()` 只 iterate 真正需要 Update 的组件（zombie/plant/bullet 视图为空，outer loop 直接退出）。

实测 11000 zombie 30s 稳态：**FPS 91 → 100 / total/frame ~10.99 → ~10.00 ms / 净 ~1.0 ms**。但**子桶判据全失效**——`2d.PhaseB_serial` 1.43 → 1.45 ms（噪声内），削减分散到整个 frame 多个字段，end-to-end FPS 才显信号。**教训：下次 perf GATE 必须锁 FPS / total/frame 作主判据，子桶削减作辅助**。详见 [pvz-phase3-component-update-skipping](project_pvz_phase3_component_update_skipping.md)。

## POST-PHASE-3 DRAW BREAKDOWN & STOP DECISION (2026-05-23, end of session)

Goal of this session: 在 commit c435a57（Phase-3 完成后 100 FPS / 11000z）状态上，
用临时插桩拆解"剩下的时间在哪"，决定是否值得继续优化。**插桩已全部清理**（chrono
对污染量 ~22ms CPU / ~2ms wall）；唯一保留的代码改动 = `DrawAll` 三个
PROFILE_SCOPE 加 `{ }` block scope（修复隐藏 bug，见下）。

**11000 zombie @ c435a57 实测稳态（已扣 chrono 污染）：**

| 段 | wall | CPU_sum | 并行加速比 |
|---|---|---|---|
| 2.GOM_Update | 2.43 ms | — | — |
| ├ 2a.Phase-A (Animator 并行) | 0.88 ms | 7.05 ms | 7.5x |
| ├ 2c.PhaseB_DrainEvents | 0.00 ms | — | (drain 免费) |
| └ 2d.PhaseB_serialUpdate | 1.50 ms | — | — |
| 3.Collision_Update | 1.62 ms | — | — |
| **6.Draw_submit(par-record)** | **3.98 ms** | **34.89 ms** | **8.77x** ← #1 |
| 7.Draw_replay | 0.06 ms | — | — |

**Draw 内部三段近均分（thread_local 拆解，已清理）：**
- `6c.submit` (DrawTextureMatrix 实际调用): ~10.1 ms CPU (29%)
- `6d.interp` (GetInterpolatedTransform 关键帧插值): ~11.8 ms CPU (34%)
- 其他 (4× cosf/sinf + glm::mat4 ctor + 子动画递归 + 框架): ~13.0 ms CPU (37%)

**关键 insight（非显然的发现）：**
1. **三段均分**：没有任何一项 > 35%；典型的"优化已饱和"信号。
2. **Draw 并行 8.77x > Animator 7.5x**：Graphics 的 per-worker SetWorkerSlot
   + per-worker batch buffer 设计扩展性比预期好，几乎无锁争用 —— 不要担心。
3. **单位成本接近理论极限**：interp ~72 ns/call，submit ~91 ns/call，
   "其他" ~79 ns/track。手写紧凑代码的合理水平在 50-100 ns/call。
4. **trig 占 transform 约 6 ms CPU**：4 个 cosf/sinf × 165k tracks × ~10 ns。
   SSE sincos 能砍 50%，但仅 ~0.4 ms wall 收益。
5. **Particles_Update 已不在采样里**——上一轮 Phase-2 笔记里"1.Particles_Update
   6.16ms 是 #1"已 stale，本轮看不到，可能 phase-3 改动间接波及或场景不同。

**Stop decision：** 当前 100 FPS / 10000+ 僵尸是 PvZ 原版 (~30 FPS) 的 3x，
工程时间不如花在新功能。CPU 层最多再榨 1-2 ms（SSE sincos + cache lastFrameIdx），
ROI 远低于过去 phase-2/3 的回报曲线。

## NEXT POTENTIAL LEVER: GPU INSTANCING (未启动, ROI 中等)

**何时启动：** 仅当未来需求是 144 FPS+ 或更多僵尸（20000+）。当前不要主动碰。

**预期收益：**
- 节省 ~25 ms CPU 串行等价（transform + framework 那部分） → 释放 8.77x 并行后 ≈ **~2.8 ms wall**
- 帧时间 10 → 7.2 ms，FPS 100 → 138；CPU 利用率显著下降

**工程成本：1-2 周**，改动范围：
1. reanim 关键帧数据 → SSBO（11000 instance × ~15 tracks × frame data）
2. vertex shader 在 GPU 端做 frame 插值 + 三角函数 + 矩阵合成（GPU 强项，几乎免费）
3. 11000 instance 改 indirect draw / instanced draw，替代 11000 次 batch submit
4. 子动画递归（手臂、旗帜、铁桶等附加节点）改 GPU instancing 模型
5. **frame event readback 同步点** —— CPU 侧仍依赖"穿过某帧"信号（射豌豆、僵尸吃植物动作触发），
   需要 GPU→CPU readback 或 CPU 镜像帧索引；这是隐藏的最大复杂度

**前置验证：** 启动前用 RenderDoc/Tracy 验证 Draw 单位成本仍是 ~3.17 µs/zombie；
若 reanim 资源变化（更多图层 / 更多子节点）该数字会变，估算的 2.8 ms 收益也会变。

## 副产品 BUG（已修复，永久保留）
`GameObjectManager::DrawAll` 原代码三个 PROFILE_SCOPE (4.Draw_sort, 6.Draw_submit,
7.Draw_replay) 都声明在**函数作用域顶层**，RAII 析构都在函数末尾 → 嵌套层叠：
- `4.Draw_sort` 实际测量整个 DrawAll → 虚报 ~4 ms（真实 sort 0.01 ms，mSortDirty 守卫工作良好）
- `6.Draw_submit` 实际测量 sort 之后所有 draw
- `7.Draw_replay` 才是唯一相对正确的

修复 = 给每个 PROFILE_SCOPE 加 `{ }` block scope。零运行时开销。未来读这三个桶不再被骗。

## LATER (residual cleanup tasks, 与 perf 无强相关)
- Remove temporary scaffolding: Profiler.h (+#include/PROFILE_SCOPE) AND the
  kProbeSkipBatchAccum probe in Graphics.cpp（如果还在）。
- `AnimatedObject::AttachAnimatorToTrack` 项目内零调用（phase-2 Task 1 audit 副产物），
  清理候选。
- Evaluate off-screen culling / overdraw（如果未来 GPU side 成为瓶颈）。

Build workflow: user builds manually in Visual Studio 2026 (CLAUDE.md forbids me
building); I deliver code, user runs and pastes profile numbers. See [collaboration-style](feedback_collaboration_style.md).
