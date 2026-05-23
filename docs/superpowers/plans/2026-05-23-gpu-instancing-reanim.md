# GPU Instancing for Reanim Sprites Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move per-sprite `mat4` construction (trig + ctor) and 6-vertex inflation from CPU to GPU vertex shader, replacing the current `BatchVertex × 6 + SSBO mat4` path with a per-instance record buffer for reanim sprites. Target: 11000-zombie wall frame time 10.0 ms → 7.2-7.7 ms, FPS 100 → 130-138.

**Architecture:** Add a second draw pipeline (`pipeInstAlpha` / `pipeInstAdd`) that consumes a buffer of `InstanceRecord` structs (~48 B each, SoA transform params + atlas UV + color + tex index). Vertex shader expands `gl_VertexIndex` 0..5 into a unit-quad corner + UV, reads its instance's params, computes `mat4` on GPU. Existing batch pipeline stays for non-reanim draws (FillRect / DrawText / DrawLine), text-cache LRU, and the ~1% of plants with child animators (豌豆 stem→head). Reanim with no `mAttachedReanims` goes through instance path; with children falls back to original `DrawInternal`.

**Tech Stack:** Vulkan 1.3 (dynamic rendering, push constants, bindless texture array via `GL_EXT_nonuniform_qualifier`), GLSL 450, persistent-mapped host-visible buffers (existing pattern), worker-thread slice writes (existing `VkWorkerSlice` pattern).

---

## Pre-Reading (do once before Task 0)

The implementer should skim these files to ground later steps in concrete signatures:

- `PlantVsZombies/Graphics.h` — see `BatchVertex` (44 B, lines 58-65), `VkWorkerSlice` (155-165), `WorkerRecord` (172-194), pipeline pointers in PIMPL `VulkanGraphicsState` (Graphics.cpp:55-65)
- `PlantVsZombies/Graphics.cpp` — `EmitDrawRange` helper (~line 102-130), `FlushBatch` (~524-605), `ReplayAndEndParallel` (~1450-1510), `InitializeVulkan` pipeline creation (~263-290), `BeginParallelRecord` slice sizing
- `PlantVsZombies/Shader/batch.vert.glsl` and `batch.frag.glsl` — current shaders (37 + 17 lines, both already shown)
- `PlantVsZombies/Reanimation/Animator.cpp:331-435` — `DrawInternal` (the hot path being replaced)
- `PlantVsZombies/Reanimation/ReanimTypes.h:15-47` — `TrackFrameTransform` (per-frame data already there) and `TrackExtraInfo`
- `PlantVsZombies/Renderer/VulkanPipeline.h` — `Desc` (Phase 2b extended with bindless desc + push const + vertex input)
- `PlantVsZombies/Profiler.h` — `PROFILE_SCOPE("name")` macro for measurement scaffolding (already exists and removed/re-added each session)

