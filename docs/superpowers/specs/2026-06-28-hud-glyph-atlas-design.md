# HUD 字形图集（方案A · 消尖峰）设计

日期：2026-06-28
分支：`feature/hud-glyph-atlas`
状态：设计已批准，待 writing-plans

## 1. 背景与问题（根因已坐实）

生存黑夜无尽后期（数百僵尸在屏、伤害密集时），出现**周期性掉帧**：`7.Draw_replay(serial)`
从 ~4.9ms 飙到 ~7.1ms，伤害停止后自动恢复。

根因：僵尸/植物血量是**每帧都变的数字串**，走「整串 → 一张 GPU 纹理」的 LRU 缓存
（`Graphics::GetOrCreateTextTexture`，键=整串，上限 `TEXT_CACHE_MAX_SIZE=1024`）。数百单位
同时快速掉血 → 每帧血量串全变 → LRU 被击穿 → 每帧上千次 `TTF_RenderUTF8_Blended` + 格式转换
+ GPU 上传 + 满载淘汰 `DestroyTexture`。

决定性一环：worker 并行记录阶段对 `DrawText` **只记 `DeferredText` 命令、不光栅化**
（线程不安全：TTF 渲染 / GPU 上传 / LRU 维护全部 defer）；真正的光栅化+上传发生在
**串行主线程的 `ReplayAndEndParallel`**，每行还各自 `FlushBatch()` + 强制 pipeline 重绑。
所以成本必然且只落在 `7.Draw_replay(serial)`，与 profile 一字不差。

两类成本：
- **① 光栅化 thrash**（随掉血节奏放大 → 尖峰主体）— 本设计要消灭的目标。
- **② 逐行 FlushBatch 地板**（随在屏单位数放大 → 稳态 4.9ms 一块）— 方案A **不动**，留待方案B。

## 2. 目标与范围

- **目标**：彻底消除 ① 光栅化 thrash 造成的周期性尖峰，z-order 与现状完全一致，风险最低。
- **范围**：两处 debug 血量叠层
  - 僵尸血量：`Zombie::Draw`，字体 `FONT_FZJZ`、字号 15，字符 = `本体一类二类` + `0-9` + `:` `/` 空格
  - 植物血量：`Plant::Draw`，字体 `FONT_FZCQ`、字号 17，字符 = `0-9` + `/`
- **非目标**（方案A 不做）：
  - 稳态逐行 FlushBatch 地板（属方案B：把字形 quad 发射也搬进 worker slice，需重新验证
    batch/instance 层序，验证方案A 消尖峰后再决定是否动）。
  - 通用 HUD 数字（阳光数/计时/分数）——变化慢、从不 thrash，YAGNI。
  - 现有 `DrawText` / `GetOrCreateTextTexture` / LRU 路径**保留不动**（中文 UI 串等继续用）。

## 3. 总体架构

新增一个**主线程构建、跨帧缓存**的字形图集，配一个新绘制原语 `DrawGlyphRun`。
字形只光栅化一次进图集，运行时逐字形拼 quad、用顶点色 tint。变化的数字成本 ≈ 0。

```
DrawGlyphRun(utf8, fontKey, fontSize, color, x, y, scale)
  ├─ worker (tl_record != null):  RecordDrawGlyphRun → 记轻量 DeferredGlyphRun cmd，return
  └─ 主线程 (串行 / replay):       FlushInstances() → GetOrBuildGlyphAtlas → 逐字形 append quad
```

### 3.1 数据结构

```cpp
struct GlyphInfo {
    float u0, v0, u1, v1;   // 图集内 UV
    int   pxW, pxH;         // 字形位图物理像素尺寸
    int   advance;          // 笔触前进（物理像素）
    int   bearingX;         // minx（物理像素）
    int   bearingY;         // maxy 相对基线（物理像素）
};

struct GlyphAtlas {
    pvz::VulkanTexture* vkTex = nullptr;   // 一张图集，一个 bindless 槽
    uint32_t            textureID = 0;     // vkTex->bindlessIndex
    int                 texW = 0, texH = 0;
    std::map<uint32_t, GlyphInfo> glyphs;  // codepoint → 字形
    std::set<uint32_t>  covered;           // 已纳入的码点集合
    int                 physSize = 0;      // 烘焙物理字号
    float               superSample = 1;   // physSize / fontSize
    uint32_t            generation = 0;    // 对齐 m_textGeneration（letterbox 失效）
    bool                dirty = false;     // 有新码点待加 → 下次重建
};
```

