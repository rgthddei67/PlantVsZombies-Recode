---
name: project_pvz_zombie_hp_text_thrash
description: "✅ 已完成(2026-07-02 主人真实游戏验证尖峰消失)：生存后期血量数字掉帧根因=整串→纹理 LRU 击穿；HUD 字形图集(方案A)+VBO 预留+基线修复+审查收口全在 master(未 push)；留作参考=foot-gun/调试教训"
metadata:
  node_type: memory
  type: project
  originSessionId: 0e403cac-79de-4744-a8da-9dd044cb85b4
---

**第二季续集（2026-07-07）**：字形图集治好光栅化 thrash 后，20000 全可见僵尸又暴露"defer 到串行 replay 逐行 FlushBatch"的 N×ε 瓶颈（19FPS），已由 [project_pvz_glyph_run_worker_instancing](project_pvz_glyph_run_worker_instancing.md) 根治（worker 录制期直接 instance 化）。

2026-06-28：生存黑夜无尽后期数百僵尸快速掉血 → 血量数字「整串→纹理」LRU(上限1024)被击穿 →
串行 `7.Draw_replay` 光栅化尖峰(4.9→7.1ms)。**已实现 HUD 字形图集(方案A)修复**，在
`feature/hud-glyph-atlas` 分支(**未 push**)，5 commit：plan(bbda8e7)/Task1 图集子系统+串行
DrawGlyphRun(98f1b97)/Task2 worker defer+replay(527b022)/Task3 接入 Zombie+Plant(4e1f72b)/
**fix VBO 预留溢出(b8ab956)**。spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-28-hud-glyph-atlas*`。

## 方案A 实现(已编译零警告 + AutoTest 验证)
- `GlyphAtlas`(单行打包白色字形,一个 bindless 槽,按 (fontKey,fontSize) 缓存)+ 新原语
  `Graphics::DrawGlyphRun`(逐字形 quad + 顶点色 tint，**烘白+NormalizeColor tint 数学等价旧烘色路径**)。
  复用现有 TTF→ABGR8888→CreateTextureRGBA8 上传路径 → 预乘 alpha/超采样自动一致。
- **当帧收敛**：DrawGlyphRun 解码整串 → 缺码点并入 covered → 当帧重建整张图集(physSize 变化也重建)。
- **worker 零光栅化**：`RecordDrawGlyphRun` 只记轻量 `DeferredGlyphRun` cmd；`ReplayAndEndParallel`
  新 case 就地发射(逐字镜像 DeferredText onTop=false：DrawGlyphRun+FlushBatch+boundPipe=None)，
  z-order 与现状一致。建图集失败 fallback DrawText。接入点 `Zombie::Draw`(FZJZ@15)/`Plant::Draw`(FZCQ@17)。

## ⚠️ 关键 bug + 修复(b8ab956，本族 foot-gun)
DrawGlyphRun 把 replay 阶段每行血量从 1 quad 膨胀到逐字形十几 quad(**顶点 ×13**)。
`BeginParallelRecord` 的 **post-parallel 预留是固定 4MB VBO / 1MB SSBO**(replay 的血量只能写这里)，
数百僵尸时被撑爆；而 grow 只翻倍 worker 切片区、**不增固定预留** → 每帧 vbo 翻倍 16→32→…→**2GB →
vmaCreateBuffer 失败**。ssbo 因字节比(顶点:矩阵≈4.125:1)与预留比(4:1)相近刚好不爆 → 现象=**vbo 爆/ssbo 稳**。
**修复=预留比例化** `max(下限, remain/4)`(vbo 下限 8MB / ssbo 下限 2MB，匹配血量 4:1)→ grow 时
预留同步放大 → 溢出自愈、正反馈打破、一两帧收敛。教训：**任何增大 replay/post-parallel 顶点量的
功能，都要检查 BeginParallelRecord 的固定预留会不会被撑爆 + 能否随 grow 自愈**。延续
[project_pvz_host_visible_buffer_grow_on_demand](project_pvz_host_visible_buffer_grow_on_demand.md)。

## ⚠️ 第二个 bug + 修复(2026-06-29，主人自行 commit)：字形基线错位
血量「本体: 270/270」三段文字不在同一行(汉字偏上/冒号偏下/数字居中)。**根因=对 SDL 行为的错误假设**：
`TTF_RenderGlyph_Blended` 单字形返回的**不是紧致位图，而是整行高(=TTF_FontHeight)、字形已按基线摆好的
surface**(顶部留白 = `ascent-maxy`，所有字形基线统一落在第 `ascent` 行；探针实测 fzjz：surf_h 恒=FontHeight，
墨迹行恰为 `[ascent-maxy, ascent-miny)`)。`BuildGlyphAtlas` 把这张 surface 整张入图集、把 `surf_h` 当字形高存，
`DrawGlyphRun` 又叠加一次 `(ascent-bearingY)` 偏移 → **基线对齐量算两遍(double-count)**：观测 baseline =
正确 + `(ascent-maxy)`，低 maxy 的冒号被多下推最多、汉字最少。**非 SDL 的锅**——旧「整串→纹理」路径用的也是
同一个 SDL2_ttf，靠它内部统一排基线所以从不错位；回归纯是新逐字形代码的错误假设。
**修复(Graphics.cpp BuildGlyphAtlas)=入图集时按真实墨迹框 `[minx, ascent-maxy, sz_width=maxx-minx,
sz_rows=maxy-miny]` 裁成紧致位图(+越界 clamp)**，GlyphInfo 回到紧致口径 → `DrawGlyphRun` 那条本就为紧致字形
写的基线公式 `baseline=gy+bearingY=y+ascent` 一行未改、自动成立。clang-release 0warn；主人真实游戏验证。

**过程教训(最值钱)**：我早就形成了正确假设(double-count)，却因"读 SDL 源码`*h=maxy-miny`证明 surface 紧致"
而放弃它；但症状稳健地指回 double-count。**当症状反复与"读代码得出的证明"矛盾，是你读代码的某个前提错了
——别再读，去测量那个值。** 30 行独立 C++ 探针(`#define SDL_MAIN_HANDLED`+cl 链 vcpkg 静态库 SDL2_ttf/
freetype/libpng16/zs/brotli*/bz2/SDL2-static + win32 系统库)直接调真实库打印 surf_h/墨迹 bbox，几分钟定了
读一小时源码读错的事。**别推断库的行为，调用它打印出来**(freetype-py 装 scratchpad 也能复刻同一 FreeType 度量)。
延续 [feedback_collaboration_style](feedback_collaboration_style.md) 的 measure-first 到"测量库行为"维度。

