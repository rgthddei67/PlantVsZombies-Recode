# Phase 6: OpenGL 残留清理 — 设计文档

> 日期：2026-05-16（v2，写计划期间据精确勘探修订）
> 关联：主迁移计划 `opengl-vulkan-vcpkg-brotli-x64-windows-snuggly-bentley.md` 的 **Phase 6 — Cleanup**；
> phase-5 计划 Task 7 Step 2 将其列为下一步。

## 目标

移除 Vulkan 迁移完成后所有死掉的 OpenGL 残留代码、误导性命名与过时注释，使代码库：

- 不再有 OpenGL 符号（`glad`、`ShaderProgram`、GL `.glsl` 着色器、`GLuint`/`GLenum` 等 GL 类型别名）
- 不再有迁移过渡期的桥接命名（`GLTexture`、`RenderTextToGLTexture`）
- 不再有 `-Vulkan` 冒烟测试脚手架（Phase 1/2 临时验证代码）

**本次为纯删除/改名/等价改型，不改变任何运行时行为。** 已核实 `Graphics.cpp` 中无任何真实 `gl*` 调用，Vulkan 已在已提交的 `c72265f` 起完全接管渲染。`GLuint` 在 Win64 上即 `unsigned int` ≡ `uint32_t`，改型为零行为变更的机械替换。

## 已确认决策

| 决策 | 取值 |
|---|---|
| 清理范围 | 完整 Phase 6（含 `GLTexture` 改名 + 移除冒烟脚手架 + 死 GL 机器删除） |
| `struct GLTexture` 改名目标 | **`Texture`**（已核实代码库无任何 `Texture` 类型，零冲突） |

## 精确勘探结论（GL 残留全量盘点，92 处 / 6 文件）

| 文件 | GL 占用 | 处置 |
|---|---|---|
| `ShaderProgram.h` (5) / `ShaderProgram.cpp` (2) | 全空壳 | Task 1 整文件删除 |
| `ResourceManager.h` | `#include <glad/glad.h>`(12) + `GLuint id`(23) | Task 4：删 include + `GLuint`→`uint32_t` |
| `ResourceManager.cpp` | 仅注释中 `GL_UNPACK_ALIGNMENT`(48) | 无代码改动（可选润色注释） |
| `Graphics.h` | 23：死机器 + 活跃但 GL 类型 + 2 个 include | Task 2/3/4 分级处理 |
| `Graphics.cpp` | 60：~50 处 `(GLuint)` 强转 + 3 处签名/局部 | Task 3 等价改型 |

**Graphics.h 死 GL 机器（零 .cpp 引用，删声明）：** `m_currentVAO`、`BindVAO(GLuint)`、`m_spriteVAO/VBO/EBO`、`m_batchVAO`、`m_batchVBO`、`m_matrixBuffer`、`m_useSSBO`、`m_geomVAO`、`m_geomVBO`、`m_geomBatchMode = GL_TRIANGLES`、`DrawGeomImmediate(..., GLenum)`。已逐一 grep 确认 `Graphics.cpp` 零引用（`m_useSSBO` 亦零引用，非行为风险）。

**Graphics.h/cpp 活跃但 GL 类型（改 `uint32_t`，不删）：** `BatchVertex::texIndex`/`matrixIndex`、第 84 行 `textureID`、`m_whiteTexture`（现为 bindless 槽位，来自 `whiteTex->bindlessIndex`）、`m_batchTextures`（`std::vector<GLuint>`）、`BindTexture(GLuint)`（声明 + 定义）、`GetOrCreateTextTexture(...)→GLuint`（声明 + 定义 + 调用处局部 `GLuint texID`）、`Graphics.cpp` 中约 50 处 `(GLuint)texIndex/matrixIndex/matIndex` 强转。

## 执行约束

