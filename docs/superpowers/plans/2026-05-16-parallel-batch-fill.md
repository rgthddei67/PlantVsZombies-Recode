# Parallel Per-Object Batch Fill — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut the serial `6.Draw_submit` phase (~9.4ms, ~57% of the 16.6ms frame at 4400 zombies) by moving the ~30MB/frame batch-buffer construction off the per-command serial hot loop into the already-parallel prepare phase, leaving the serial path only a few per-object bulk appends.

**Architecture:** Each `Animator` builds, during the already-parallel `PrepareForDraw`, its own raw `BatchVertex`/`glm::mat4` arrays (atlas UV remap, color normalize, blend resolve, 6-vertex construction — all pure CPU, per-object, no shared/GL state). The serial submit then makes ONE bulk append per object (≈5000 iterations) instead of per command (≈90811 iterations): bulk-copy matrices, bulk-copy vertices while rebasing `matrixIndex` and patching `texIndex`, respecting the existing 32768-matrix flush boundary. A measurement gate after Task 4 decides whether this is sufficient or whether the riskier full cross-core fill (separate follow-up plan) is needed.

**Tech Stack:** C++17, custom OpenGL batch renderer (`Graphics`), custom skeletal `Animator`, a `ThreadPool` already driving `GameObjectManager`'s parallel `PrepareForDraw`.

**Verification model (READ THIS FIRST):** This project has **no unit-test harness**. CLAUDE.md forbids the assistant from building. The verification loop for every task that changes runtime behavior is:
1. Assistant delivers the code edit.
2. **User** builds in Visual Studio 2026 (F7) and runs the 4400-zombie stress test.
3. User lets it reach **steady state (warmed up)** and pastes the `==== FRAME PROFILE ====` block.
4. Assistant compares against the documented baseline before proceeding.

"Expected" blocks below describe the steady-state FRAME PROFILE deltas to look for, not test output. Do not fabricate or assume numbers — wait for the pasted profile.

**Baseline (same-session, post-Experiment-1, vsync off, measured 2026-05-16):**
```
total / frame        :   16.65 ms  (60.1 FPS)
5.Draw_prepare(par)  :    0.85 ms
6.Draw_submit(serial):    9.44 ms
9.SwapWindow(vsync)  :    2.26 ms
FlushBatch calls     :    4.0 /frame  (~90808 sprites/frame)
```
Diagnostic probe (skip accumulation write only): `6.Draw_submit` collapsed to **3.86 ms** ⇒ ~5.6ms of the 9.4ms is the serial accumulation write. That ~5.6ms is the target.

**Scope:** This plan covers ONLY the per-object bulk variant (Design A) + its measurement gate. The full cross-core preallocated-offset fill (Design B) is intentionally a separate, conditional follow-up plan, written only if Task 4's numbers show Design A is bandwidth-bound rather than push_back-overhead-bound. Each plan must produce working, measurable software on its own; Design A does.

---

## File Structure

- `PlantVsZombies/Reanimation/Animator.h` — add per-object batch buffers + a builder declaration. Owns the parallel-built fragment for one animator.
- `PlantVsZombies/Reanimation/Animator.cpp` — build the fragment in `PrepareCommands` (parallel); consume it via the new Graphics entry in `Draw` (serial); keep the existing per-command path as an intact fallback.
- `PlantVsZombies/Graphics.h` — declare one new public entry `AppendObjectBatch`; batch internals stay private.
- `PlantVsZombies/Graphics.cpp` — implement `AppendObjectBatch`: flush-boundary-correct per-object bulk append with `matrixIndex` rebase and `texIndex` patch.

No file is restructured; all four already exist and follow the codebase's large-file convention.

---

## Task 1: Per-object parallel-built batch fragment in Animator

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.h` (member block near `mCachedCommands`)
- Modify: `PlantVsZombies/Reanimation/Animator.cpp` (`PrepareCommands`)

Build, in the parallel `PrepareCommands`, the object's own `BatchVertex`/`glm::mat4` arrays. The heavy CPU work (atlas remap, `NormalizeColor`, blend resolve, 6-vertex construction) moves here. A homogeneity guard leaves the fragment empty for the rare multi-texture animator so it falls back to the original per-command path.

- [ ] **Step 1: Add fragment members to Animator.h**

In `PlantVsZombies/Reanimation/Animator.h`, find:

```cpp
    std::vector<AnimDrawCommand> mCachedCommands;  ///< PrepareCommands() 的缓存结果