Graphics 持有 `std::map<GlyphAtlasKey, GlyphAtlas> m_glyphAtlases`，
key = `(fontKey, fontSize)`（physSize 由 letterbox 派生、随 generation 失效）。

### 3.2 图集构建（主线程懒构建 + 按需扩字符集）

`GetOrBuildGlyphAtlas(fontKey, fontSize, needed) -> GlyphAtlas&`（`needed` = 本次绘制串的码点集合）：
- 把 `needed` 里 `covered` 没有的码点并入 `covered`。
- 若图集不存在、或 `generation != m_textGeneration`、或 `covered` 本次有新增 → (重)建。
- 构建流程：
  1. `physSize = ComputeTextRasterSize(fontSize, m_letterboxScale, superSample)`（与文字同款超采样）。
  2. 对 `covered` 全集逐码点：`TTF_GlyphMetrics32` 取度量；`TTF_RenderGlyph32_Blended` 取
     位图（白色，alpha=覆盖度）。
  3. 行式打包进一张 `SDL_Surface`（简单 shelf 打包即可，字形数 ≤ 二十几个）。
  4. `SDL_ConvertSurfaceFormat(ABGR8888)` → `CreateTextureRGBA8`（**复用现有上传路径** →
     预乘 alpha / 格式 / 超采样语义自动与现文字一致）。
  5. 旧 `vkTex` 若存在先 `DestroyTexture` 回收 bindless 槽。
  6. 填 `glyphs` 的 UV/度量，清 `dirty`，`generation = m_textGeneration`。

按需扩（**当帧收敛**）：`DrawGlyphRun` 先 UTF-8 解码出本串全部码点，把缺的并入 `covered` 后
**当帧立即重建整张图集**（**不做子区域纹理更新**，整张重建对 ≤ 二十几字形完全够用）。因此新串
出现的**当帧**图集即对它完整——僵尸标签字（`本体`/`一类`/`二类`）在第一个画血量的僵尸处一次建好，
此后所有数字串全命中，永不再建。`dirty` 字段在此设计下退化为"covered 本次有新增"的局部判断，可不入结构体。

**关键约束**：所有 TTF 渲染 / 纹理创建 / map 写入只发生在主线程（串行 `DrawGlyphRun` 或 replay
内）。worker 永不进入构建路径 → 天然满足"worker 线程不能建纹理"约束，无并发写 map 风险。

### 3.3 `DrawGlyphRun` 主线程发射

1. UTF-8 解码出本串全部码点 `needed`。
2. `FlushInstances()`（保 z-order：文字落在已画对象之上、后续对象之下，与 `DrawText` 一致）。
3. `GlyphAtlas& atlas = GetOrBuildGlyphAtlas(fontKey, fontSize, needed)`；建失败 → fallback `DrawText(整串)` 后 return（见 §6）。
4. `texIndex = BindTexture(atlas.textureID)`。
5. 逐码点（**此时所有字形必在图集中**，无缺字形分支）按 `GlyphInfo` 拼一个 quad：
   - 屏幕逻辑位置：`penX += advance / superSample * scale`，字形左上
     `(x + (penX0 + bearingX)/superSample*scale, y + (ascent - bearingY)/superSample*scale)`
     （`ascent = TTF_FontAscent(physFont)`，用于把"基线对齐"换算成"行顶对齐"，与现整串渲染的
     行盒顶对齐口径一致；细节在实现期对照像素微调）。
   - quad 逻辑尺寸：`pxW/superSample*scale` × `pxH/superSample*scale`。
   - UV：`atlas.glyphs[cp]` 的 `u0,v0,u1,v1`。
   - 顶点色：`color`（0..255，经 NormalizeColor → tint）。
   - `AddVertices(6)` + `CheckBatch()`（与 `DrawText` 末段同款 batch 路径）。

### 3.4 worker defer + replay 就地发射（镜像 DeferredText）

- 新增 `RecCmdType::DeferredGlyphRun`、`struct DeferredGlyphRunCmd{ text; fontKey; fontSize; color; x; y; scale; }`、
  `WorkerRecord::glyphRunCmds`。
- `RecordDrawGlyphRun(r, ...)`：push 一条 `DeferredGlyphRun` cmd（记 `vertOffsetAtCmd` /
  `instOffsetAtCmd` / `payloadIdx`），**零图集访问、零光栅化**。
