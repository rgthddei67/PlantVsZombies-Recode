# Phase 6: OpenGL 残留清理 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **本计划由用户在 VS 2026 内联手动构建**（CLAUDE.md 禁止助手构建）。每个 Task 末尾是一个**编译检查点**：列出用户需在 VS 验证的点，然后**等待用户回报**再进入下一 Task。**不要运行 msbuild/cl.exe。计划不含 `git commit` 步骤——提交时机由用户决定（commits user-driven）。**

**Goal:** 移除 Vulkan 迁移后所有死掉的 OpenGL 残留代码、误导命名与过时注释，使代码库无 GL 符号、无 glad、无 `ShaderProgram`、无 `GLTexture`、无 `-Vulkan` 冒烟脚手架。

**Architecture:** 纯删除 / 改名 / 等价改型。`Graphics.cpp` 已无任何真实 `gl*` 调用，Vulkan 自 `c72265f` 起完全接管。`GLuint` 在 Win64 上即 `unsigned int` ≡ `uint32_t`，改型为零行为变更。按 7 个编译检查点切分，单变量、可回退。

**Tech Stack:** C++17, Vulkan 1.3, MSVC / VS 2026, vcpkg-static-x64。

---

## 关联文档

- Spec: `docs/superpowers/specs/2026-05-16-phase6-opengl-cleanup-design.md`
- 主迁移计划: `C:\Users\ZJ\.claude\plans\opengl-vulkan-vcpkg-brotli-x64-windows-snuggly-bentley.md`（Phase 6）

## 死/活分类（已逐符号 grep 核实）

**死 GL 机器（Graphics.h，零 `Graphics.cpp` 引用 → 删声明）：**
`m_currentVAO`、`void BindVAO(GLuint)`、`m_spriteVAO`、`m_spriteVBO`、`m_spriteEBO`、`m_batchVAO`、`m_batchVBO`、`m_matrixBuffer`、`m_useSSBO`、`m_geomVAO`、`m_geomVBO`、`m_geomBatchMode = GL_TRIANGLES`、`bool InitSpriteRenderer()`、`bool InitGeomRenderer()`、`void DrawGeomImmediate(..., GLenum)`。

**活跃但 GL 类型（改 `uint32_t`，不删）：**
`BatchVertex::texIndex`/`matrixIndex`、Graphics.h 第 84 行 `textureID`、`m_whiteTexture`、`m_batchTextures`、`int BindTexture(GLuint)`、`GLuint GetOrCreateTextTexture(...)`、`Graphics.cpp` 中所有 `(GLuint)texIndex/matrixIndex/matIndex` 强转、`Graphics.cpp:941` 局部 `GLuint texID`。

**活跃保留（名字含 GL 但已是 Vulkan 实现，不动）：**
`ApplyTopClipRectToGL()`（定义 Graphics.cpp:466，调用 507/521）。

> **⚠️ 2026-05-17 执行期勘误（已落实代码）：** 原分类把 `FlushGeomBatch()`、`m_geomBatchVertices`、`m_geomBatchCapacity` 误列为「活跃保留」。实测：working-tree 的 Vulkan 改写已把几何并入统一 `FlushBatch()`，整个 geom-batch 子系统在 `Graphics.cpp` 中**零定义零使用**——`FlushGeomBatch()` 仅有声明(Graphics.h)+2 处调用(Graphics.cpp:499/518)却**无定义**，导致重新生成时 **LNK2019**（与 Phase 6 Task 1 改动无关，属未提交 Vulkan 迁移的预存断裂）。已在 Task 1 验证前作**行为等价**的死代码清除：删 Graphics.cpp:499/518 两处 `FlushGeomBatch();` 调用（其前的真实 `FlushBatch();` 保留）+ 删 Graphics.h 死声明 `struct GeomVertex`、`m_geomBatchVertices`、`m_geomBatchMode`、`m_geomBatchCapacity`、`DrawGeomImmediate`、`FlushGeomBatch`。故下方 Task 2 Step 4 不再含 `m_geomBatchMode`、Step 5 不再含 `DrawGeomImmediate`（均已删，步骤已订正）。

