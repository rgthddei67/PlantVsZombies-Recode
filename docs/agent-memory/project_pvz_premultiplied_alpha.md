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
**How to apply**：(1) 加新混合模式时 srcColor 记得用 ONE 系语义；(2) 若出现旧式 `srcColorBlendFactor = SRC_ALPHA` 或着色器 `t*vColor`，是漏改的预乘断点；(3) shader 改完要 glslc 重编 `.spv` 并拷到 `x64/Debug` 和 `x64/Release` 的 `Shader/spv/`（无自动编译脚本，glslc 在 `D:\VulkanSDK\Bin`）；(4) 活动配置是 Release（Debug exe 长期 stale）。

相关渲染层 foot-gun：[feedback_dual_queue_order_preservation](feedback_dual_queue_order_preservation.md)、[project_pvz_gpu_instancing_reanim](project_pvz_gpu_instancing_reanim.md)（其发光走 additive 路径，受此契约约束）。
