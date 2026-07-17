---
name: project-pvz-glyph-atlas-text-followup
description: "后续可操作项——glyph atlas 逐字形 quad 替换\"整串→纹理\"文本绘制；含动手前的约束清单与\"先量\"闸门"
metadata:
  node_type: memory
  type: project
  originSessionId: 32ac1cd8-8c32-4677-a0dc-a1aed4c24425
---

**状态:未启动的后续候选(2026-05-31 提出)。动手前必须先量——见闸门。**

**目标**：把动态数字文本从"整串→一张 GPU 纹理 + 256-LRU"换成 **glyph atlas + 逐字形 quad**。字形(0-9、`/` 等)只光栅化一次进图集，字符串拼成多个 quad 各采样自己的字形。变化的数字成本≈0，永不重新光栅化，惠及全游戏动态文本(阳光数/计时/分数/HP)。思想同 reanim instancing：固定资源 + 廉价逐实例引用。

**为什么**：现 `Graphics::DrawText`→`GetOrCreateTextTexture`(`Graphics.cpp:1065`)整串光栅化进 256 条 LRU(`TEXT_CACHE_MAX_SIZE` `Graphics.h:787`)。频繁变化的数字 = 每个新值一次 `TTF_RenderUTF8_Blended` + GPU 纹理 create/destroy；working set 超 256 即 thrashing(11000 僵尸开 HP 时爆降真因)。详见 [project-pvz-draw-view-hp-text-cull](project_pvz_draw_view_hp_text_cull.md)。

**⛔ 闸门(先量再动)**：HP 文字视口剔除(`IsWorldPointVisible`)已把可见串砍到几十<256→缓存命中→光栅化抖动已消失。**正常对局文本很可能已不在热路径**。启动前先测:开 HP + 剔除下，正常对局帧数还掉吗？`6.Draw_submit` 占比变了吗？不疼就搁置——ROI 不足，同 [project-pvz-precomputed-animation](project_pvz_precomputed_animation.md) 的判断逻辑。仅当"残余文本成本可量到"或"压测要稳开 HP"才做。

**动手前要协调的约束(今天挖出，是真难点)**：
1. **batch/instance 双队列 z-order**([reference-pvz-batch-instance-zorder](reference_pvz_batch_instance_zorder.md))：字形 quad 仍走 batch 路径，cross-flush 保序不变，逐字形不能破坏"当前层"语义。
2. **超采样**：`ComputeTextRasterSize(fontSize, m_letterboxScale, superSample)`(`Graphics.cpp:1058`)——字形按物理像素光栅化进图集，quad 再 /superSample 回逻辑尺寸。letterbox scale 变化时图集需失效重建(参考现 `m_textGeneration`/`ClearPinnedTextCache` 代际失效模式)。
3. **worker 不能建纹理**(`Graphics.cpp:1188` 未命中返空)——图集须主线程预热；**但预建后即静态 → worker draw 只引用字形 UV、无需创建 → 反而消除了这条限制**(对已覆盖字形)。这是 atlas 的额外红利。
4. **颜色**：现缓存按 color 进键；atlas 字形光栅化成白/alpha，靠 vertex color tint(守预乘 alpha 管线 [project-pvz-premultiplied-alpha](project_pvz_premultiplied_alpha.md) + 0..255 约定 [reference-pvz-color-0-255-convention](reference_pvz_color_0_255_convention.md))→ 一套图集服务所有颜色，去掉 color 维度，大赢。
5. **字形度量**：`TTF_GlyphMetrics` 取 advance/bearing 做布局。数字+`/` 简单、可跳过 kerning。

**建议增量范围**：只做 **数字 + 标点的 HUD 图集**(HP/阳光/计时)，**别碰中文 UI 串**(Unicode 太大，保留现整串路径)。拣起时走 brainstorming→writing-plans。