- `ReplayAndEndParallel` 新 case，**逐字镜像** `DeferredText` 的 `onTop=false` 分支：
  ```cpp
  case RecCmdType::DeferredGlyphRun: {
      const DeferredGlyphRunCmd& t = r.glyphRunCmds[cmd.payloadIdx];
      DrawGlyphRun(t.text, t.fontKey, t.fontSize, t.color, t.x, t.y, t.scale);
      FlushBatch();
      boundPipe = BoundPipe::None;   // 强制后续几何重绑管线
      break;
  }
  ```
  `emitUpTo` 已把本对象 sprite emit 完 → 文字就地夹在本对象与后续对象之间 = 当前层 z-order，
  与现状逐字一致。血量只需"当前层"，**无 onTop 变体**。

## 4. 接入点

- `Zombie::Draw`（约 584/589/592/595，`drawLine` lambda 内）：`g->DrawText(...)` → `g->DrawGlyphRun(...)`（FZJZ, 15, lightBlue）。
- `Plant::Draw`（约 144）：`g->DrawText(...)` → `g->DrawGlyphRun(...)`（FZCQ, 17, green）。
- 两处现有的 `IsWorldPointVisible` 视口剔除守卫**保留**（屏外不画，省 VBO + CPU）。

## 5. 不变量与守则

- **预乘 alpha**（[[project_pvz_premultiplied_alpha]]）：图集走现有 `CreateTextureRGBA8` 上传路径
  （rgb*=a），frag 把 vColor.a 预乘进 rgb；复用上传路径自动满足。
- **颜色 0..255**（[[reference_pvz_color_0_255_convention]]）：`DrawGlyphRun` 的 `color` 同 `DrawText`，0..255。
- **z-order 当前层**（[[reference_pvz_batch_instance_zorder]]）：replay 就地 + `FlushBatch` + `boundPipe=None`。
- **worker 不建纹理**：图集只在主线程（串行 / replay）建。
- **letterbox 失效**：`generation` 对齐 `m_textGeneration`，`ClearTextCache`/`ClearPinnedTextCache`/
  `RecomputeLetterbox` 时一并令图集失效（实现期定具体钩子；图集纹理也要在 Shutdown 前还给 pool）。

## 6. 回退与边界

- **建图集失败 / 当前帧码点缺失** → 该 `DrawGlyphRun` 内部 fallback 到 `DrawText(整串)`，
  保证文字永不消失（仅退化为旧路径，不崩、不空白）。
- **排版精度**：HP 串无需 kerning（数字/标点/少量 CJK 等宽足够）；逐字形 advance 排版与现整串
  渲染可能有亚像素差异，可接受；实现期对照像素微调 ascent/bearing 口径。

## 7. 验证（主人手动，不用 AutoTest）

> AutoTest 名表含未实现植物（高坚果/地刺/忧郁菇等）会一建即崩 + 触发杀毒拦 shell，本任务的
> 验证由主人在 VS / 真实游戏里完成（[[project_pvz_zombie_hp_text_thrash]]）。

- 编译：`clang-release` 零警告零错误。
- 真实游戏：开 `mShowZombieHP` / `mShowPlantHP`，生存黑夜无尽后期，看 `-Profile` 三行：
  - `textRaster(miss)/frame` → ~0（血量不再走整串光栅化；若别处还有少量普通 `DrawText` 会有残余）。
  - `7.Draw_replay` 周期性尖峰消失。
- 目视：血量数字清晰、颜色对（僵尸浅蓝 / 植物绿）、正确叠在身体上、屏外不画。
- 诊断探针（已在工作树未提交：`Profiler.h` 的 `CountText`、`Graphics.cpp` 的
  `7a.TextRaster_miss`/`CountText`/`CountFlush` 接线）随本 feature 一并保留/提交，作为验证手段。

## 8. 提交边界

- 本 feature 改动文件：`Graphics.h` / `Graphics.cpp`（图集 + 原语 + record + replay）、
  `Game/Zombie/Zombie.cpp`、`Game/Plant/Plant.cpp`，加诊断探针文件。
- 工作树里 `Game/Board.h`、`Game/Plant/GameDataManager.cpp` 的改动是主人 DoorZombie WIP，
  **本 feature 完全不碰**，提交时不 `git add` 这两个文件。

## 9. 方案B（后续，本次不做，仅备忘）

把字形 quad 的发射从串行 replay 搬进 worker slice（仿 `RecordDrawTextureRegion` 的 `EmitQuad`），
额外并行化发射 + 字形 quad 自然合批去掉逐行 flush 地板。收益更大，但需重新验证 batch quad 与
reanim instance 在 worker slice 内的层序（历史老坑）。**待主人验证方案A 消尖峰、并确认稳态地板
仍值得动时再启动。**