**Hard constraints to remember (from previous phases, memory `[[pvz-perf-optimization]]`):**
- Any Graphics entry that runs under `tl_record` must defer or skip (don't touch GL/cache from worker threads).
- Render order is preserved by `slot ascending + within-slot record order` — instance path must not break that.
- A/B measurements happen in the **same VS session** (user builds in Visual Studio 2026; do not attempt to build).
- GATE judges on `total / frame` + FPS, **not** sub-bucket deltas (phase-3 lesson: sub-bucket gates miss end-to-end signal).
- All commits are user-driven; this plan never commits anywhere.

---

## File Structure

**New files:**

- `PlantVsZombies/Shader/reanim_inst.vert.glsl` — per-instance vertex shader. Reads `gl_InstanceIndex` → `InstanceRecord`, expands `gl_VertexIndex 0..5` into unit-quad corner+UV, computes `mat4` from `(tx, ty, tA, tB, tC, tD)` (already-multiplied scale × cos/sin), outputs `gl_Position`, UV in atlas space, color, tex slot.
- `PlantVsZombies/Shader/reanim_inst.frag.glsl` — same as `batch.frag.glsl` (texture array sample × color). Probably a verbatim copy; kept separate so future divergence is easy.
- `PlantVsZombies/Shader/reanim_inst.vert.spv`, `reanim_inst.frag.spv` — compiled with glslangValidator (instructions in Task 1).

**Modified files:**

- `PlantVsZombies/Graphics.h` — add `InstanceRecord` struct, extend `VkWorkerSlice` with instance buffer fields (`instPtr` / `instBaseIdx` / `instCap` / `instCount`), extend `VulkanGraphicsState` PIMPL with `pipeInstAlpha` / `pipeInstAdd` and host-visible instance VBO, add `Graphics::AppendReanimInstance` (record path) and `Graphics::EmitInstanceDrawRange` (static helper, mirror of `EmitDrawRange`), add `instance_serial_buffer` for non-parallel path.
- `PlantVsZombies/Graphics.cpp` — implement everything above; modify `InitializeVulkan` to create the two new pipelines; modify `BeginParallelRecord` to also slice the instance buffer; modify `ReplayAndEndParallel` to interleave instance draw segments with batch draw segments per slot; modify `FlushBatch` (and add a parallel `FlushInstances` or interleave in same flush).
- `PlantVsZombies/Reanimation/Animator.cpp` — `DrawInternal`: when this Animator has no `mAttachedReanims` (the 99% path), build an `InstanceRecord` and call `g->AppendReanimInstance(...)` instead of computing mat4 + calling `DrawTextureMatrix`. Fallback (1% path with children) keeps current code untouched.

---

## Open Numbers Resolved at Task 0

Task 0 measurements decide three plan variables — they are referenced in later tasks by `${name}` and should be filled in before starting Task 1.

| Variable | What it is | How Task 0 sets it |
|---|---|---|
| `${childAnimFrac}` | Fraction of `DrawInternal` invocations on Animators with `mAttachedReanims.empty() == false` | Insert a temporary atomic counter pair around `DrawInternal` entry; print at frame 60 |
| `${trigCpuMs}` | CPU sum ms / 60 frames inside `DrawInternal` lines 348-357 (4× cosf/sinf) | Wrap with `PROFILE_SCOPE("Xa.trig")`; read FRAME PROFILE |
| `${mat4CpuMs}` | CPU sum ms inside `DrawInternal` lines 379-384 (mat4 ctor) | Wrap with `PROFILE_SCOPE("Xb.mat4")` |

**GATE A condition** (decided at end of Task 0): if `${childAnimFrac} > 5%`, **stop and revise plan** — children are more common than expected, fallback strategy must change. If `${trigCpuMs} + ${mat4CpuMs} < 10 ms CPU sum`, **stop and revise** — collapse target dropped, ROI insufficient.

---

### Task 0: Baseline measurement + child-animator audit

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:331-435` (insert temporary PROFILE_SCOPE blocks + atomic counters)
- Modify: `PlantVsZombies/GameApp.cpp` (verify `#include "Profiler.h"` + `Profiler::Get().EndFrame()` already wired — they are from prior phases)

**Goal:** measure baseline FRAME PROFILE on commit `c435a57` HEAD with 11000 zombies, and resolve `${childAnimFrac}` / `${trigCpuMs}` / `${mat4CpuMs}`.

- [ ] **Step 1: Add three measurement probes to `DrawInternal`**

In `Animator.cpp`, inside `DrawInternal` body, modify:

```cpp
void Animator::DrawInternal(Graphics* g, float baseX, float baseY, float Scale) const {
    if (!mReanim) return;

    // Task 0 probes — REMOVE before Task 1
    static std::atomic<int> sChildCount{0}, sTotalCount{0};
    bool hasChildren = false;
    for (const auto& e : mExtraInfos) {
        if (!e.mAttachedReanims.empty()) { hasChildren = true; break; }
    }
    sTotalCount++;
    if (hasChildren) sChildCount++;
    if ((sTotalCount.load() & 0xFFFF) == 0) {
        std::printf("[Task0] child=%d total=%d frac=%.3f%%\n",
            sChildCount.load(), sTotalCount.load(),
            100.0 * sChildCount.load() / sTotalCount.load());
    }

    static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    // ... existing code ...
```

Around lines 348-353 (the 4× cosf/sinf block), wrap with `PROFILE_SCOPE("Xa.trig")`:

```cpp
{
    PROFILE_SCOPE("Xa.trig");
    float angleX = -transform.kx * DEG_TO_RAD;
    float angleY = -transform.ky * DEG_TO_RAD;
    float cosX = cosf(angleX);
    float sinX = sinf(angleX);
    float cosY = cosf(angleY);
    float sinY = sinf(angleY);
    float tA = cosX * transform.sx;
    float tB = -sinX * transform.sx;
    float tC = sinY * transform.sy;
    float tD = cosY * transform.sy;
}
```

Around lines 379-384 (the `glm::mat4 mat(...)` block), wrap with `PROFILE_SCOPE("Xb.mat4")`. Be careful: `tA/tB/tC/tD` must remain in outer scope so the mat4 block can read them — refactor so the probes are inner blocks but the local floats outlive both:

```cpp
float tA, tB, tC, tD;
{
    PROFILE_SCOPE("Xa.trig");
    // compute tA..tD as above (declare without re-shadowing)
}
// ... shouldDrawSelf logic ...
{
    PROFILE_SCOPE("Xb.mat4");
    glm::mat4 mat(...);
    // hand off mat to DrawTextureMatrix calls below — keep `mat` in outer scope too
}
```

**Note:** `PROFILE_SCOPE` itself contributes ~4.6 ms self-pollution at 165k calls/frame (from phase-3 lessons). Treat absolute numbers as upper bounds; deltas between probes are what matter.

- [ ] **Step 2: Build in VS, run warmed-up 11000-zombie scene, paste FRAME PROFILE**

User runs in VS (`F7` build, `F5` run). Need: 30 seconds steady state, then a `FRAME PROFILE` block. Record `Xa.trig` and `Xb.mat4` CPU sum values. Record the `[Task0]` print `frac=…%` line. Record `total / frame`, `2.GOM_Update`, `6.Draw_submit(par-record)`.

- [ ] **Step 3: Apply GATE A**

Fill in `${childAnimFrac}`, `${trigCpuMs}`, `${mat4CpuMs}` at the top of this plan. Decide:

- `${childAnimFrac}` ≤ 5% **and** `${trigCpuMs} + ${mat4CpuMs}` ≥ 10 ms → PROCEED to Task 1.
- Otherwise → STOP, write findings into `[[pvz-perf-optimization]]` memory, do not start Task 1.

- [ ] **Step 4: Remove probes (REQUIRED before Task 1)**

Restore `DrawInternal` to pristine `c435a57` HEAD state — the probes were diagnostic only and self-pollute downstream measurements. Verify by `git diff Animator.cpp` shows zero lines changed.

---

### Task 1: Design `InstanceRecord` + write `reanim_inst.vert.glsl`

**Files:**
- Create: `PlantVsZombies/Shader/reanim_inst.vert.glsl`
- Create: `PlantVsZombies/Shader/reanim_inst.frag.glsl`
- Modify: `PlantVsZombies/Graphics.h` (add `InstanceRecord` struct after `BatchVertex`)

**Goal:** lock the per-instance data layout and the shader that consumes it. No CPU code that produces records yet — that's Task 4-5.

- [ ] **Step 1: Add `InstanceRecord` to `Graphics.h`**

Insert immediately after `BatchVertex` (around line 65):

```cpp
/**
 * @brief Per-sprite instance record consumed by reanim_inst.vert.glsl.
 *        Vertex shader expands gl_VertexIndex 0..5 to a unit-quad corner+UV,
 *        applies the 2x3 affine (tA tB tC tD tx ty), samples atlas UV range
 *        (u0 v0 u1 v1), multiplies vertex color.
 *        Stride 48 B (16-byte aligned for std140-ish SSBO).
 */
struct InstanceRecord {
    float tA, tB, tC, tD;   // 16 B — pre-multiplied 2x2: row0=(tA, tC), row1=(tB, tD)
    float tx, ty;           // 8 B  — translation in world space (already includes baseX + Scale)
    float u0, v0;           // 8 B  — atlas UV top-left
    float u1, v1;           // 8 B  — atlas UV bottom-right
    uint32_t texSlot;       // 4 B  — bindless texture index
    uint32_t colorRGBA8;    // 4 B  — packed RGBA8 (r=lsb, a=msb), pre-tinted
};
static_assert(sizeof(InstanceRecord) == 48, "InstanceRecord must be 48 bytes");
```

**Why this layout:**
- `tA/tB/tC/tD` not `mat4`: 4 floats vs 16, vertex shader builds the 2D affine inline.
- `u0/v0/u1/v1`: atlas UV bbox; vertex shader picks corner by `gl_VertexIndex`. Avoids storing 4×UVs.
- `colorRGBA8` packed: 4 B vs 16 B for `vec4`. Vertex shader unpacks.
- Total 48 B = 7x reduction vs current path (6×`BatchVertex` 44B + 1×`mat4` 64B = 328 B per sprite).

- [ ] **Step 2: Write `reanim_inst.vert.glsl`**

```glsl
#version 450

// GPU instancing vertex shader for reanim sprites.
//
// Inputs:
//   gl_InstanceIndex → InstanceRecord in SSBO at set=0 binding=1
//   gl_VertexIndex 0..5 → quad corner + UV (two triangles, CCW)
//
// set=0 binding=1   readonly buffer of InstanceRecord (host-mapped)
// push_constant     projView mat4
//
// Triangle 0: 0,1,2 = TL, BL, BR     (CCW with Y-down screen)
// Triangle 1: 0,2,3 = TL, BR, TR
//
// Vertex index → corner mapping:
//   idx 0 → (0,0)  TL    UV (u0,v0)
//   idx 1 → (0,1)  BL    UV (u0,v1)
//   idx 2 → (1,1)  BR    UV (u1,v1)
//   idx 3 → (0,0)  TL    UV (u0,v0)
//   idx 4 → (1,1)  BR    UV (u1,v1)
//   idx 5 → (1,0)  TR    UV (u1,v0)
//
// (Note: corner is in unit quad; sprite size is baked into tA..tD on CPU side as
//  tA*=width etc. when building the InstanceRecord, see Task 5.)

struct InstanceRecord {
    float tA, tB, tC, tD;
    float tx, ty;
    float u0, v0;
    float u1, v1;
    uint  texSlot;
    uint  colorRGBA8;
};

layout(set = 0, binding = 1) readonly buffer InstanceBlock {
    InstanceRecord inst[];
} instances;

layout(push_constant) uniform PC {
    mat4 projView;
} pc;

layout(location = 0) out vec2      vUV;
layout(location = 1) out vec4      vColor;
layout(location = 2) flat out uint vTex;

void main() {
    InstanceRecord r = instances.inst[gl_InstanceIndex];

    // gl_VertexIndex → unit-quad corner + UV
    // bit 0 → right (u1, x=1), bit 1 → bottom (v1, y=1), except idx 3..5 reorder for tri2
    vec2 corner;
    vec2 uv;
    switch (gl_VertexIndex) {
        case 0: corner = vec2(0.0, 0.0); uv = vec2(r.u0, r.v0); break;
        case 1: corner = vec2(0.0, 1.0); uv = vec2(r.u0, r.v1); break;
        case 2: corner = vec2(1.0, 1.0); uv = vec2(r.u1, r.v1); break;
        case 3: corner = vec2(0.0, 0.0); uv = vec2(r.u0, r.v0); break;
        case 4: corner = vec2(1.0, 1.0); uv = vec2(r.u1, r.v1); break;
        case 5: corner = vec2(1.0, 0.0); uv = vec2(r.u1, r.v0); break;
    }

    // Apply 2x3 affine (tA tC ; tB tD) then translate.
    // tA/tB/tC/tD were already multiplied by sprite (width, height) and global Scale
    // on the CPU side (Task 5), so corner is unit-quad.
    vec2 local = vec2(
        r.tA * corner.x + r.tC * corner.y + r.tx,
        r.tB * corner.x + r.tD * corner.y + r.ty
    );

    gl_Position = pc.projView * vec4(local, 0.0, 1.0);

    vUV    = uv;
    vTex   = r.texSlot;
    // Unpack RGBA8 (matches CPU pack: r=lsb, a=msb)
    vColor = vec4(
        float((r.colorRGBA8      ) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >>  8) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >> 16) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >> 24) & 0xFFu) / 255.0
    );
}
```

- [ ] **Step 3: Write `reanim_inst.frag.glsl`**

Copy `batch.frag.glsl` verbatim — the fragment behavior is identical (texture array + multiply by color):

```glsl
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2      vUV;
layout(location = 1) in vec4      vColor;
layout(location = 2) flat in uint vTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 t = texture(textures[nonuniformEXT(vTex)], vUV);
    outColor = t * vColor;
}
```

- [ ] **Step 4: Verify compilation locally (manual, before Task 2)**

Compile both to SPIR-V using the project's existing glslangValidator (check `vcxproj` for `<CustomBuild>` rules invoking `glslangValidator -V ... -o ....spv`). Run:

```
glslangValidator -V PlantVsZombies/Shader/reanim_inst.vert.glsl -o PlantVsZombies/Shader/reanim_inst.vert.spv
glslangValidator -V PlantVsZombies/Shader/reanim_inst.frag.glsl -o PlantVsZombies/Shader/reanim_inst.frag.spv
```

Expected: both files generated, no errors. If `glslangValidator` complains about `gl_InstanceIndex` without instance count > 1, ignore — it's used in `vkCmdDraw(.., instanceCount, .., firstInstance)`.

- [ ] **Step 5: STOP — no commit yet. Manual review of `InstanceRecord` layout + shader.**

Re-read the struct + shader together once. Verify:
- Vertex index → corner mapping yields two CCW triangles covering [0,0]×[1,1].
- UV mapping puts `(u0,v0)` at TL, `(u1,v1)` at BR (matches atlas convention used by `Graphics::DrawTextureMatrix`).
- 2x3 affine matches the original CPU mat4 (Animator.cpp:379-384): `model[0] = (tA*w, tB*w)`, `model[1] = (tC*h, tD*h)`, `model[3] = (baseX + tx*Scale, baseY + ty*Scale)`. CPU side will bake `w*Scale` into `tA` etc. — corner stays unit.

---

### Task 2: Add instance pipeline to `Graphics::InitializeVulkan`

**Files:**
- Modify: `PlantVsZombies/Graphics.cpp` — pipeline creation section (~line 263-290)
- Modify: `PlantVsZombies/Graphics.cpp` — `VulkanGraphicsState` struct (~line 55-65)
- Verify: `PlantsVsZombies.vcxproj` shader build rules include the two new `.glsl` files

**Goal:** at the end of this task, both instance pipelines exist alongside the batch pipelines, but nothing yet uses them.

- [ ] **Step 1: Add pipeline pointers to PIMPL state**

In `Graphics.cpp` around line 58:

```cpp
struct VulkanGraphicsState {
    pvz::VulkanContext*       ctx       = nullptr;
    pvz::VulkanRenderer*      renderer  = nullptr;
    pvz::VulkanTexturePool*   pool      = nullptr;
    std::unique_ptr<pvz::VulkanPipeline> pipeBatchAlpha;
    std::unique_ptr<pvz::VulkanPipeline> pipeBatchAdd;
    std::unique_ptr<pvz::VulkanPipeline> pipeInstAlpha;   // NEW
    std::unique_ptr<pvz::VulkanPipeline> pipeInstAdd;     // NEW
    // ... rest of fields unchanged ...
};
```

- [ ] **Step 2: Create the two instance pipelines after the batch pipelines**

After `pipeBatchAdd` is created (~line 285), add a sibling block:

```cpp
{
    pvz::VulkanPipeline::Desc desc{};
    desc.vertSpvPath           = "Shader/reanim_inst.vert.spv";
    desc.fragSpvPath           = "Shader/reanim_inst.frag.spv";
    desc.colorFormat           = swapchainFormat;        // same var name as batch block
    desc.setLayouts            = batchSetLayouts;         // same SSBO + bindless layouts
    desc.setLayoutCount        = 2;
    desc.pushConstantSize      = sizeof(glm::mat4);
    desc.pushConstantStages    = VK_SHADER_STAGE_VERTEX_BIT;
    // NO vertex input — gl_VertexIndex and gl_InstanceIndex only
    desc.vertexBindings        = nullptr;
    desc.vertexBindingCount    = 0;
    desc.vertexAttributes      = nullptr;
    desc.vertexAttributeCount  = 0;
    desc.alphaBlend            = true;

    state->pipeInstAlpha = std::make_unique<pvz::VulkanPipeline>();
    if (!state->pipeInstAlpha->Initialize(ctx, desc)) {
        std::cerr << "[Graphics::InitializeVulkan] pipeInstAlpha 创建失败" << std::endl;
        return false;
    }

    desc.alphaBlend    = false;
    desc.additiveBlend = true;
    state->pipeInstAdd = std::make_unique<pvz::VulkanPipeline>();
    if (!state->pipeInstAdd->Initialize(ctx, desc)) {
        std::cerr << "[Graphics::InitializeVulkan] pipeInstAdd 创建失败" << std::endl;
        return false;
    }
}
```

**Adjust to match the actual variable names** in the existing `pipeBatchAlpha` creation block (e.g. `batchSetLayouts` may be named differently — copy directly from that block to ensure layouts match).

- [ ] **Step 3: Wire shader compilation into the build**

Open `PlantsVsZombies.vcxproj`. Find the `<CustomBuild>` rules for `Shader/batch.vert.glsl` and `batch.frag.glsl`. Duplicate the two rule blocks with paths changed to `reanim_inst.vert.glsl` and `reanim_inst.frag.glsl`. The custom build invokes `glslangValidator -V $(InputPath) -o $(OutDir)$(InputName).spv` or similar — match exactly.

Then add the two new `.glsl` files to `<ItemGroup>` near where `batch.*.glsl` are listed.

- [ ] **Step 4: User builds in VS, verifies no errors + run launches**

User: F7 build → confirm "0 errors". F5 run → confirm game starts to main menu without "pipeInstAlpha 创建失败" / "pipeInstAdd 创建失败" stderr lines. Game still renders correctly (no path uses instance pipelines yet).

- [ ] **Step 5: STOP — no commit. Manual confirmation pipelines exist.**

---

### Task 3: Add instance buffer slice to `VkWorkerSlice` + persistent-mapped allocation

**Files:**
- Modify: `PlantVsZombies/Graphics.h` — `VkWorkerSlice` struct (~155-165)
- Modify: `PlantVsZombies/Graphics.cpp` — `BeginParallelRecord` slice carving, `VulkanGraphicsState` PIMPL adds frame's instance VBO

**Goal:** at end of task, each worker slot has a non-overlapping write window into a per-frame `InstanceRecord` host-mapped buffer. Still no consumers.

- [ ] **Step 1: Extend `VkWorkerSlice` with instance fields**

In `Graphics.h:155-165`:

```cpp
struct VkWorkerSlice {
    BatchVertex*    vboPtr      = nullptr;
    glm::mat4*      ssboPtr     = nullptr;
    InstanceRecord* instPtr     = nullptr;   // NEW
    uint32_t        vboBaseVert = 0;
    uint32_t        ssboBaseMat = 0;
    uint32_t        instBaseIdx = 0;          // NEW
    uint32_t        vboCap      = 0;
    uint32_t        ssboCap     = 0;
    uint32_t        instCap     = 0;          // NEW
    uint32_t        vboCount    = 0;
    uint32_t        ssboCount   = 0;
    uint32_t        instCount   = 0;          // NEW
    bool            overflowed  = false;
};
```

- [ ] **Step 2: Add per-frame instance buffer to PIMPL state**

In `VulkanGraphicsState` (Graphics.cpp), find the existing per-frame VBO/SSBO members (there will be a `PerFrameBuffers` or similar struct). Add:

```cpp
struct PerFrameBuffers {
    // ... existing vbo, ssbo, mappings ...
    VkBuffer        instBuf      = VK_NULL_HANDLE;
    VmaAllocation   instAlloc    = VK_NULL_HANDLE;
    InstanceRecord* instMapped   = nullptr;
    uint32_t        instCapacity = 0;    // in records
    uint32_t        instCursor   = 0;    // main-thread serial cursor
};
```

In `InitializeVulkan`, after batch VBO/SSBO allocation, allocate `instBuf` with size = `instCapacity * sizeof(InstanceRecord)`. Initial capacity: pick by estimate `11000 zombies × 15 tracks × 3 (normal+glow+overlay) ≈ 500k records × 48 B = 24 MB per frame × 2 frames in flight = 48 MB`. Reasonable headroom: round up to **32 MB per frame** (≈ 700k records). Use `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | _MAPPED_BIT`, same flags as the existing batch VBO.

- [ ] **Step 3: Update `BeginParallelRecord` to slice the instance buffer**

Find `BeginParallelRecord` in `Graphics.cpp`. It currently carves VBO and SSBO into per-worker windows using `m_prevSliceVboDemand` and `m_prevSliceSsboDemand` weighted distribution. Add a parallel branch for instances:

- Add member: `std::vector<uint32_t> m_prevSliceInstDemand;` (Graphics.h:681-682 area)
- Reserve usable instance region: `instUsable = instCapacity - mainThreadSerialReserve` (mirror the existing pattern — see how `vboUsable` is computed)
- Floor per slot: ~ instUsable / numWorkers / 64 (so a newly-hot slot survives 1 frame); elastic remainder by weight from `m_prevSliceInstDemand`
- Write slot's `instPtr`, `instBaseIdx`, `instCap`, reset `instCount = 0`

In `ReplayAndEndParallel`, after the existing batch slice demand capture, also do `m_prevSliceInstDemand[s] = max(slot.instCount, slot.overflowed ? slot.instCap * 3/2 : slot.instCount)`.

- [ ] **Step 4: Add a serial-path cursor for non-parallel calls**

For Animators that hit `DrawInternal` outside the parallel record window (UI animation, main menu, plant-picker animation), the main thread serial path also needs an `AppendReanimInstance`. Mirror the existing batch `m_batchVertices` / `m_batchMatrices` pattern with `m_batchInstances`. Same `CheckBatch`-style flush semantics (Task 4).

- [ ] **Step 5: User builds, runs, confirms game still renders correctly.**

No instance buffer is read yet by any draw call, so this is a regression-only check. FRAME PROFILE should show `total/frame` within ±0.2 ms of Task 0 baseline. If `total/frame` regresses by >0.5 ms, suspect VMA allocation churn — investigate before proceeding.

- [ ] **Step 6: STOP — no commit.**

---

### Task 4: Add `Graphics::AppendReanimInstance` API + `EmitInstanceDrawRange` helper

**Files:**
- Modify: `PlantVsZombies/Graphics.h` — public `AppendReanimInstance`, private `EmitInstanceDrawRange`
- Modify: `PlantVsZombies/Graphics.cpp` — implementations
- Modify: `PlantVsZombies/Graphics.cpp` — `ReplayAndEndParallel` integrates instance draws

**Goal:** API is callable; in the parallel record path, worker writes to its slice. In the serial path, main thread writes to `m_batchInstances`. `EmitInstanceDrawRange` emits `vkCmdDraw(cb, 6, instCount, 0, firstInstance)` per BlendMode segment, switching to `pipeInstAlpha` / `pipeInstAdd`.

- [ ] **Step 1: Public API signature**

In `Graphics.h` after `DrawTextureMatrix` (~line 374):

```cpp
/**
 * @brief Append a per-instance reanim record. tA/tB/tC/tD are pre-multiplied by
 *        (sprite width × Scale) and (sprite height × Scale) on the caller side
 *        (matches Animator.cpp:379-384 inline mat4 layout). tx/ty are absolute
 *        world coords (already include baseX + transform.x * Scale + offsets).
 *        Atlas UV (u0,v0,u1,v1) must reference an entry already uploaded to the
 *        VulkanTexturePool bindless array. blendMode chooses pipeline at emit time.
 *
 *        Under tl_record (worker thread): writes into the current slot's
 *        slice.instPtr at slice.instCount, bumps the counter, returns silently
 *        on overflow (sets slice.overflowed). Records a SetBlend command if
 *        blendMode differs from current.
 *
 *        Under main thread: appends to m_batchInstances, calls CheckBatch
 *        (instance equivalent — see step 3) which may FlushBatch when full.
 */
void AppendReanimInstance(const InstanceRecord& rec, BlendMode blendMode);
```

- [ ] **Step 2: Implementation — main-thread serial path**

In `Graphics.cpp`, before the existing `FlushBatch` definition:

```cpp
void Graphics::AppendReanimInstance(const InstanceRecord& rec, BlendMode blendMode) {
    if (tl_record) {
        // Worker thread: write into slice
        VkWorkerSlice& s = tl_record->slice;
        if (s.instCount >= s.instCap) {
            s.overflowed = true;
            return;
        }
        // Track BlendMode boundary as a record cmd so replay can switch pipelines.
        if (tl_record->cmds.empty() || tl_record->blendModes.empty() ||
            tl_record->blendModes.back() != blendMode) {
            RecordCmd rc{};
            rc.type = RecCmdType::SetBlend;
            rc.vertOffsetAtCmd = s.instCount;   // reuse field; means "instance offset"
            rc.payloadIdx = static_cast<uint32_t>(tl_record->blendModes.size());
            tl_record->cmds.push_back(rc);
            tl_record->blendModes.push_back(blendMode);
        }
        s.instPtr[s.instCount++] = rec;
        return;
    }

    // Main thread serial path
    if (m_batchInstances.size() >= m_batchInstancesLimit) {
        FlushInstances();
    }
    if (blendMode != m_currentBlendMode) {
        FlushInstances();
        m_currentBlendMode = blendMode;
    }
    m_batchInstances.push_back(rec);
}
```

- [ ] **Step 3: Add `FlushInstances` (main-thread serial flush)**

```cpp
void Graphics::FlushInstances() {
    if (m_batchInstances.empty()) return;
    if (!m_vk) return;

    // Reuse the per-frame instance buffer cursor (same pattern as batch FlushBatch).
    auto& fr = m_vk->frames[m_vk->currentFrame];
    uint32_t base = fr.instCursor;
    if (base + m_batchInstances.size() > fr.instCapacity) {
        std::cerr << "[Graphics::FlushInstances] instance buffer 溢出 ("
                  << base << "+" << m_batchInstances.size() << ">"
                  << fr.instCapacity << ")" << std::endl;
        m_batchInstances.clear();
        return;
    }
    std::memcpy(fr.instMapped + base, m_batchInstances.data(),
                m_batchInstances.size() * sizeof(InstanceRecord));

    VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
    BindDescriptorSetsForInstancing(cb);   // small helper, set=0 includes inst SSBO
    pvz::VulkanPipeline* pipe = (m_currentBlendMode == BlendMode::Add)
        ? m_vk->pipeInstAdd.get()
        : m_vk->pipeInstAlpha.get();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
    PushProjViewConstant(cb, pipe->Layout());

    // 6 vertices × instCount instances, firstInstance = base in the instance buffer.
    vkCmdDraw(cb, 6, static_cast<uint32_t>(m_batchInstances.size()), 0, base);

    fr.instCursor = base + static_cast<uint32_t>(m_batchInstances.size());
    m_batchInstances.clear();
}
```

Add `BindDescriptorSetsForInstancing` and `PushProjViewConstant` if not already factored. Likely they're inline in `FlushBatch` and you can refactor them out, or just inline the same code here.

- [ ] **Step 4: Implementation — static `EmitInstanceDrawRange` for parallel replay**

Mirror `EmitDrawRange`. In `Graphics.cpp` near the existing helper:

```cpp
static void EmitInstanceDrawRange(VkCommandBuffer cb,
                                  VkPipelineLayout layoutAlpha,
                                  VkPipelineLayout layoutAdd,
                                  pvz::VulkanPipeline* pipeAlpha,
                                  pvz::VulkanPipeline* pipeAdd,
                                  uint32_t firstInst,
                                  uint32_t instCount,
                                  BlendMode mode) {
    if (instCount == 0) return;
    pvz::VulkanPipeline* pipe = (mode == BlendMode::Add) ? pipeAdd : pipeAlpha;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
    vkCmdDraw(cb, 6, instCount, 0, firstInst);
}
```

- [ ] **Step 5: Integrate into `ReplayAndEndParallel`**

In `ReplayAndEndParallel`, the current loop walks each slot, replaying `RecordCmd`s and emitting `vkCmdDraw` for `BatchVertex` segments. Extend the per-slot processing:
- After processing `BatchVertex` segments for the slot, also walk `slice.instCount` and emit `EmitInstanceDrawRange` segments. BlendMode boundaries inside `slice` come from the `SetBlend` RecordCmds that the worker injected — walk them in order, slicing the slot's `[0, instCount)` into BlendMode segments, each becoming one `vkCmdDraw`.

**Order:** within each slot, emit batch (sprite-style) draws first, then instance draws. The visual ordering between batch and instance within a slot does **not** preserve the original `DrawInternal` interleaving — but since this plan reserves instance path for animators **without** mAttachedReanims (whole-object atomic), and slot boundaries already break the render order for `DrawAll`'s chunked parallel split, the relative order within a slot is the only thing at stake. Plants and zombies inside the same slot don't interact visually in a way that depends on draw order, so this is safe. **DOCUMENT this assumption in a comment** and revisit if a visual artifact appears in Task 7.

- [ ] **Step 6: User builds, runs. No caller of `AppendReanimInstance` yet — regression check only.**

Confirm `total/frame` within ±0.2 ms of Task 0 baseline. No new errors. Confirm both serial path (DrawText etc.) and parallel path still render zombies correctly.

- [ ] **Step 7: STOP — no commit.**

---

### Task 5: Switch `Animator::DrawInternal` to instance path (no-children case)

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.cpp:331-435`

**Goal:** the 99% case (Animator has no `mAttachedReanims`) emits one or more `InstanceRecord`s; fallback for the 1% case stays untouched.

- [ ] **Step 1: Add the fork at the top of `DrawInternal`**

```cpp
void Animator::DrawInternal(Graphics* g, float baseX, float baseY, float Scale) const {
    if (!mReanim) return;

    // Fast path: no child animators on any track → emit instance records and return.
    // Slow fallback (豌豆 anim_stem→anim_head and similar): keep original code,
    // computes mat4 + calls DrawTextureMatrix.
    bool hasChildren = false;
    for (size_t i = 0; i < mExtraInfos.size(); ++i) {
        if (!mExtraInfos[i].mAttachedReanims.empty()) {
            hasChildren = true;
            break;
        }
    }
    if (!hasChildren) {
        DrawInternalInstanced(g, baseX, baseY, Scale);
        return;
    }

    // ... existing code (the original DrawInternal body) ...
}
```

- [ ] **Step 2: Implement `DrawInternalInstanced`**

Add as a new private method:

```cpp
void Animator::DrawInternalInstanced(Graphics* g, float baseX, float baseY, float Scale) const {
    static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    float blendRatio = 0.0f;
    if (mReanimBlendCounter > 0.0f)
        blendRatio = 1.0f - mReanimBlendCounter / mReanimBlendCounterMax;

    for (int i = 0; i < static_cast<int>(mReanim->GetTrackCount()); ++i) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i, blendRatio);

        bool visible = (i < static_cast<int>(mExtraInfos.size())) &&
                       mExtraInfos[i].mVisible && transform.f != -1;
        if (!visible) continue;

        const Texture* image = mExtraInfos[i].mImage ? mExtraInfos[i].mImage : transform.image;
        if (!image) continue;

        // Compute tA/tB/tC/tD on CPU — these still need trig.
        // (Recall: ROI analysis assumed trig stays on CPU; the GPU win is mat4 ctor +
        //  6-vertex inflation, not trig elimination. See plan top.)
        float angleX = -transform.kx * DEG_TO_RAD;
        float angleY = -transform.ky * DEG_TO_RAD;
        float cosX = cosf(angleX);
        float sinX = sinf(angleX);
        float cosY = cosf(angleY);
        float sinY = sinf(angleY);

        float w = static_cast<float>(image->width);
        float h = static_cast<float>(image->height);

        float tx = transform.x + mExtraInfos[i].mOffsetX;
        float ty = transform.y + mExtraInfos[i].mOffsetY;

        InstanceRecord rec;
        // Bake (sprite width × Scale) into column 0 of the affine, height × Scale into column 1.
        rec.tA = cosX * transform.sx * w * Scale;
        rec.tB = -sinX * transform.sx * w * Scale;
        rec.tC = sinY  * transform.sy * h * Scale;
        rec.tD = cosY  * transform.sy * h * Scale;
        rec.tx = baseX + tx * Scale;
        rec.ty = baseY + ty * Scale;

        // Atlas UV bbox already cached on Texture (Phase atlas commit, see ResourceManager.h)
        rec.u0 = image->aU0;
        rec.v0 = image->aV0;
        rec.u1 = image->aU1;
        rec.v1 = image->aV1;
        rec.texSlot = image->atlasPage;   // bindless slot (Phase 3c naming, verify field)

        // Normal draw: white × alpha
        float baseAlpha = std::clamp(transform.a * mAlpha, 0.0f, 1.0f);
        rec.colorRGBA8 = PackRGBA8(255, 255, 255, static_cast<uint8_t>(baseAlpha * 255.0f));
        g->AppendReanimInstance(rec, BlendMode::Alpha);

        // Glow (Add blend) — same geometry, different color + pipeline
        if (mEnableExtraAdditiveDraw) {
            InstanceRecord glow = rec;
            glow.colorRGBA8 = PackRGBA8(mExtraAdditiveColor.r, mExtraAdditiveColor.g,
                                        mExtraAdditiveColor.b, mExtraAdditiveColor.a);
            g->AppendReanimInstance(glow, BlendMode::Add);
        }

        // Overlay (Alpha blend, color scaled by baseAlpha)
        if (mEnableExtraOverlayDraw) {
            InstanceRecord ov = rec;
            ov.colorRGBA8 = PackRGBA8(mExtraOverlayColor.r, mExtraOverlayColor.g,
                                      mExtraOverlayColor.b,
                                      static_cast<uint8_t>(mExtraOverlayColor.a * baseAlpha));
            g->AppendReanimInstance(ov, BlendMode::Alpha);
        }
    }
}