---

## Task 1: 删除 `ShaderProgram` 空壳

**Files:**
- Delete: `PlantVsZombies/ShaderProgram.h`
- Delete: `PlantVsZombies/ShaderProgram.cpp`
- Modify: `PlantVsZombies/Graphics.h`（第 24 行 include；第 677–679 行三成员）
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj`（第 90、192 行）
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj.filters`（第 172、453 行）

- [ ] **Step 1: 删除两个文件**

删除磁盘文件 `PlantVsZombies/ShaderProgram.h` 与 `PlantVsZombies/ShaderProgram.cpp`。

- [ ] **Step 2: 移除 Graphics.h 的 include**

删除 `Graphics.h` 中这一整行：

```cpp
#include "ShaderProgram.h"
```

- [ ] **Step 3: 删除 Graphics.h 三个死成员**

在 `Graphics.h` 删除这三行（位于 private 段，原约 677–679）：

```cpp
	ShaderProgram m_spriteShader;      ///< 精灵着色器（立即模式）
	ShaderProgram m_batchShader;        ///< 批处理着色器
	ShaderProgram m_geomShader;         ///< 几何图形着色器（仅颜色，无纹理）
```

- [ ] **Step 4: 移除 vcxproj 条目**

`PlantsVsZombies.vcxproj`：删除整行 `<ClCompile Include="ShaderProgram.cpp" />` 和整行 `<ClInclude Include="ShaderProgram.h" />`。

- [ ] **Step 5: 移除 vcxproj.filters 条目**

`PlantsVsZombies.vcxproj.filters`：删除 `ShaderProgram.cpp` 的 `<ClCompile Include="ShaderProgram.cpp">…</ClCompile>` 整个元素块，以及 `ShaderProgram.h` 的 `<ClInclude Include="ShaderProgram.h">…</ClInclude>` 整个元素块（含其内部 `<Filter>` 子元素）。

- [ ] **Step 6: 编译检查点（用户在 VS 2026 验证）**

请用户在 VS：从解决方案资源管理器确认 ShaderProgram 文件已移除 → 重新生成。
预期：**编译通过**（三个成员与文件确认零引用）。
若报「`m_spriteShader` 等未定义引用」或别处 include 了 `ShaderProgram.h`：停下，把报错原文回报，不继续。
**等待用户回报后进入 Task 2。**

---

## Task 2: 删除 Graphics.h 死 GL 机器

**Files:**
- Modify: `PlantVsZombies/Graphics.h`（private 段，原约 674–717、779、785、792）

- [ ] **Step 1: 删除 VAO/BindVAO 行**

`Graphics.h` 删除这两行（原约 674–675）：

```cpp
	GLuint m_currentVAO = 0;                           ///< 当前绑定的 VAO（避免冗余切换）
	void BindVAO(GLuint vao);
```

- [ ] **Step 2: 删除 sprite VAO/VBO/EBO 行**

删除这三行（原约 680–682）：

```cpp
	GLuint m_spriteVAO = 0;              ///< 精灵渲染 VAO
	GLuint m_spriteVBO = 0;              ///< 精灵渲染 VBO
	GLuint m_spriteEBO = 0;              ///< 精灵渲染 EBO
```

- [ ] **Step 3: 删除 batch VAO/VBO/matrixBuffer/useSSBO 行**

删除这四行（原约 707–710），**保留** `m_batchBufferCapacity`：

```cpp
	GLuint m_batchVAO = 0;                       ///< 批处理 VAO
	GLuint m_batchVBO = 0;                       ///< 批处理 VBO
	GLuint m_matrixBuffer = 0;                   ///< 矩阵缓冲（UBO 或 SSBO）
	bool m_useSSBO = false;                      ///< 运行时是否使用 SSBO（GL 4.3+）
```

- [ ] **Step 4: 删除 geom VAO/VBO 行**

删除这两行：

```cpp
	GLuint m_geomVAO = 0;                         ///< 几何图形 VAO
	GLuint m_geomVBO = 0;                         ///< 几何图形 VBO
```

