---
name: reference-pvz-color-0-255-convention
description: PvZ 绘制接口的 glm::vec4 颜色是 0..255 范围而非归一化 0..1（DrawText/DrawTexture tint 等）
metadata:
  node_type: memory
  type: reference
  originSessionId: d82d330e-766a-4111-b72d-0887c31152b1
---

PvZ 仓库里所有绘制接口（`Graphics::DrawText` / `DrawTexture` tint / `FillRect` 等）的
`glm::vec4 color` 参数语义是 **0..255 范围**，不是归一化的 0..1——尽管类型是浮点 vec4。

证据：`Graphics.h` 的 `ToSDLColor()` 直接 `static_cast<Uint8>(color.r)`（不乘 255）；
Animator 自绘用 `baseColor(255,255,255, alpha*255)`；GameScene UI 文字用 `{223,186,98,255}`。

**陷阱**：写成 0..1（如 `vec4(0,1,0,1)` 想表示纯绿不透明）会被 cast 成 `SDL_Color{0,1,0,1}`，
rgb≈黑、**alpha=1/255≈全透明 → 文字/图形隐身**。这正是 2026-05 加植物/僵尸血量显示时
"DrawText 执行了却一个可见像素都没有、与并行/串行/sprite 大小都无关" 的真根因。

正确写法：植物绿 `vec4(0,255,0,255)`，僵尸浅蓝 `vec4(150,200,255,255)`。

调试教训：遇到"代码执行了但没像素"，第一步应做"在相同坐标画个实心矩形"二分隔离
（区分 文字/颜色 vs 坐标/层级），别上来连改坐标和 z-order。相关：[reference-pvz-batch-instance-zorder](reference_pvz_batch_instance_zorder.md)
