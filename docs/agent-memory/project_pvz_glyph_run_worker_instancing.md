---
name: project_pvz_glyph_run_worker_instancing
description: 2026-07-07(db5c3e6) 修20000血量行打爆串行replay——字形quad在worker录制期直接instance化，51.9→9.7ms；N×ε教训+profiler计数器盲区foot-gun
metadata:
  node_type: memory
  type: project
  originSessionId: adafb045-e8c5-44d6-bfab-9c6d02ba4b99
---

2026-07-07（db5c3e6，合 master 未 push）修"开僵尸血量 130→25FPS"第二季。

**根因**（与 [project_pvz_zombie_hp_text_thrash](project_pvz_zombie_hp_text_thrash.md) 的光栅化 thrash 是完全不同的病）：字形图集健在、零光栅化，但 `DrawGlyphRun` 在 worker 只打包参数、quad 发射全 defer 到串行 replay，每行一次 FlushBatch+管线重绑+打断 instancing。单行 ~1.5µs 无罪，20000 可见行/帧聚合成 Draw_replay 0.04→31.8ms。**N×ε 模式：单次无罪、聚合致死；压测要压"剔除后幸存者数量"不是总量**（11000 时可见密度低没暴露）。

**修法**：`RecordDrawGlyphRun`（Graphics.cpp）加快路径——图集就绪时 worker 把字形拼成 `InstanceRecord` 直写本 slot 切片，与 reanim 精灵同管线（两 frag shader 逐字节相同，预乘 alpha 契约天然一致）；记录顺序即绘制顺序 z-order 不变；Alpha 用 AppendReanimInstance 同款 SetBlend 包夹恢复；慢路径兜底（首帧/新码点/letterbox 变/超 64 码点）defer 主线程 replay 构建、下帧自动回快路径。实测 51.9→9.7ms(103FPS)，血量显示总成本 ~1.7ms。

**Foot-gun（调试期真踩的）**：
- `textDraw(lines)` 计数器只埋在 `GetOrCreateTextTexture`（DrawText 整串路径），**字形快路径完全不经过它**——"18 行"其实是 HUD 文字，血量 2 万行是计数盲区，误导了两轮。已加 `glyphRun(lines)`/`glyphAtlasBuild`/`glyphFb(build|missing)` 计数器 + replay 细分计时（7a-7d/FBa/FBb）留作回归探针，全部 -Profile 门控。
- `GetOrBuildGlyphAtlas` 用 textureID==0 同时表示"未建"和"建失败"→建失败会每帧重建循环（本次未命中但结构性弱点仍在，negative cache 缺失）。
- AutoTest 僵尸名是 `ZOMBIE_TRAFFIC_CONE` 不是 `ZOMBIE_CONE`。
- Profiler 计数器非线程安全，worker 快路径不能调 CountGlyphLine——glyphRun(lines) 修后=回退行数，应为 0。

**验证**：smoke_zombie_hp_glyph.json（show_zombie_hp op + 截图）+ 主人真机 2w 存档 profile。与大佬争论的裁决：双方都对——"画字便宜"(µs 级)成立，"开血量掉帧"也成立，烧的是批次碎片化不是绘制本身。
