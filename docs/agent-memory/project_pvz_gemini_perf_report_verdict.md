---
name: project_pvz_gemini_perf_report_verdict
description: "Gemini 对 Animator/Graphics/Shader 三文件出的性能报告(Performance_Analysis_Report.md)逐条裁决——核心论点\"GPU带宽瓶颈\"被实测CPU-bound证伪,7条全不建议动"
metadata:
  node_type: memory
  type: project
  originSessionId: e345dbd6-aaf6-4767-9250-28c59ab8f03b
---

2026-06-30：Gemini 只读 `Animator`/`Graphics`/Shader **三个文件**(非全项目、从未跑 profiler)出了 `Performance_Analysis_Report.md`,逐条核实代码+对照实测后**整份不建议动手任何一条**。

**核心病根=瓶颈归因反了。** 报告中心论点"瓶颈指向显卡纹理单元/显存带宽,非CPU"——与本项目实测**完全相反**:11000z/168.5FPS/5.94ms 下 GPU `Present` 0.14ms、`replay` 0.03ms(几乎免费),CPU 侧 par-record 2.37/GOM_Update 1.85/Collision 1.33 吃满帧时间=**完全 CPU-bound**(见 [project_pvz_perf_optimization](project_pvz_perf_optimization.md))。外部审查者看不到测量就把"代码里看得见的东西"默认成瓶颈,与 [project_pvz_gemini_vulkan_review](project_pvz_gemini_vulkan_review.md)(6条4误判)、[project_pvz_dalao_geometry_compute_suggestion](project_pvz_dalao_geometry_compute_suggestion.md) 同源模式。

**逐条裁决(均已核实代码锚点):**
- **问题一 Pipeline切换** ❌ 误判+修法错:Overlay/Glow 两路是**条件触发**(`Animator.cpp:417,429` `if(mEnableExtraOverlayDraw/AdditiveDraw)`),压测绝大多数对象只 append 一条 Alpha 不切换;replay 有 `boundPipe` 哨兵已消冗余绑定(`Graphics.cpp:2192`);replay 总共 0.03ms 无空间。**且建议的"传RGB倍率进shader避免切换"解决不了——颜色tint本就走顶点色(colorRGBA8→vColor),两pipeline差的是blend方程(Alpha vs Add)=Vulkan固定功能状态,不能用shader颜色乘法表达。Gemini把"颜色"和"混合模式"搞混。**
- **问题二 batch改2×3** ⚠️ 事实对(`batch.vert` 确用 mat4,`reanim_inst.vert` 用2×3)但"降15%CPU"虚构+**打错地方**:压测~100%对象走 instance 路径(`DrawInternal` hasChildren==false→`DrawInternalInstanced`),batch 路径只剩文字/阴影/UI 少量绘制,且深度耦合刚稳定的 HUD 字形图集([project_pvz_glyph_atlas_text_followup](project_pvz_glyph_atlas_text_followup.md)),风险>收益。
- **问题三 纹理压缩(报告的"高优先级")** ⚠️ 事实对(`VulkanTexturePool.cpp:259` 确是 `VK_FORMAT_R8G8B8A8_UNORM` 未压缩、mipLevels=1)但每层结论断:①打的不是瓶颈(present 0.14ms,"220-240FPS"虚构)②Gemini 建议"运行时压缩上传"给 CPU-bound 帧**加** CPU 方向反③**BC3 有损块压缩会撞预乘 alpha 不变量→白边可能复现**(Gemini 不知这段历史,见 [project_pvz_premultiplied_alpha](project_pvz_premultiplied_alpha.md))④纹理本就只~54MB 非内存大头(大头是逐帧host-visible缓冲,已 grow-on-demand 解决,见 [project_pvz_host_visible_buffer_grow_on_demand](project_pvz_host_visible_buffer_grow_on_demand.md))。
- **问题四 帧事件 multimap** ⚠️ 微观属实(`Animator.h:71` `unordered_multimap` 缓存差)但**实际可忽略**:`equal_range(f)` 只在跨整数帧查(`Animator.cpp:271`),且绝大多数 animator 的 mFrameEvents **空**(帧事件按 CLAUDE.md 须先问主人才加),空表 equal_range≈O(1);"拖累1ms+"虚构,这块已并行。
- **Early-Z 预排序(下一步)** ❌ 对 alpha 混合 2D 根本不适用:PvZ 半透明须 back-to-front 画家算法,Early-Z 要求 front-to-back 不透明,排了会破坏混合。Overdraw 在混合管线只能靠视口/遮挡剔除治(见 [project_pvz_draw_view_hp_text_cull](project_pvz_draw_view_hp_text_cull.md)),不是深度测试。
- **横向对比表(Godot/Unity FPS)** ⚠️ 纯臆测,"预估帧率"无任何真实基准。

**唯一有价值的事实**=纹理确实未压缩 RGBA8,但"对的事实、错的结论",当前瓶颈+预乘alpha约束下不该碰。**再压 FPS 的真实序(已测)**仍在 CPU 侧:①par-record视口剔除(先量屏占比)②GOM 的1.13ms 2d串行段③collision(低ROI);主人已选"够了停在这"。

**通用教训**=瓶颈归因必须靠 profiler,不能靠读代码+核显常识猜;报告里"if 13k对象每帧触发回调"这种"若"承担整个论证=把假设当事实。再有人拿"纹理压缩/Early-Z/pipeline合并/batch改2×3"提渲染优化,直接引此裁决免重新勘察。