> 勘误：`m_geomBatchMode`、`m_geomBatchVertices`、`m_geomBatchCapacity` 已在 Task 1 前置 LNK2019 解阻修复中删除，本步不再涉及。

- [ ] **Step 5: 删除死的立即模式方法声明**

删除这条声明及其上方 Doxygen 注释块（原约 775–779）：

```cpp
	/**
	 * @brief 初始化精灵渲染器（立即模式）。
	 * @return 成功返回 true
	 */
	bool InitSpriteRenderer();
```

删除这条声明及其上方注释块（原约 781–785）：

```cpp
	/**
	 * @brief 初始化几何图形渲染器。
	 * @return 成功返回 true
	 */
	bool InitGeomRenderer();
```

> 勘误：`DrawGeomImmediate` 已在 Task 1 前置 LNK2019 解阻修复中删除，本步不再涉及。

- [ ] **Step 6: 编译检查点（用户在 VS 验证）**

请用户重新生成。
预期：**编译通过**。这些符号已逐一 grep 确认 `Graphics.cpp` 零引用。
若报某符号「未定义」或「仍被引用」：停下回报原文（说明分类有误，需把该符号移回保留集）。
**等待用户回报后进入 Task 3。**

---

## Task 3: 活跃 GL 类型 → `uint32_t` 等价改型

**Files:**
- Modify: `PlantVsZombies/Graphics.h`（第 64–65、84、701、722、815、846 行）
- Modify: `PlantVsZombies/Graphics.cpp`（第 604、780、941 行 + 所有 `(GLuint)` 强转）

> `GLuint` 在 Win64 即 `unsigned int`，`uint32_t` 亦 `unsigned int` —— 改型为零行为变更。`<cstdint>` 已被工程广泛包含；如本步后报 `uint32_t` 未定义，在 `Graphics.h` 顶部 include 区补 `#include <cstdint>`。

- [ ] **Step 1: BatchVertex 两字段改型（Graphics.h 第 64–65）**

```cpp
	GLuint texIndex;     // 纹理索引（对应纹理单元）
	GLuint matrixIndex;  // 变换矩阵索引（对应矩阵数组）
```

改为：

```cpp
	uint32_t texIndex;     // 纹理索引（bindless 槽位）
	uint32_t matrixIndex;  // 变换矩阵索引（SSBO 槽位）
```

- [ ] **Step 2: textureID 改型（Graphics.h 第 84）**

```cpp
	GLuint textureID = 0;          // bindless 槽位（Phase 3c）
```

改为：

```cpp
	uint32_t textureID = 0;          // bindless 槽位（Phase 3c）
```

- [ ] **Step 3: m_batchTextures 改型（Graphics.h 第 701）**

```cpp
	std::vector<GLuint> m_batchTextures;        ///< 当前批次使用的纹理 ID 列表
```

改为：

```cpp
	std::vector<uint32_t> m_batchTextures;        ///< 当前批次使用的 bindless 槽位列表
```

- [ ] **Step 4: m_whiteTexture 改型（Graphics.h 第 722）**

```cpp
	GLuint m_whiteTexture = 0;   ///< 1×1 纯白纹理，用于 FillRect 批处理
```

改为：

```cpp
	uint32_t m_whiteTexture = 0;   ///< 1×1 纯白纹理 bindless 槽位，用于 FillRect 批处理
```

- [ ] **Step 5: BindTexture / GetOrCreateTextTexture 声明改型（Graphics.h 第 815、846）**

`int BindTexture(GLuint textureID);` 改为 `int BindTexture(uint32_t textureID);`

`GLuint GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,` 改为 `uint32_t GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,`（仅改返回类型 `GLuint`→`uint32_t`，其余签名两行不动）

- [ ] **Step 6: BindTexture / GetOrCreateTextTexture 定义改型（Graphics.cpp 第 604、780）**

`int Graphics::BindTexture(GLuint textureID) {` 改为 `int Graphics::BindTexture(uint32_t textureID) {`