static inline uint32_t PackRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}
```

Declare `DrawInternalInstanced` in `Animator.h` private section.

- [ ] **Step 3: Verify field names against `Texture` struct**

`Texture::aU0/aV0/aU1/aV1` and `Texture::atlasPage` field names came from memory `[[pvz-perf-optimization]]` ("per-reanim texture atlas" section). Open `PlantVsZombies/ResourceManager.h` and confirm. If different (e.g. `mU0` or `atlasSlot`), substitute. **Do not guess** — read the struct.

- [ ] **Step 4: User builds, runs warmed-up 11000-zombie test, pastes FRAME PROFILE.**

**GATE B condition:**
- `total / frame` drops to ≤ 8.5 ms (i.e. ≥ 117 FPS) → PROCEED to Task 6.
- `total / frame` drops to 8.5-9.5 ms → MARGINAL, evaluate by inspection (visual artifacts?) before deciding.
- `total / frame` ≥ 9.5 ms (≤ 105 FPS) → STOP, write findings to memory, revert.

**Visual check:** all zombies look correct (no flicker, no garbage colors, no z-order issues). Plants with `anim_stem→anim_head` (豌豆 family) should still look correct — they took the fallback path.

- [ ] **Step 5: STOP — no commit.** Only proceed to Task 6 if GATE B passes.

---

### Task 6: Resilience pass — glow/overlay correctness, parallel slice overflow handling

**Files:**
- Modify: `PlantVsZombies/Graphics.cpp` — `BeginParallelRecord` (slice sizing), overflow telemetry
- Modify: `PlantVsZombies/Reanimation/Animator.cpp` — slot ordering inside `DrawInternalInstanced`

**Goal:** clean up edge cases that surfaced in Task 5 testing.

- [ ] **Step 1: Verify worker slice never overflows under stress**

Run 11000-zombie + 5 simultaneous CherryBombs (lots of glow/overlay). Check stderr for `Worker slot N overflowed slice` style messages. If any, the floor in Task 3 Step 3 is too small — raise `instUsable / numWorkers / 32` instead of `/64`.

- [ ] **Step 2: Verify glow + overlay drawn in correct order**

Plant a SnowPea (has glow), CherryBomb (has overlay during explosion), check visually that:
- Normal sprite draws first (correct base coloring)
- Glow Add blend over the sprite (lighter)
- Overlay Alpha blend on top (color tint)

In `DrawInternalInstanced`, this is ensured by calling `AppendReanimInstance(rec, Alpha)` first, then `(glow, Add)`, then `(ov, Alpha)`. Inside a slot, the order is preserved by worker writing into `slice.instPtr` sequentially. **But Replay emits Alpha-segment then Add-segment per slot's BlendMode boundaries** — so if normal-Alpha and overlay-Alpha straddle a glow-Add in the middle, the BlendMode boundaries auto-split into 3 segments in the right order. Verify by inspection.

- [ ] **Step 3: STOP — no commit.**

---

### Task 7: A/B verification under full game stress + final GATE

**Files:**
- No code changes — measurement only.

**Goal:** definitive go/no-go data with `m_parallelDrawEnabled` toggle for A/B in the same session.

- [ ] **Step 1: Add a runtime switch `m_useInstancePath`**

In Graphics.h add `bool m_useInstancePath = true;` with `SetInstancePathEnabled(bool)` / `IsInstancePathEnabled()`. In `Animator::DrawInternal` fork, gate the fast path on `g->IsInstancePathEnabled() && !hasChildren`. (Implementation detail: Animator needs a way to query Graphics — pass it via the Graphics* parameter already plumbed.)

- [ ] **Step 2: A run, instance disabled**

User: launch, force `SetInstancePathEnabled(false)`, warm 11000-zombie scene, paste FRAME PROFILE.

- [ ] **Step 3: B run, instance enabled**

User: in same session (or VS rebuild + relaunch as the user prefers), `SetInstancePathEnabled(true)`, same scene, paste FRAME PROFILE.

- [ ] **Step 4: Decide**

| Outcome | Action |
|---|---|
| B `total/frame` − A `total/frame` ≤ -1.5 ms (≥ +20 FPS) | **KEEP** — write success to memory, proceed to Task 8 cleanup |
| -1.5 < B−A ≤ -0.5 ms | **MARGINAL** — decide based on visual quality, complexity acceptance |
| B−A > -0.5 ms | **REVERT** — instance path didn't pay; write learning to memory, restore HEAD |

- [ ] **Step 5: STOP — no commit.** Record numbers in memory before proceeding.

---

### Task 8: Cleanup + handoff

**Files:**
- Modify: `PlantVsZombies/Reanimation/Animator.cpp` (remove any Task 0 probes if any leaked through)
- Optionally: `PlantVsZombies/Graphics.cpp` (remove `m_useInstancePath` toggle if final-decided)
- Optionally: `PlantVsZombies/Profiler.h` and call sites (full removal — per `[[pvz-perf-optimization]]` "LATER" section)

**Goal:** repo is in a clean state for the user-driven commit.

- [ ] **Step 1: `git diff` review**

Verify only intended files changed. No commented-out blocks. No leftover `[Task0]` printfs. No `Xa.trig` / `Xb.mat4` `PROFILE_SCOPE`s.

- [ ] **Step 2: Decide whether to keep the `m_useInstancePath` toggle**

Pro-keep: future debug A/B is one-line. Pro-remove: dead code. Default: **keep**, given history of phase plans (`m_parallelDrawEnabled` was kept) and per `[[collaboration-style]]` "保留 fallback".

- [ ] **Step 3: Update memory**

Write a new file `pvz-gpu-instancing-reanim.md` summarizing:
- Final wall-time delta vs Task 0 baseline (e.g. 10.0 → X.X ms, 100 FPS → YYY FPS)
- The ~1% child-animator fallback decision (豌豆 stem→head)
- Any unexpected discoveries (worker slice sizing, atlas UV plumbing, etc.)
- Lessons echoed back to `[[pvz-perf-optimization]]` POST-PHASE-3 section

Update `MEMORY.md` index entry.

- [ ] **Step 4: Hand off to user for commit**

Show `git status` + `git diff --stat` to user. User commits with their own message. **Plan complete.**

---

## Self-Review Notes

**Spec coverage check** — every requirement from the brainstorming session has a task:
- Two new pipelines: Task 2 ✓
- Instance record + shader: Task 1 ✓
- Buffer slice + persistent mapped: Task 3 ✓
- API + replay integration: Task 4 ✓
- Animator switch + fallback: Task 5 ✓
- Glow/overlay multi-blend: Task 5 + 6 ✓
- A/B GATE: Task 7 ✓
- Memory writeback: Task 8 ✓

**Placeholder scan:** no "TBD" / "implement later" / "Similar to Task N" left. All code is complete.

**Type consistency:**
- `InstanceRecord` 48 B fields used identically in Graphics.h, vert shader, and `DrawInternalInstanced` ✓
- `tl_record` / `tl_state` thread-local guard convention reused from existing Graphics ✓
- `BlendMode::Alpha` / `BlendMode::Add` enum names match Graphics.h:93-97 ✓
- Atlas field names (`aU0..aV1`, `atlasPage`) noted as "verify against ResourceManager.h" in Task 5 Step 3

**Risk reminder:** Task 5 Step 4 is the make-or-break GATE. If it fails, do not push deeper into Task 6 hoping incremental wins will close the gap — the architectural assumption (GPU vertex-shader mat4 ctor + 6-vert inflation is cheaper than CPU + write traffic reduction) was either right at Task 5 or wrong.
