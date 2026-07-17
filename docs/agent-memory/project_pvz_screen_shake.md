---
name: project_pvz_screen_shake
description: "屏幕抖动已落地(f3e87b4+修复f09abb3,未push)；全屏视觉效果必须走相机projView而非变换栈(instancing快路径不消费栈)；SAD截图验证法+其教训；附带发现smoke_doomshroom在master基线就失败(非抖动回归)"
metadata:
  node_type: memory
  type: project
  originSessionId: a5e84e26-1265-4ff9-af1e-8d17dc4e91dc
---

2026-07-16 (f3e87b4 功能 + f09abb3 修复, 未push) 屏幕抖动移植原版 Board::ShakeBoard，樱桃+毁灭菇爆炸接入。

**⚠️ 核心 foot-gun（以后任何全屏视觉效果直接引用）：变换栈 ≠ 全覆盖。**
- 初版在 GameScene::Draw 外围 PushTransform 平移——看似 worker 会经 BeginParallelRecord 继承主线程栈顶，实则 **GPU instancing 快路径完全不消费变换栈**：`Animator::DrawInternalInstanced` 直写世界坐标进 InstanceRecord（`rec.tx = baseX + tx*Scale`，Animator.cpp:394），`AppendReanimInstance` 原样入切片，shader 只吃 projView push constant。"每次 DrawXxx 已把 finalMatrix 烘进 record"的注释只对 DrawTextureMatrix 慢路径成立，而 reanim 快路径命中率 ~100%（GATE A）。血量字形快路径（RecordDrawGlyphRun 直写 InstanceRecord）同理。结果=背景/UI 抖、全体植物僵尸割草机纹丝不动（主人肉眼抓出）。
- **正解 = 相机**：`GameScene::Draw` 里 `SetCameraPosition(-shake)`。projView = m_projection * m_viewMatrix 是 batch/instance/字形/延迟文字四条管线的公共出口（FlushBatch/FlushInstances/replay 三处构造），一次覆盖；IsWorldPointVisible 剔除与 LogicalToWorld 鼠标反变换用同一 projView 自动一致。引擎相机接口本就存在（SetCameraPosition/Zoom/Rotation + UpdateViewMatrix）。
- **相机不能每帧无条件覆写**：开场动画背景平移也用 SetCameraPosition(camX,0)（GameScene.cpp Update 内两处）。抖动只在 GAME 状态发生（相机基线恒 0）→ 仅抖动中覆写 + mShakeCameraApplied 标记在结束后归零一次，平时不碰。
- 相机须在 Scene::Draw 之前设好——mid-frame blend 切换会触发 flush，本帧所有 flush 必须读同一 projView。

**机制（Board 侧，f3e87b4）：**
- 抖动状态在 Board：mShakeTimer 乘 dt 递减（暂停冻结/倍速同步，同弹坑口径），纯视觉不入存档。
- 曲线双模：osc=1 = 原版 TodCurves.Bounce 三角波 `1-|1-2t|`（0.12s=原版 12刻@100Hz）；osc>1 = 衰减正弦 `sin(nπt)·(1-t)`。**"更剧烈"≠放大单跳，要靠多次变号振荡**。符号约定 offset=(-amountX·w, +amountY·w)。樱桃 (3,-4) 0.12s 忠原版；毁灭菇 (6,-9) 0.5s osc=5。原版樱桃/毁灭菇/土豆雷/辣椒全是 (3,-4)。
- dump_state 有 shake 块：shakeOn 整数投影供 assert_state，offsetX/Y 浮点勿 equals。

**验证方法论（SAD 截图位移检测，可复用）：**
- 爆炸帧定位：PS 生成密集 dump 探测脚本读 shakeOn 翻转。seed 42：樱桃=种下+1.0s+17~20帧（PlayTrack 速度随机但 seed 定即确定），毁灭菇=+1.2s+13~16帧（51帧@23fps）。
- Add-Type 内联 C# LockBits SAD 搜 ±10px 整数平移。**教训：必须分管线选区域对测**——初版验证区域全是 batch 内容（房屋背景/UI 按钮），假绿放过了 reanim 不动；修复后用"割草机柱"（纯 reanim）与背景区对测三方吻合才算数。非零残差（21/110/129）本身就是"部分内容没动"的指纹，别忽略。毁灭菇爆炸后 ~15+ 帧全屏紫闪把画面冲成近纯色，闪内帧不可光学测（连 UI 区都会给错值），选闪后帧或靠 dump 断言。区域坐标从图像边缘内缩 range px 防越界。

**附带发现（待查，非抖动回归）：** smoke_doomshroom.json 在 master 基线（stash 掉抖动改动重编译）就 FAIL at cmd#32：click 序列选中小喷菇卡后弹坑旁格 (682,338) 点击 plants=[]（小喷菇 0 阳光所以 sun 无线索）。两 build 逐帧一致=确定性。db73b07 时声称七阶段全过——疑 3918192 的 UpdatePlantPreviewPosition 弹坑改动后没复跑，或验收跑的别的 preset/seed。见 [project_pvz_doomshroom_crater](project_pvz_doomshroom_crater.md)。