`GLuint Graphics::GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,` 改为 `uint32_t Graphics::GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,`

- [ ] **Step 7: 局部变量改型（Graphics.cpp 第 941）**

```cpp
	GLuint texID = GetOrCreateTextTexture(text, fontKey, fontSize, color, w, h);
```

改为：

```cpp
	uint32_t texID = GetOrCreateTextTexture(text, fontKey, fontSize, color, w, h);
```

- [ ] **Step 8: 全量替换 Graphics.cpp 中的 `(GLuint)` 强转**

在 `Graphics.cpp` 内，把所有 `(GLuint)` 文本替换为 `(uint32_t)`（约 50 处，集中在 BatchVertex 聚合初始化：`(GLuint)texIndex`、`(GLuint)matrixIndex`、`(GLuint)matIndex`）。这是全文件范围、仅限 `(GLuint)` 字面的机械替换，无其他 `GLuint` 用法残留于 .cpp（已 grep 确认 .cpp 的 `GLuint` 仅出现在第 604/780/941 行与这些强转）。

- [ ] **Step 9: 编译检查点（用户在 VS 验证）**

请用户重新生成。
预期：**编译通过**。`glad/glad.h` 此时仍在（Task 4 才删），故 `GLuint` 仍可解析——本步只把活跃符号脱离 GL 类型。
若报 `uint32_t` 未定义：在 `Graphics.h` include 区加 `#include <cstdint>`，重生成。
若报别处仍以 `GLuint` 接收 `BindTexture`/`GetOrCreateTextTexture` 返回值导致类型不匹配：回报原文（一般无，因 `GLuint`≡`uint32_t` 隐式可转）。
**等待用户回报后进入 Task 4。**

---

## Task 4: 切断 glad

**Files:**
- Modify: `PlantVsZombies/Graphics.h`（第 5 行）
- Modify: `PlantVsZombies/ResourceManager.h`（第 12、23 行）

- [ ] **Step 1: 删除 Graphics.h 的 glad include（第 5 行）**

删除整行：

```cpp
#include <glad/glad.h>
```

- [ ] **Step 2: 删除 ResourceManager.h 的 glad include（第 12 行）**

删除整行：

```cpp
#include <glad/glad.h>
```

- [ ] **Step 3: ResourceManager.h 的 GLuint 改型（第 23 行）**

```cpp
    GLuint id = 0;           // Phase 3b: bindlessIndex（4096 槽位之一）
```

改为：

```cpp
    uint32_t id = 0;           // Phase 3b: bindlessIndex（4096 槽位之一）
```

（`ResourceManager.h` 已含 `<cstdint>` 依赖链；若报 `uint32_t` 未定义，于该文件 include 区补 `#include <cstdint>`。）

- [ ] **Step 4: 编译检查点（glad 完全脱钩，用户在 VS 验证）**

请用户重新生成。
预期：**编译通过且全工程不再依赖 glad**。这是 glad 脱钩的关键检查点。
若报任何 `GLuint`/`GLint`/`GLenum`/`GLfloat`/`GL_*` 未定义：说明仍有遗漏的 GL 符号未在 Task 2/3 处理。**把每条报错的文件:行原文回报**，逐个判定（死→删 / 活→`uint32_t` 等定宽类型），不批量盲替、不补回 glad include。
**等待用户回报后进入 Task 5。**

---

## Task 5: 删除旧 GL 着色器文件

**Files:**
- Delete: `PlantVsZombies/Shader/batch_vertex.glsl`、`batch_fragment.glsl`、`geom_vertex.glsl`、`geom_fragment.glsl`、`sprite_vertex.glsl`、`sprite_fragment.glsl`
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj`（第 202–207 行）
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj.filters`（第 550–565 行）

> 这 6 个旧 GL 着色器**不在** `<ShaderSource>` 列表（不参与 glslc 预构建），仅以 `<None>` 出现。**保留** `batch.vert.glsl`/`batch.frag.glsl`（活跃 Vulkan 着色器，在 ShaderSource 中）。

