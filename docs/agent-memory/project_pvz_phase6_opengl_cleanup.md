---
name: pvz-phase6-opengl-cleanup
description: Phase 6 OpenGL 残留清理：7 个 Task 全部完成并经用户 VS 构建+冒烟验证（2026-05-17）；含执行期修复的预存 LNK2019
metadata:
  node_type: memory
  type: project
  originSessionId: d807e490-66c5-464a-9d51-eb4467cbc0b6
---

# PvZ Phase 6 — OpenGL 残留清理（已完成，用户验证通过；2026-05-17）

主 Vulkan 迁移计划（`C:\Users\ZJ\.claude\plans\opengl-vulkan-vcpkg-brotli-x64-windows-snuggly-bentley.md`）的 **Phase 6 — Cleanup**。用户原话引用 phase-5 计划，但 OpenGL 清理实为 Phase 6。关联 [pvz-perf-optimization](project_pvz_perf_optimization.md)、[collaboration-style](feedback_collaboration_style.md)。

## 产出物（已写入仓库 docs/）
- Spec: `docs/superpowers/specs/2026-05-16-phase6-opengl-cleanup-design.md`（v2，已据精确勘探修订）
- Plan: `docs/superpowers/plans/2026-05-17-phase6-opengl-cleanup.md`（7 个编译检查点 Task，无占位符，已自查）

## 用户已批准的决策
- 范围：**完整 Phase 6**（含 GLTexture 改名 + 移除 -Vulkan 冒烟脚手架 + 删死 GL 机器）
- `struct GLTexture` → **`Texture`**（已核实代码库无任何 `Texture` 类型，零冲突；不能用 VulkanTexture，已被 pvz::VulkanTexture 占用）

## 状态：Phase 6 完成（2026-05-17）
- **7 个 Task 全部完成，逐 Task 经用户在 VS 2026 重新生成 + 最终完整冒烟验证通过**，行为与清理前完全一致。内联 superpowers:executing-plans 执行；commits user-driven，**用户已于 2026-05-17 提交 `afa9f9a`「Vulkan 迁移收尾 + Phase 6 OpenGL 残留清理」，工作树干净**。
- 源码树终态（已全仓 grep 核实零残留）：无 ShaderProgram / glad / GL 代码类型别名 / gl*() / GLTexture / mGLTextures / RenderTextToGLTexture / -Vulkan 冒烟脚手架。`GLTexture`→`Texture`、`mGLTextures`→`mTextures`、`RenderTextToGLTexture`→`RenderTextToVulkanTexture`。保留 `VulkanContext.cpp` 的 `pEngineName="PVZ-Vulkan"`（合法 Vulkan 串）。
- **执行期发现并已修复的预存断裂（非 Phase 6 引入）：** working-tree 未提交的 Vulkan 改写已把几何并入统一 `FlushBatch()`，整个 geom-batch 子系统死掉：`FlushGeomBatch()` 仅声明+2 调用无定义 → 重新生成 **LNK2019**。已行为等价清除（删 2 调用，保留其前真实 `FlushBatch();`；删 Graphics.h 死声明 GeomVertex/m_geomBatchVertices/Mode/Capacity/DrawGeomImmediate/FlushGeomBatch）。plan 已同步勘误。
- Phase 6 改动与 `c72265f` 后那批**未提交**的 Vulkan 迁移在同批文件交织，无法按文件拆——用户选择一并提交，二者同收于 `afa9f9a`。注：该次 `git add -A` 也把无关备份 `PlantsVsZombies.vcxproj.20260516_174138.bak` 一并提交了（如介意仓库整洁可后续 `git rm`）。
- 教训：plan 的死/活分类基于旧勘探，执行时**逐 Task 在当前工作树重新 grep 复核**才发现 FlushGeomBatch 误分类。后续类似清理沿用此「先复核再删」纪律。

## 已逐符号 grep 核实的关键事实（执行时直接用，省重复勘探）
- `Graphics.cpp` 已无任何真实 `gl*` 调用（Vulkan 自提交 `c72265f` 完全接管）。
- 项目内无 glad 源文件，`#include <glad/glad.h>` 仅在 `Graphics.h:5`、`ResourceManager.h:12`（死 include）。
- **死 GL 机器（Graphics.h，零 .cpp 引用，删声明）：** m_currentVAO, BindVAO, m_spriteVAO/VBO/EBO, m_batchVAO/VBO, m_matrixBuffer, **m_useSSBO（确认零引用，非行为风险——曾误判，已纠正）**, m_geomVAO/VBO, m_geomBatchMode=GL_TRIANGLES, InitSpriteRenderer, InitGeomRenderer, DrawGeomImmediate。
- **活跃但 GL 类型（改 uint32_t，勿删）：** BatchVertex::texIndex/matrixIndex, Graphics.h:84 textureID, m_whiteTexture, m_batchTextures, BindTexture(GLuint), GetOrCreateTextTexture()→GLuint, Graphics.cpp ~50 处 (GLuint) 强转, Graphics.cpp:941 局部 GLuint texID。`GLuint`≡`uint32_t`（Win64），改型零行为变更。
- **历史状态（已被 2026-07-23 取代）：** 当时 `ApplyTopClipRectToGL` 仍活跃；现已由通用逐顶点/逐实例 shader ClipRect 取代并删除，见 [project_pvz_shader_clip_rect](project_pvz_shader_clip_rect.md)。原列的 FlushGeomBatch/m_geomBatchVertices/m_geomBatchCapacity 实为**死**（无定义/无使用），已删，见上方状态节。
- **保留的活跃 Vulkan 着色器：** Shader/batch.vert.glsl、batch.frag.glsl（+spv，在 vcxproj ShaderSource）。删的是旧 GL：batch_vertex/batch_fragment/geom_vertex/geom_fragment/sprite_vertex/sprite_fragment.glsl（仅 None，不参与 glslc）。
- **冒烟专用、仅 main.cpp 调用：** VulkanRenderer 的 DrawFrame/SetTrianglePipeline/SetTexturedQuad/ClearTexturedQuad + 成员 mTrianglePipeline/mQuadPipeline/mQuadDescSet/mQuadPC + QuadPushConsts；triangle/quad 着色器。游戏主路径用 BeginFrame/EndFrame/CurrentCmdBuffer（保留）。
- `GLTexture` 改名面：89 处 / 27 文件（plan Task 6 已列全 27 文件清单）。
