---
name: reference-pvz-texture-minification-no-mipmap
description: 纹理池已有全量mipmap(a5b3184)，缩小绘制任意倍数不糊；此前无mipmap时>2:1欠采样发糊(卡牌0.46实证)
metadata:
  node_type: memory
  type: reference
  originSessionId: ff8d8ca7-e36c-40f2-b2f7-f429f26e18b2
---

**现状（2026-07-12 a5b3184 起）**：`VulkanTexturePool` 建图带完整 mip 链（上传 level 0 后级联 `vkCmdBlitImage` linear 生成到 1×1），采样器本就是 `MIPMAP_MODE_LINEAR + maxLod=VK_LOD_CLAMP_NONE`，shader 零改动自动按缩放比选级。**任何贴图缩小到任意倍数都不再欠采样发糊**。含能力检查：RGBA8 optimal tiling 不支持 BLIT_SRC/DST+SAMPLED_FILTER_LINEAR 时回退单级（LOG_WARN）。显存 +1/3（~54→72MB）。msvc-debug validation 零报错验证过。

**历史教训（改动前）**：双线性只能正确覆盖缩小 ≤2:1；CARD_SCALE 0.55(1.82:1)清晰、0.46(2.17:1)糊、定稿 0.5 恰在上限。Card.h 当年注释如此；有 mipmap 后该限制已不存在。

**How to apply:**
- mip 正确性前提 = staging 时预乘 alpha（rgb*=a，见 [project_pvz_premultiplied_alpha](project_pvz_premultiplied_alpha.md)）——若改上传管线勿破坏此顺序，否则 mip 级会渗白边。
- barrier 编排=逐级放行（i-1 级 DST→SRC→blit→SHADER_READ；mipLevels==1 退化为旧单 barrier 路径）；再动此文件必开 validation（msvc-debug 即 _DEBUG 自动开）。
- 小字号文字锐化另有解：AcquireTextTexture 2× 光栅化 + DrawCachedText scale=0.5（CardDisplayComponent kSunTextRasterSize=28）。
- resources 不在 git；改/覆盖资产前必须备份或先问主人（auto 分类器也会拦原地覆盖）。