- [ ] **Step 1: 删除 6 个磁盘文件**

删除 `PlantVsZombies/Shader/` 下：`batch_vertex.glsl`、`batch_fragment.glsl`、`geom_vertex.glsl`、`geom_fragment.glsl`、`sprite_vertex.glsl`、`sprite_fragment.glsl`。

- [ ] **Step 2: 移除 vcxproj 的 6 个 None 条目**

`PlantsVsZombies.vcxproj` 删除这六行（原约 202–207），**不要**动其下方的 `quad.*` / `triangle.*` / `batch.*` 行：

```xml
    <None Include="Shader\batch_fragment.glsl" />
    <None Include="Shader\batch_vertex.glsl" />
    <None Include="Shader\geom_fragment.glsl" />
    <None Include="Shader\geom_vertex.glsl" />
    <None Include="Shader\sprite_fragment.glsl" />
    <None Include="Shader\sprite_vertex.glsl" />
```

- [ ] **Step 3: 移除 vcxproj.filters 的 6 个对应条目**

`PlantsVsZombies.vcxproj.filters`（原约 550–565）删除这 6 个着色器各自的 `<None Include="Shader\…_….glsl">…</None>` 元素块（含内部 `<Filter>`）：`batch_fragment.glsl`、`batch_vertex.glsl`、`sprite_fragment.glsl`、`sprite_vertex.glsl`、`geom_fragment.glsl`、`geom_vertex.glsl`。**保留** `quad.*`/`triangle.*`（Task 7 处理）与 `batch.*`。

- [ ] **Step 4: 编译检查点（用户在 VS 验证）**

请用户重新生成并启动游戏，简单跑一下主菜单 + 进一关。
预期：**构建与 glslc 预构建均不受影响**（这些文件从不参与 glslc）；游戏画面与之前一致。
若 glslc 步骤报找不到某 `.spv`：回报原文（一般无，因仅删了非 ShaderSource 文件）。
**等待用户回报后进入 Task 6。**

---

## Task 6: `GLTexture` → `Texture` 全量改名

**Files（27 个含 `GLTexture` 的文件 + 结构体定义）：**
- Modify（定义）: `PlantVsZombies/ResourceManager.h`（`struct GLTexture`，第 22；成员 `mGLTextures`、`mAtlasPages` 等）
- Modify（引用）: `ResourceManager.cpp`、`Graphics.cpp`、`Graphics.h`、`ParticleSystem/Particle.h`、`UI/Button.cpp`、`UI/GameMessageBox.cpp`、`UI/Slider.cpp`、`Reanimation/Animator.cpp`、`Reanimation/Animator.h`、`Reanimation/Reanimation.cpp`、`Reanimation/ReanimTypes.h`、`Game/CardDisplayComponent.h`、`Game/CardDisplayComponent.cpp`、`Game/ChooseCardUI.h`、`Game/FlagMeter.h`、`Game/FlagMeter.cpp`、`Game/GameProgress.h`、`Game/GameProgress.cpp`、`Game/Bullet/Bullet.h`、`Game/ShadowComponent.h`、`Game/ShadowComponent.cpp`、`Game/Shovel.h`、`Game/Scene.h`、`Game/Scene.cpp`、`Game/ZombieAlmanacScene.cpp`、`Game/ShovelBank.h`

- [ ] **Step 1: 改结构体定义与注释（ResourceManager.h 第 21–22）**

```cpp
// 纹理信息（Phase 3b：id 字段语义变为 bindless slot 索引；过渡期保留 GLTexture 名称以避免大规模改名）
struct GLTexture {
```

改为：

```cpp
// 纹理信息：id = bindless slot 索引；vkTex 为拥有的底层 VulkanTexture（Unload 时回收槽位 + 销毁 image）
struct Texture {
```

- [ ] **Step 2: 全工程整词替换标识符 `GLTexture` → `Texture`**