- 遵循 `CLAUDE.md` 与协作记忆：**助手不构建**。每个 Task 收尾列出用户需在 VS 2026 验证的点，然后等待。
- 任务按**编译检查点**切分；单变量、可回退；任一 Task 编译失败即停，报告根因而非堆补丁（measure-first）。
- commits user-driven：计划不含 `git commit` 步骤；提交时机由用户决定。

## 工作单元（7 个编译检查点 Task）

1. **删除 `ShaderProgram` 空壳** — 删文件 + `Graphics.h` 的 include 与 3 个死 `ShaderProgram` 成员 + vcxproj/filters 条目。
2. **删除 Graphics.h 死 GL 机器** — 移除上列零引用的 VAO/VBO/EBO/matrixBuffer/useSSBO/geom/currentVAO/BindVAO/DrawGeomImmediate 声明（含 `GL_TRIANGLES` 初值）。
3. **活跃 GL 类型 → `uint32_t` 等价改型** — Graphics.h/cpp 中 BatchVertex 字段、textureID、m_whiteTexture、m_batchTextures、BindTexture、GetOrCreateTextTexture 及所有 `(GLuint)` 强转。
4. **切断 glad** — 删 `Graphics.h` 与 `ResourceManager.h` 的 `#include <glad/glad.h>`；`ResourceManager.h:23` `GLuint`→`uint32_t`。这是 glad 完全脱钩的检查点：任何遗漏 GL 符号在此暴露。
5. **删除旧 GL 着色器文件** — 6 个不参与 glslc 的旧 `.glsl` + vcxproj/filters 条目。
6. **`GLTexture` → `Texture` 全量改名** — 89 处 / 27 文件机械改名 + 结构体注释更新。
7. **移除 `-Vulkan` 冒烟脚手架 + 命名/注释收尾** — main.cpp 冒烟区段与参数分支、triangle/quad 着色器、VulkanRenderer 冒烟专用 API（经调用方核实后）、`RenderTextToGLTexture`→`RenderTextToVulkanTexture`、过时注释。

## 已确认无需处理

- `SDL_GL_*` / `SDL_GLContext`：已在 Phase 3a 移除（`GameApp.h:60` 注释佐证）
- `USE_VULKAN` 编译开关：grep 全库无引用，已不存在
- glad 库文件：项目内无 glad 源文件，仅死 include（Task 4 处理）
- `ResourceManager.cpp`：唯一 GL 命中为注释，无代码改动

## 风险与边界

1. **Task 3/4 顺序**：先把 Graphics.h/cpp 内所有活跃 GL 类型改成 `uint32_t`（Task 3），再删 include（Task 4）。Task 4 编译失败即说明仍有遗漏的 GL 类型——逐个换定宽类型，不批量盲替。
2. **死/活分类已逐符号 grep 核实**：死机器删声明、活跃符号改型，二者不可混淆（删活跃符号会破坏 Vulkan 渲染）。
3. **Task 6 改名误伤**：`GLTexture` 子串安全（无 `XGLTexture` 类标识符）；`mGLTextures`→`mTextures` 等关联名仅在确属该结构体时收敛，逐文件核对。
4. **Task 7 冒烟 API 边界**：删 `DrawFrame`/`SetTrianglePipeline`/`SetTexturedQuad` 等前必须 grep 每个符号全部调用方，确认无游戏主路径（`BeginFrame`/`EndFrame`/`CurrentCmdBuffer`）依赖；有疑问停下报告。
5. 全程单变量、可回退；任一 Task 编译失败即停。

## 验收

完整 Phase 6 完成后：

1. 代码库构建无 GL 符号、无 glad、无 `ShaderProgram`、无 `GLTexture`、无 `-Vulkan` 脚手架
2. 游戏在 Vulkan 路径下行为与清理前完全一致（纯死代码移除 + 等价改型，零行为变更）
3. 旧 GL 着色器与冒烟着色器从工程与磁盘移除，`.vcxproj`/`.filters` 同步干净
