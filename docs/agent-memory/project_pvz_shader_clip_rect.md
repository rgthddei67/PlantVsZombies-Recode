---
name: project_pvz_shader_clip_rect
description: 2026-07-23 通用 PushClipRect 改为逐顶点/逐实例 shader 矩形裁剪；不再 flush、切 draw 或动态改 scissor
metadata:
  node_type: memory
  type: project
---

# 通用 shader ClipRect

2026-07-23 将 `Graphics::PushClipRect/PopClipRect` 从动态 `vkCmdSetScissor` 改为随绘制数据提交的 fragment shader 裁剪。水路 `PushClipBottom/PopClipBottom` 只是全宽矩形的便捷别名，因此水中僵尸、伴舞出土、僵尸图鉴格窗和大喷粒子阻断共用同一套实现。

## 当前契约

- `ClipRect` 仍是左上原点的逻辑屏幕坐标，不受 Transform 或摄像机影响。push 时经 letterbox 换算成 framebuffer 像素，fragment shader 用 `gl_FragCoord` 比较，保持旧 scissor 的坐标语义。
- `PackedClipRect` 用两个 `uint32_t` 打包 `left/top` 与右/下开区间边界，各坐标占 16 bit。`BatchVertex` 为 52 B，`InstanceRecord` 为 56 B；无裁剪哨兵是 `min=0/max=0xFFFFFFFF`，fragment shader 先走一致性很高的哨兵快路径，只有活动 Clip 才解包四边并判断。
- 通用 Clip 不调用 `FlushBatch/FlushInstances`，不生成 `vkCmdSetScissor`，也不进入 worker `RecordCmd`；worker 本地维护逻辑/打包双栈，直接把当时的栈顶写入顶点或实例。`GetLastFrameScissorChangeCount()` 保留为回归探针，正常应恒为 0。
- worker 延迟到主线程的当前层 `DrawText/DrawGlyphRun` 会保存录制点的已相交逻辑 Clip，回放时临时压栈后发射文字；`DrawTextOnTop` 沿用旧行为，作为绝对顶层不继承对象内 Clip。
- 动态 scissor 现在只保留帧首全 framebuffer 默认值；letterbox viewport 本身负责限制黑边区域。物理 framebuffer 坐标上限为 65535，远高于本项目实际窗口/桌面尺寸。

## 性能与验证

水路改造前，2 只水中僵尸的逐对象矩形 scissor 为 `21 draw + 4 scissor`；改为 shader Clip 后为 `19 draw + 0 scissor`。通用化后，伴舞出土、图鉴每格僵尸和被铁门阻断的大喷粒子也不再因每对象 Push/Pop 线性制造 draw 分段。

`clang-playtest` 全量构建 0 warning，4 个 shader 均由 `glslc` 成功重编。当前桌面可见验证：`smoke_pool_zombie_visuals` 默认实例与 `-NoInstance` 均 exit 0、截图水线正确且 scissor=0；`smoke_backupdancer` exit 0、出土中段地面线正确；`smoke_dancer_almanac` exit 0、所有格窗无越界；`smoke_icefumeshroom` exit 0、铁门阻断与攻击链通过。