对上列全部文件做**整词**（whole-word，词边界）替换 `GLTexture` → `Texture`。仅匹配独立标识符 `GLTexture`，不影响子串（已核实无 `XGLTexture` 类标识符）。覆盖 `struct GLTexture`、`const GLTexture*`、`std::unordered_map<std::string, GLTexture>`、`std::list<GLTexture>`、函数参数/返回/成员等全部出现处。

- [ ] **Step 3: 收敛关联成员名（仅 ResourceManager.h/.cpp）**

`ResourceManager.h`：`std::unordered_map<std::string, Texture> mGLTextures;` → 成员名 `mGLTextures` 改为 `mTextures`。在 `ResourceManager.cpp` 同步把所有 `mGLTextures` 引用改为 `mTextures`。
注意：`LoadTiledTextureGL` 这类**函数名不在本次改名范围**（非该结构体标识符；保守不动，避免误伤）。

- [ ] **Step 4: 编译检查点（用户在 VS 验证）**

请用户全代码库搜索确认无遗留 `GLTexture` 与 `mGLTextures` → 重新生成 → 启动游戏跑主菜单、图鉴、进一关、放植物。
预期：**编译通过**；全库无 `GLTexture`；游戏行为与改名前完全一致（纯改名）。
若报某处 `GLTexture` 未定义或 `mGLTextures` 未定义：回报原文（一般为漏改的引用，补改即可）。
**等待用户回报后进入 Task 7。**

---

## Task 7: 移除 `-Vulkan` 冒烟脚手架 + 命名/注释收尾

**Files:**
- Modify: `PlantVsZombies/main.cpp`（第 17–30 棋盘格辅助、32–163 `RunVulkanSmokeTest`、184–190 参数分支、6–9 冒烟专用 include）
- Modify: `PlantVsZombies/Renderer/VulkanRenderer.h`（第 30、32–42、44–46 及成员 79、81–83 + `QuadPushConsts`）
- Modify: `PlantVsZombies/Renderer/VulkanRenderer.cpp`（`VulkanRenderer::DrawFrame` 整函数，第 257 起）
- Delete: `PlantVsZombies/Shader/triangle.vert.glsl`、`triangle.frag.glsl`、`quad.vert.glsl`、`quad.frag.glsl` 及 `Shader/spv/triangle.vert.spv`、`triangle.frag.spv`、`quad.vert.spv`、`quad.frag.spv`
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj`（第 208–211 None、217–228 ShaderSource）
- Modify: `PlantVsZombies/PlantsVsZombies.vcxproj.filters`（第 568–571）
- Modify: `PlantVsZombies/Graphics.cpp`（第 800、826、888、896 + 977/996 注释）、`Graphics.h`（第 851–856 注释/声明、第 511 注释）

> 已 grep 核实：`DrawFrame`/`SetTrianglePipeline`/`SetTexturedQuad`/`ClearTexturedQuad`/`mTrianglePipeline`/`mQuadPipeline`/`mQuadDescSet`/`mQuadPC` **仅** `main.cpp` 调用；游戏主路径走 `BeginFrame`/`EndFrame`/`CurrentCmdBuffer`（保留）。Graphics.cpp:977/996、Graphics.h:511 中 “DrawFrame” 仅为注释提及。

- [ ] **Step 1: 删除 main.cpp 冒烟测试函数与辅助**

删除 `main.cpp`：`MakeCheckerboardRGBA`（原约 17–30，含其上注释）整函数；`RunVulkanSmokeTest`（原约 32–163，含其上注释）整函数。

- [ ] **Step 2: 删除 main.cpp 的 `-Vulkan` 参数分支**

删除 `main()` 中这一分支整块（原约 184–190）：

```cpp
		else if (arg == "-Vulkan" || arg == "-vulkan")
		{
			// 走 Vulkan 烟雾测试分支，不启动游戏
			int rc = RunVulkanSmokeTest();
			CrashHandler::Cleanup();
			return rc;
		}
