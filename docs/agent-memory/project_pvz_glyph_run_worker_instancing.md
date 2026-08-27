---
name: project_pvz_glyph_run_worker_instancing
description: 2026-07-07字形quad改在worker直接instance化；2026-08-27满血本体行改为紧致共享整行实例，590071→410071 instances、约134→137-138FPS；保留动态字形回退与候选裁决
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

## 2026-08-27：满血共同文本的紧致整行实例

`build/clang-release/saves/level46_data.json` 在血量开启、僵尸数仍稳定时有约 `590071 recordInstances`、`338M gpuFragmentInvocations`，总帧时约 7.43～7.52ms（约 133～134.5FPS）。新增窄快路：`Zombie::Update()` 只在主线程为 `mBodyHealth == mBodyMaxHealth` 的本体行取得 pinned `CachedText`；Draw 当帧复核 current/max、句柄和代际，任一不匹配立即回原 `DrawGlyphRun`。头盔/盾牌和受伤后的短命数值不缓存，避免重演 1024-LRU/历史值纹理膨胀。

`AcquireTextTexture` 的 pinned surface 会扫描 alpha，裁掉四周全透明像素并在 `CachedText::offsetX/Y` 保存原 surface 原点；`DrawCachedText` 补回偏移。默认 Vulkan worker 把该纹理按原调用位置写成一条 `InstanceRecord`，`-NoInstance`、OpenGL 和裁剪场景保留六顶点 batch，因此对象交错顺序不变。最终同规模为 `410071 recordInstances`、约 `340M fragments`，稳定窗口 7.23～7.30ms（约 137～138FPS）；另一次同源码 A/B 出现 7.14ms/140.1FPS，但最终复跑未稳定越过 140，不能把它宣传成稳定 140+。

两个否决候选必须保留：①未裁透明边的单整行虽然同为 410071 instances，却把 fragments 推到约 346M，只有约 125～135FPS；②按空格拆成标签/数值两张紧致纹理，fragments 降到约 336M、instances 430071，但每只僵尸交替两张纹理破坏采样局部性，GPU 回到约 6.9～7.1ms、仅约 129～134FPS。结论是“少实例/少片元”均不足以单独预测 GPU 结果，必须同场 Profile。

真实存档 AutoTest 默认不读 `saves/`，脚本必须显式加只读 `-AutoTestLoadSave`；只写 `goto_level 46` 会新开空关。该档僵尸随后撞小推车，8 秒后数量会从 20000 降到约 6691，性能比较只取事件爆发前对象数与实例数稳定的早期 60-frame 窗口。最终 `clang-release` 构建通过；`smoke_zombie_hp_glyph` 在默认 Vulkan、`-NoInstance`、强制 OpenGL 均 exit 0 且截图一致；`smoke_imitater_morph`、`smoke_zombie_frozen_parallel_layer`、`smoke_pool_instanced_shadows` 的默认/`-NoInstance` 可见截图均通过，覆盖换图/白化/云团、冰晶与 glow 交错、睡莲阴影层序。
