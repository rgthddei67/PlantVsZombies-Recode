---
name: project-pvz-premultiplied-alpha
description: PvZ 渲染现为预乘 alpha 管线（修白边）——加纹理上传路径/新混合模式必须守此不变量
metadata:
  node_type: memory
  type: project
  originSessionId: 688c6d48-443f-4e94-ac69-5dbab67d2087
---

2026-05-30：为修复精灵边缘白边/光晕（透明 PNG 像素藏白色 RGB + 双线性过滤 + 直通 alpha 混合三者叠加所致），把渲染改为**预乘 alpha 管线**。该契约跨三层，缺一即回归（白边复现或发光/淡出整体变暗）：

1. **上传**：`VulkanTexturePool::UploadPixels` 拷入 staging 时 `rgb = rgb*a/255`（带 +127 四舍五入）。这是所有纹理（LoadTexture / tiled / reanim / 粒子）唯一咽喉点 → 新增上传路径也会自动预乘。
2. **混合**：`VulkanPipeline.cpp` alpha & additive 两处 `srcColorBlendFactor = ONE`（不是 SRC_ALPHA，因 rgb 已乘 a）。dst 系数不变。
3. **着色器**：`batch.frag.glsl` + `reanim_inst.frag.glsl` 输出 `vec4(t.rgb*vColor.rgb*vColor.a, t.a*vColor.a)`。把 vColor.a 预乘进 rgb，使 **CPU 端 vColor 仍是直通语义**（传色/淡出/染色 C++ 代码一行不用改）。

**Why**：预乘是跨"上传→混合→着色"的原子不变量；动手前先纸上证明新旧每像素输出完全相等（只边缘插值不再渗白），才避免发光/淡出回归——这是预乘 alpha 的典型坑。
**How to apply**：(1) 加新混合模式时 srcColor 记得用 ONE 系语义；(2) 若出现旧式 `srcColorBlendFactor = SRC_ALPHA` 或着色器 `t*vColor`，是漏改的预乘断点；(3) shader 改完由当前 CMake Shader target 自动用 `glslc` 生成对应 preset 的 `Shader/spv/`，不得只改 GLSL 后沿用旧 SPV；(4) 最终仍须用 `clang-release` 产物验证。

2026-08-23 模仿者新增 `WashedOut` / `LessWashedOut` 纹理滤镜。因为采样值已经预乘，shader 必须先以
`rgb / alpha` 还原直通颜色，再执行原版 HSL 的 lightness/saturation 变换，最后把结果重新乘 alpha；直接在预乘 RGB 上
做 HSL 或叠纯白都会让半透明边缘过白。该契约同时覆盖 Vulkan batch、Vulkan reanim instance 和 OpenGL 3.3 batch；
普通滤镜参数为 lightness×1.8、saturation×0.2，较轻滤镜为 lightness×1.2、saturation×0.3。

相关渲染层 foot-gun：[feedback_dual_queue_order_preservation](feedback_dual_queue_order_preservation.md)、[project_pvz_gpu_instancing_reanim](project_pvz_gpu_instancing_reanim.md)（其发光走 additive 路径，受此契约约束）。