## 调试坑(记牢)
- **AutoTest 的 overflow/vma warning 在 exe 的 stdout/stderr，不在 run.log**(run.log 只是命令 trace)；
  跑 exe 时 `| Out-Null` 会把它丢掉 → 误判"无 overflow"。验证 Logger 输出要 `1>out.txt 2>err.txt` 再 grep。
- **复现 HP overflow 用 AutoTest 是可行且安全的**：`{"op":"show_zombie_hp","on":true}` 开血量 +
  spawn 大量 **已实现** 僵尸(`ZOMBIE_NORMAL`，避开未实现类型崩溃坑)。普通关只 5 行(row 0-4)，
  `row%5`；要超 4MB 阈值需 ~1600+ 行血量(2500 普通僵尸稳触发)。TestDriver 单帧最多执行 64 条命令。
- 对照实验法：`git stash push -- <file>` 暂存修复→重编"修复前"→跑确认 bug→`stash pop`→重编→验证消失。

## 2026-06-30 max 代码审查 + 5 处隐患收口(commit 992304f，未 push)
字形图集全链(98f1b97/527b022/4e1f72b/b8ab956)+ 基线修复 e587388 **现都在 master**(前述"feature 分支未 push"已 stale)。
主人请审"优化文字显示+修复文字位置"两改动。**max 审查结论=无任何影响当前 HUD 行为的真 bug**；逐一证伪的承重不变量(免重审)：
- **replay 期重建图集不会 use-after-free**：`VulkanTexturePool::DestroyTexture` **延迟回收 bindless 槽**——销毁只入
  `mPendingDeletions`，`FreeBindlessIndex` 要等 `ReclaimDeletion` 过 `FRAMES_IN_FLIGHT` 帧才调(308-323行注释明说"槽位此时
  不归还，否则新纹理 WriteDescriptor 覆盖仍被 in-flight 帧采样的 descriptor")→ 同帧早先引用旧槽的字形 quad 仍采样有效旧图集。
- **glowing(Add 混合)僵尸血量文字不会渲染错**：`BatchVertex.blendMode` 在 DrawGlyphRun 顶点硬编码 `0.0f`=Alpha，
  `AddVertices`(962)不覆盖它 → 与 `m_currentBlendMode` 解耦，恒 Alpha(同 DrawText)。
- **post-parallel 字形顶点溢出有界自愈**：`FlushBatch`(838-855)`cursor+bytes>cap` 检查→置 overflow+记 `frameVboDemand`+丢绘制，
  绝不越界写 GPU；`EndFrame`(616)按 demand 扩容。b8ab956 比例化预留打破"翻倍→预留不变→仍溢出"正反馈。
- **无数据竞争**：`m_glyphAtlases` 只主线程触碰(串行 DrawGlyphRun + replay)，worker 只记 cmd 不碰图集。
- **BindTexture 是 bindless 直通**(返回 ID 本身)→ 多 quad 共享一次 BindTexture、循环内 CheckBatch 中途 flush 也安全；
  CheckBatch 只看 `m_batchVertices.size()`。
**修掉的 5 处潜在隐患(均当前不触发，防御性)**：①DrawGlyphRun 缺字形整串回退 DrawText(原会静默吞字+不推进 penX)
②BuildGlyphAtlas 显式跳过非 BMP 码点(>0xFFFF，(Uint16)截断会别名到错 BMP 字形)③移除 vestigial `m_batchTextures`
(bindless 后只 push 不读，注释还误称"CheckBatch 会读 size")④Zombie 碰撞箱禁用补 `mCollider` 判空(读档+进入死亡两处，
统一 494 行 if(mCollider) 模式)⑤DrawGlyphRun 头加 @warning(仅限 HUD 短串，长文本撑爆单行图集)。clang-release 0warn/exit0。

## ✅ 任务完成(2026-07-02)
- **主人已在真实游戏验证消尖峰**(textRaster(miss)≈0、`7.Draw_replay` 尖峰消失)。全部 commit 在 master，
  feature/hud-glyph-atlas 分支已删除。push 待主人。
- 方案B(可选后续，未做也无需做除非稳态地板再成瓶颈)：字形 quad 发射搬进 worker slice(消稳态逐行 flush 地板)，
  需重验 batch/instance 层序。