```

- [ ] **Step 3: 清理 main.cpp 冒烟专用 include**

删除仅冒烟测试用到的 include（原约 6–9 与 11、15）：`#include "Renderer/VulkanContext.h"`、`#include "Renderer/VulkanRenderer.h"`、`#include "Renderer/VulkanPipeline.h"`、`#include "Renderer/VulkanTexturePool.h"`、`#include <SDL2/SDL_vulkan.h>`、`#include <vector>`。**保留** `CrashHandler.h`、`GameRandom.h`、`GameAPP.h`、`GameMonitor.h`、`<SDL2/SDL.h>`、`<cstdint>`、`<iostream>`、`<cmath>` 中游戏主路径仍需者。删除后若 `main.cpp` 报某 include 缺失，按报错补回该一行（保守）。

- [ ] **Step 4: 删除 VulkanRenderer 冒烟专用 API（VulkanRenderer.h）**

删除第 30 行 `void SetTrianglePipeline(VulkanPipeline* p) { mTrianglePipeline = p; }` 及其上方注释行 `// 设置当前帧要用的 pipeline。nullptr 表示只 clear。`

删除 `SetTexturedQuad`（原约 32–41，含上方两行注释）与 `void ClearTexturedQuad() {...}`（第 42）整块。

删除 `DrawFrame` 声明（原约 44–46，含上方两行注释 `// 渲染一帧…` / `// 留给 -Vulkan 烟雾测试用…`）：

```cpp
    bool DrawFrame(float r, float g, float b, float a);
```

在 private 段删除成员：`VulkanPipeline* mTrianglePipeline = nullptr;`（第 79）、`VulkanPipeline* mQuadPipeline = nullptr;`（第 81）、`VkDescriptorSet mQuadDescSet = VK_NULL_HANDLE;`（第 82）、`QuadPushConsts mQuadPC{};`（第 83），以及 `QuadPushConsts` 结构体定义本身（位于 private 段，grep `QuadPushConsts` 定位其 `struct QuadPushConsts { … };` 块并整体删除——它仅被 `mQuadPC` 与 DrawFrame 使用）。**保留** `BeginFrame`/`EndFrame`/`CurrentCmdBuffer`/`CurrentFrameIdx` 及其成员。

- [ ] **Step 5: 删除 VulkanRenderer.cpp 的 DrawFrame 实现**

删除 `VulkanRenderer.cpp` 中 `bool VulkanRenderer::DrawFrame(float r, float g, float b, float a) {`（第 257）起至其匹配右花括号 `}` 的整个函数体。仅删此函数，相邻函数不动。

- [ ] **Step 6: 删除 triangle/quad 着色器文件**

删除磁盘：`PlantVsZombies/Shader/triangle.vert.glsl`、`triangle.frag.glsl`、`quad.vert.glsl`、`quad.frag.glsl`，以及 `PlantVsZombies/Shader/spv/triangle.vert.spv`、`triangle.frag.spv`、`quad.vert.spv`、`quad.frag.spv`。**保留** `Shader/batch.vert.glsl`、`batch.frag.glsl` 及 `Shader/spv/batch.vert.spv`、`batch.frag.spv`。

- [ ] **Step 7: 移除 vcxproj 的 triangle/quad 条目**

`PlantsVsZombies.vcxproj`：删除这四行 None（原约 208–211）：

```xml
    <None Include="Shader\quad.frag.glsl" />
    <None Include="Shader\quad.vert.glsl" />
    <None Include="Shader\triangle.frag.glsl" />
    <None Include="Shader\triangle.vert.glsl" />
```

删除 triangle/quad 的 4 个 `<ShaderSource>` 元素块（原约 217–228）：`triangle.vert.glsl`、`triangle.frag.glsl`、`quad.vert.glsl`、`quad.frag.glsl`（每个含 `<Stage>` 子元素）。**保留** `batch.vert.glsl`/`batch.frag.glsl` 的 `<ShaderSource>`（原约 229–232）与 `<None Include="Shader\batch.vert.glsl" />` / `batch.frag.glsl`。`CompileShaders` 目标本身不动。

- [ ] **Step 8: 移除 vcxproj.filters 的 triangle/quad 条目**