```

Replace with:

```cpp
    std::vector<AnimDrawCommand> mCachedCommands;  ///< PrepareCommands() 的缓存结果

    // 并行预构造的本对象批处理片段（容量跨帧复用；只在 PrepareCommands 写、Draw 读）。
    // mFragVerts.size() == mFragMatrices.size()*6。mFragTexId 为本对象单一图集纹理 id。
    // mFragHomogeneous=false 时放弃快路（多纹理动画），Draw 回退到逐命令路径。
    std::vector<BatchVertex> mFragVerts;
    std::vector<glm::mat4>   mFragMatrices;
    GLuint                   mFragTexId = 0;
    bool                     mFragHomogeneous = true;
```

- [ ] **Step 2: Build the fragment in PrepareCommands**

In `PlantVsZombies/Reanimation/Animator.cpp`, find:

```cpp
void Animator::PrepareCommands(float baseX, float baseY, float Scale) {
    mCachedCommands.clear();
    if (!mReanim) return;
    CollectDrawCommands(mCachedCommands, baseX, baseY, Scale);
}
```

Replace with:

```cpp
void Animator::PrepareCommands(float baseX, float baseY, float Scale) {
    mCachedCommands.clear();
    mFragVerts.clear();          // clear() 保留容量，稳态不再分配
    mFragMatrices.clear();
    mFragTexId = 0;
    mFragHomogeneous = true;
    if (!mReanim) return;
    CollectDrawCommands(mCachedCommands, baseX, baseY, Scale);

    // ---- 并行阶段（经 AnimatedObject::PrepareForDraw 由线程池调用）：把图集重映射 /
    //      颜色归一化 / 混合解析 / 6 顶点构造 全部在此完成。只碰自身成员，无共享/GL 状态。----
    const size_t n = mCachedCommands.size();
    mFragMatrices.reserve(n);
    mFragVerts.reserve(n * 6);

    GLuint firstTexId = 0;
    bool   haveTex = false;

    for (const auto& cmd : mCachedCommands) {
        const GLTexture* tex = cmd.texture;
        if (!tex) continue;                       // 与 DrawTextureMatrix 的 null 提前返回一致

        const GLTexture* bindTex = tex;
        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
        if (tex->atlasPage) {
            bindTex = tex->atlasPage;
            u0 = tex->aU0; v0 = tex->aV0; u1 = tex->aU1; v1 = tex->aV1;
        }
        const GLuint texId = bindTex->id;

        if (!haveTex) { firstTexId = texId; haveTex = true; }
        else if (texId != firstTexId) {
            // 多纹理动画：放弃快路，整体回退（极少见——按 reanim 图集页通常单一纹理）
            mFragHomogeneous = false;
            mFragVerts.clear();
            mFragMatrices.clear();
            return;
        }

        glm::vec4 c = NormalizeColor(cmd.color);
        // Animator 命令的 blendMode 恒为 Alpha/Add；None 兜底为 Alpha（与 Graphics 默认一致）
        BlendMode actual = (cmd.blendMode == BlendMode::None) ? BlendMode::Alpha : cmd.blendMode;
        const float bm = (actual == BlendMode::Add) ? 1.0f : 0.0f;

        // 最终矩阵：pivot=0；Animator 绘制时变换栈顶为单位阵（实验1），故最终矩阵即 transform。
        // 串行 AppendObjectBatch 仍会在栈顶非单位时统一再乘，保证其它调用方正确。
        mFragMatrices.push_back(cmd.transform);

        // matrixIndex 先填本片段内局部下标（串行阶段 += base 重基）；texIndex 先填 0（串行 patch）。
        const GLuint localMat = static_cast<GLuint>(mFragMatrices.size() - 1);
        mFragVerts.push_back({ 0.0f, 1.0f, u0, v1, 0u, localMat, c.r, c.g, c.b, c.a, bm });
        mFragVerts.push_back({ 1.0f, 1.0f, u1, v1, 0u, localMat, c.r, c.g, c.b, c.a, bm });
        mFragVerts.push_back({ 1.0f, 0.0f, u1, v0, 0u, localMat, c.r, c.g, c.b, c.a, bm });
        mFragVerts.push_back({ 0.0f, 1.0f, u0, v1, 0u, localMat, c.r, c.g, c.b, c.a, bm });
        mFragVerts.push_back({ 0.0f, 0.0f, u0, v0, 0u, localMat, c.r, c.g, c.b, c.a, bm });
        mFragVerts.push_back({ 1.0f, 0.0f, u1, v0, 0u, localMat, c.r, c.g, c.b, c.a, bm });
    }

    mFragTexId = firstTexId;
    // mCachedCommands 已转换完毕；释放以减半瞬时内存（回退路径在未 Prepare 时自行重算）。
    mCachedCommands.clear();
}
```

- [ ] **Step 3: Verify it compiles (user, in VS)**

This task alone does not change rendering (the fragment is built but not yet consumed; `Draw` is unchanged so it still uses the old per-command path via `mCachedCommands`... which is now cleared at the end of `PrepareCommands`).

> **CORRECTNESS NOTE for the executor:** After Step 2, `PrepareCommands` clears `mCachedCommands`, so the old `Draw` (still in place until Task 3) would hit its `else` branch and recompute via `CollectDrawCommands(localCommands,...)`. Rendering stays correct (recompute yields identical commands) but Task 1 in isolation will be *slower*, not faster. **Do not measure performance after Task 1.** Tasks 1–3 are one atomic change; only measure at Task 4. This note exists so the executor does not panic at an interim slowdown or "fix" it.

Run: user builds in Visual Studio (F7).
Expected: Compiles clean. No visual change, no crash when running normally (not the stress test yet).

- [ ] **Step 4: Commit checkpoint (optional, user-driven)**

Commits in this repo are user-driven (CLAUDE.md: user handles builds; assistant commits only when asked). If the user wants a checkpoint:

```bash
git add PlantVsZombies/Reanimation/Animator.h PlantVsZombies/Reanimation/Animator.cpp
git commit -m "perf: build per-object batch fragment in parallel PrepareCommands"
```

---

## Task 2: Flush-boundary-correct per-object bulk append in Graphics

**Files:**
- Modify: `PlantVsZombies/Graphics.h` (public method declaration, after `FlushBatch();`)
- Modify: `PlantVsZombies/Graphics.cpp` (new method, placed just before `void Graphics::SetBatchMode(bool enable) {`)

One public entry point; all batch internals (`m_batchVertices`, `m_batchMatrices`, `BindTexture`, `FlushBatch`, `CheckBatch`, `m_matrixBatchLimit`, `m_transformStack`, `m_transformIsIdentity`) stay private.

- [ ] **Step 1: Declare the public entry in Graphics.h**

In `PlantVsZombies/Graphics.h`, find:

```cpp
	void FlushBatch();

	/**
	 * @brief 设置批处理模式开关。切换时会自动提交当前批次。
```

Replace with:

```cpp
	void FlushBatch();

	/**
	 * @brief 追加一个对象在并行阶段预构造好的批处理片段（每对象一次 bulk 追加，
	 *        代替每命令一次）。verts 的 matrixIndex 为片段内局部下标，本函数按
	 *        当前批次基址重基；texIndex 由 texId 经 BindTexture 解析后回填。
	 *        逐对象保持与 DrawTextureMatrix 一致的 flush 边界语义。
	 *        前置条件：matCount <= m_matrixBatchLimit（单个动画 sprite 数远小于此）。
	 * @param texId    本对象单一图集纹理 id
	 * @param mats     片段矩阵数组
	 * @param matCount 矩阵数（= 命令数）
	 * @param verts    片段顶点数组（长度 = matCount*6），matrixIndex 为局部下标
	 */
	void AppendObjectBatch(GLuint texId, const glm::mat4* mats, int matCount,
		const BatchVertex* verts);

	/**
	 * @brief 设置批处理模式开关。切换时会自动提交当前批次。
```

- [ ] **Step 2: Implement AppendObjectBatch in Graphics.cpp**

In `PlantVsZombies/Graphics.cpp`, find:

```cpp
void Graphics::SetBatchMode(bool enable) {
```

Insert the following function definition immediately *before* that line:

```cpp
void Graphics::AppendObjectBatch(GLuint texId, const glm::mat4* mats, int matCount,
	const BatchVertex* verts) {
	if (matCount <= 0) return;

	// 单个对象的 sprite 数远小于 m_matrixBatchLimit；若极端超限，退回逐命令更安全。
	if (matCount > m_matrixBatchLimit) {
		for (int i = 0; i < matCount; ++i) {
			AddVertices(verts + i * 6, 6);   // 退化路径：极少触发，正确优先
		}
		(void)mats; (void)texId;
		return;
	}

	// 顺序与 DrawTextureMatrix 一致且自洽：
	//  1) 边界 flush（保证本对象矩阵整体落在同一段，绝不跨 flush）
	//  2) BindTexture（在 flush 之后，避免 texIndex 因 flush 清空 m_batchTextures 而失效）
	//  3) base 取值（必须在 1/2 之后）
	//  4) bulk 追加矩阵
	//  5) bulk 追加顶点，matrixIndex += base，texIndex = ti
	//  6) CheckBatch（与旧路径行为对齐）
	if (static_cast<int>(m_batchMatrices.size()) + matCount > m_matrixBatchLimit) {
		FlushBatch();
	}

	const int ti = BindTexture(texId);

	const GLuint base = static_cast<GLuint>(m_batchMatrices.size());

	m_batchMatrices.insert(m_batchMatrices.end(), mats, mats + matCount);

	const int vertCount = matCount * 6;
	const size_t vbase = m_batchVertices.size();
	m_batchVertices.resize(vbase + static_cast<size_t>(vertCount));
	BatchVertex* dst = m_batchVertices.data() + vbase;
	const GLuint tiu = static_cast<GLuint>(ti);
	for (int i = 0; i < vertCount; ++i) {
		BatchVertex v = verts[i];
		v.matrixIndex += base;   // 局部下标 -> 全局下标
		v.texIndex = tiu;
		dst[i] = v;
	}

	CheckBatch();
}
```

> **Design note for the executor:** Pre-`resize` + sequential indexed write deliberately replaces 90811×6 `push_back` (each with a capacity branch) with one `resize` + a branchless sequential loop per object. The `matrixIndex += base` and `texIndex` patch are the only unavoidable serial per-vertex touches; everything heavy (matrix math, color normalize, vertex layout) was done in parallel in Task 1. `m_batchMatrices.insert` is a bulk copy. This is the crux of the speedup; do not "simplify" it back into per-command calls.

- [ ] **Step 3: Verify it compiles (user, in VS)**

`AppendObjectBatch` is defined but not yet called (Task 3 wires it). Behavior unchanged.

Run: user builds in Visual Studio (F7).
Expected: Compiles clean. No behavior change.

- [ ] **Step 4: Commit checkpoint (optional, user-driven)**

```bash
git add PlantVsZombies/Graphics.h PlantVsZombies/Graphics.cpp
git commit -m "perf: add Graphics::AppendObjectBatch (per-object bulk batch append)"
```

---

## Task 3: Wire the fast path in Animator::Draw with intact fallback

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.cpp` (`Draw`)

When the parallel fragment exists and is homogeneous, do one bulk append and return. Otherwise the original per-command path runs unchanged (recomputing commands if needed).

- [ ] **Step 1: Add the fast path to Animator::Draw**

In `PlantVsZombies/Reanimation/Animator.cpp`, find:

```cpp
void Animator::Draw(Graphics* g, float baseX, float baseY, float Scale) {
    if (!mReanim || !g) return;

    // 优先使用 PrepareCommands() 预计算的缓存（多线程模式）
    // 若缓存为空则即时计算（单线程回退）
    std::vector<AnimDrawCommand> localCommands;
```

Replace with:

```cpp
void Animator::Draw(Graphics* g, float baseX, float baseY, float Scale) {
    if (!mReanim || !g) return;

    // 快路：并行阶段已构造本对象的同质批处理片段，串行此处仅一次 bulk 追加。
    // 矩阵已是最终值；AppendObjectBatch 内部读栈顶（含实验1单位阵快路），无需 Push/PopTransform。
    if (mFragHomogeneous && !mFragVerts.empty()) {
        g->AppendObjectBatch(mFragTexId,
            mFragMatrices.data(), static_cast<int>(mFragMatrices.size()),
            mFragVerts.data());
        mFragVerts.clear();
        mFragMatrices.clear();
        return;
    }

    // ---- 回退路径：未 Prepare 或多纹理动画，按原逐命令逻辑（行为与改造前完全一致）----
    // 优先使用 PrepareCommands() 预计算的缓存（多线程模式）
    // 若缓存为空则即时计算（单线程回退）
    std::vector<AnimDrawCommand> localCommands;
```

> **Note for the executor:** The rest of `Draw` (the `mCachedCommands`/`localCommands` selection, `PushTransform`/loop/`PopTransform`, final `mCachedCommands.clear()`) is left exactly as-is. Only the early fast-path block is inserted. The fallback still uses the stack-identity `PushTransform`; `AppendObjectBatch` instead consults the transform stack itself for the non-identity case — see correctness review below.

> **Transform-stack correctness:** Old behavior = `PushTransform(identity)` then `DrawTextureMatrix` used `m_transformStack.back() * transform` (with Experiment 1's identity fast-path skipping the multiply when the top is identity, which it always is during the GameObjectManager submit loop — that loop pushes only clip rects, never transforms). `AppendObjectBatch` stores `cmd.transform` as the matrix and the GameObjectManager submit loop's stack top is identity, so the matrix is final and correct. **This task does NOT add a non-identity multiply inside `AppendObjectBatch`** (it intentionally has none): the only callers that draw an Animator under a pushed non-identity Graphics transform would be non-GameObjectManager UI paths, which do not run `PrepareForDraw` and therefore hit the empty-fragment fallback (correct). If a future caller pushes a transform AND pre-builds a fragment, that is out of scope and must be revisited then. Document, do not over-engineer now (YAGNI).

- [ ] **Step 2: Build (user, in VS)**

Run: user builds in Visual Studio (F7).
Expected: Compiles clean.

- [ ] **Step 3: Visual correctness check (user) — BEFORE measuring performance**

Run the game normally (a real level, then the 4400-zombie stress scene). Verify:
- Zombies, plants, projectiles, sun render correctly (no missing/garbled/mis-tinted sprites, correct draw order, additive glow effects still additive).
- `FlushBatch calls` in the profile is still ~4/frame and `~90808 sprites/frame` (semantics preserved — NOT 0, NOT wildly different).

Expected: Visually identical to baseline. If sprites are missing, mis-ordered, or `FlushBatch`/sprite counts changed materially, STOP — flush-boundary or rebase logic is wrong; report the symptom before continuing.

- [ ] **Step 4: Commit checkpoint (optional, user-driven)**

```bash
git add PlantVsZombies/Reanimation/Animator.cpp
git commit -m "perf: route Animator::Draw through per-object bulk append fast path"
```

---

## Task 4: Measurement gate (decides whether Design A suffices)

**Files:** none (measurement + decision only).

- [ ] **Step 1: User runs the steady-state stress profile**

User runs the 4400-zombie stress test, lets it warm up to steady state, pastes the full `==== FRAME PROFILE ====` block.

- [ ] **Step 2: Compare against baseline**

Fill this table from the pasted profile:

| Phase | Baseline | After Design A | Note |
|---|---|---|---|
| `6.Draw_submit(serial)` | 9.44 ms | ? | primary target |
| `5.Draw_prepare(par)` | 0.85 ms | ? | expected to rise (work moved here, but parallel) |
| `total / frame` (FPS) | 16.65 ms (60.1) | ? | net win |
| `9.SwapWindow` | 2.26 ms | ? | control — should be ~unchanged |
| `FlushBatch / sprites` | 4.0 / ~90808 | ? | MUST stay ~4 / ~90808 (no regression) |

- [ ] **Step 3: Apply the decision rule (assistant)**

- **`FlushBatch` ≠ ~4 or sprites ≠ ~90808, or visual artifacts:** correctness regression. Revert Task 3 first (restores old path), diagnose rebase/flush-boundary logic. Do not proceed.
- **`6.Draw_submit` ≤ ~3ms:** Design A captured the win. Proceed to Task 5 (cleanup). Design B NOT needed (YAGNI).
- **`6.Draw_submit` in ~3–6ms:** partial win, push_back-overhead was dominant but residual remains. Keep Design A. Decide with user whether the gain is enough to stop, or to write the Design B follow-up plan for the remaining bandwidth-bound portion.
- **`6.Draw_submit` ≈ unchanged (~8–9ms):** Design A insufficient ⇒ the cost is raw serial memory bandwidth, not push_back overhead. Keep or revert Design A per its own merit, and write the **separate** follow-up plan `docs/superpowers/plans/YYYY-MM-DD-cross-core-batch-fill.md` (true cross-core preallocated-offset fill: prefix-sum per-object ranges, threads write disjoint slots of preallocated global buffers, serial = bulk upload + segmented draw). That plan is out of scope here by design.

- [ ] **Step 4: Record outcome in project memory**

Update `C:\Users\ZJ\.claude\projects\D--PVZ-PlantsVsZombies-PlantVsZombies\memory\project_pvz_perf_optimization.md` with the measured numbers and which decision-rule branch fired (keep / partial / escalate-to-Design-B), so the investigation survives a session break.

---

## Task 5: Cleanup (only if Design A is kept and deemed sufficient)

**Files:**
- Modify: `PlantVsZombies/Graphics.cpp` (remove the temporary probe)
- (Profiler scaffolding removal is tracked separately in project memory's "LATER"; do NOT remove it here — it is still the measurement channel for any Design B follow-up.)

- [ ] **Step 1: Remove the diagnostic probe**

In `PlantVsZombies/Graphics.cpp`, delete the probe block (the `// [临时诊断探针]` comment banner through `static constexpr bool kProbeSkipBatchAccum = false;`) and the two guard lines:
- In `Graphics::AddMatrix`: delete `	if (kProbeSkipBatchAccum) return 0;   // [PROBE] 跳过累积写入，隔离内存带宽成本`
- In `Graphics::AddVertices`: delete `	if (kProbeSkipBatchAccum) return;     // [PROBE] 跳过累积写入，隔离内存带宽成本`

- [ ] **Step 2: Build + final steady-state profile (user, in VS)**

Run: user builds (F7), runs 4400-zombie stress, pastes steady-state FRAME PROFILE.
Expected: Identical to Task 4 numbers (probe removal is a no-op when it was already `false`); confirms no accidental dependency on the probe.

- [ ] **Step 3: Commit (user-driven)**

```bash
git add PlantVsZombies/Graphics.cpp
git commit -m "perf: remove temporary Draw_submit diagnostic probe"
```

---

## Self-Review

**1. Spec coverage:** Spec = "move ~30MB/frame batch construction off the per-command serial loop into the parallel prepare, leaving serial only per-object bulk appends, measure, decide." Task 1 = parallel fragment build (atlas/color/blend/verts moved off serial). Task 2 = serial per-object bulk append (replaces per-command push_back). Task 3 = wiring + intact fallback + correctness review. Task 4 = measurement gate + decision rule (covers the "decide / escalate to Design B" requirement). Task 5 = cleanup. Cross-core fill (Design B) explicitly scoped out to a conditional follow-up plan, consistent with the skill's "break multi-subsystem specs into separate plans; each produces working software." No gaps.

**2. Placeholder scan:** No "TBD/TODO/handle edge cases/similar to Task N". Every code step shows the full replacement code. Task 4/5 are inherently measurement/cleanup (no code placeholders; Task 5 references exact text to delete).

**3. Type/name consistency:** Members `mFragVerts` (`std::vector<BatchVertex>`), `mFragMatrices` (`std::vector<glm::mat4>`), `mFragTexId` (`GLuint`), `mFragHomogeneous` (`bool`) — declared in Task 1, consumed identically in Task 3. `Graphics::AppendObjectBatch(GLuint texId, const glm::mat4* mats, int matCount, const BatchVertex* verts)` — signature identical in Graphics.h (Task 2 Step 1), Graphics.cpp (Task 2 Step 2), and call site (Task 3 Step 1). `BatchVertex` aggregate init order matches the struct (`x,y,u,v,texIndex,matrixIndex,r,g,b,a,blendMode`) and the original `DrawTextureMatrix` vertex layout. `NormalizeColor`, `BlendMode`, `GLTexture` (`atlasPage/aU0/aV0/aU1/aV1/id`), `AnimDrawCommand` (`texture/blendMode/color/transform`) are existing types used consistently. Flush-boundary ordering in Task 2 matches the `DrawTextureMatrix` side-effect order documented inline. Consistent.