`PlantsVsZombies.vcxproj.filters`（原约 568–571）删除 `quad.frag.glsl`、`quad.vert.glsl`、`triangle.frag.glsl`、`triangle.vert.glsl` 四个 `<None Include=…>` 元素（含内部 `<Filter>` 若有）。

- [x] **Step 9: `RenderTextToGLTexture` → `RenderTextToVulkanTexture`（已在 Task 6 提前完成）**

> 勘误：因 `RenderTextToGLTexture` 含 `GLTexture` 子串，会与 Task 6 的 `GLTexture`→`Texture` 子串替换冲突，故已在 Task 6 全局替换前用 `sed` 在 `Graphics.h`/`Graphics.cpp` 整词替换 `RenderTextToGLTexture` → `RenderTextToVulkanTexture`（含声明/定义/调用与 `[Graphics::RenderTextToGLTexture]` 日志前缀字符串，10 处，0 残留）。Task 6 全局子串替换随后安全执行，并顺带把 `mGLTextures`→`mTextures`（即原 Task 6 Step 3，子串替换等价完成）。**剩余仅注释文字微调**（“一张新的 GL 纹理”→“…Vulkan 纹理”）留待下方 Step 10 一并处理。

- [ ] **Step 10: 收尾过时注释**

`Graphics.cpp` 第 888 注释 `// RenderTextToGLTexture 走 glGenTextures/glTexImage2D，且要写共享的` 改为反映 Vulkan 现实，例如 `// RenderTextToVulkanTexture 走 VulkanTexturePool 上传，且要写共享的`。
其余明显误导的 GL 注释按就近原则顺手订正（如 `Graphics.h:800` 的 `ApplyTopClipRectToGL` 注释、`Graphics.h:511`/`Graphics.cpp:977/996` 的 “DrawFrame” 字样改为 “EndFrame/BeginFrame”），**不改任何函数名/代码**，仅改注释文字。范围限于已读到的明显过时注释，不做地毯式扫描。

- [ ] **Step 11: 最终编译 + 完整冒烟（用户在 VS 验证）**

请用户重新生成并启动游戏，依次验证：
1. 游戏正常启动（不带任何参数）；带 `-Vulkan` 参数时应作为未知参数被忽略、正常进游戏（不再有冒烟分支）。
2. 主菜单、图鉴（植物/僵尸）渲染正常。
3. 进一关：植物、僵尸、阳光、子弹渲染正常，动画无闪烁。
4. 一波僵尸：无缺失/错色/层级错乱。
5. UI 文字（计时、阳光数、波次）在场景之上正常显示（验证 `RenderTextToVulkanTexture` 改名无碍）。
6. 割草机/樱桃炸弹/坚果被啃等粒子正常。
7. `-Debug` 碰撞框绘制正常。

预期：编译通过；行为与 Phase 6 之前完全一致。
若任何环节异常：停下，报告具体场景/元素与报错原文。
**全部通过 = Phase 6 完成。** 建议用户此时按需提交（commits user-driven）。

---

## Self-Review（写计划后自查）

- **Spec 覆盖：** Spec 7 个工作单元 ↔ 本计划 Task 1–7 一一对应；死/活分类、glad 切断顺序（Task 3 改型先于 Task 4 删 include）、改名目标 `Texture`、冒烟 API 边界均落到具体 Step。无遗漏需求。
- **占位符扫描：** 无 “TBD/TODO/类似 Task N/酌情处理”。每个删除/改型 Step 给出确切文件、确切代码文本或确切元素。改名 Task 列出全部 27 文件并给出整词替换的确定性操作 + 编译 gate。
- **类型一致：** 全程统一 `GLuint`→`uint32_t`；`GLTexture`→`Texture`、`mGLTextures`→`mTextures`、`RenderTextToGLTexture`→`RenderTextToVulkanTexture` 命名前后一致。Task 间引用（保留集 vs 删除集）一致。
- **顺序安全：** Task 3（活跃符号脱 GL 类型）严格早于 Task 4（删 glad include），使 Task 4 成为干净的脱钩检查点；每 Task 独立可编译、单变量、可回退。
