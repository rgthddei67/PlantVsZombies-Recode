---
name: adapting-classic-reanimation
description: Use when adapting an existing PvZ reanim timeline to a new plant or zombie by replacing part art, tuning pivots or clip ranges, or diagnosing detached parts, hard sprite swaps, and loop seams. Do not use for ParticleSystem XML or a purely static card image.
---

# 经典 Reanimation 适配

目标是保留原版时间轴的连续运动，同时让新素材的语义连接点、资源注册和存档表现都可靠。不要把“有多张图”误当成骨骼动画：整株静态图硬切或交叉淡化仍会显得像幻灯片。

## 先确定可复用的运动

1. 同时阅读当前 `resources/reanim/*.reanim`、对应 C# 参考实现和本项目最近的同类实体。列出 `anim_*` 片段范围、FPS、循环首尾、原版换图帧以及实际部件轨道。
2. 只在动作语义相近时复用时间轴。新植物可复用三叶草的弯折与回弹，但不能仅因尺寸接近就套用无关骨架。
3. 动画帧事件属于玩法提交点；新增前必须先询问主人。若玩法本来应即时结算，保留逻辑即时，只让 reanim 负责演出。

## 按语义拆分和挂接部件

- 保留连接身体的原版茎、关节或肢体轨。固定地面轨只承载真正固定的叶座、脚底或设备底座；把整段茎烘进固定底图会在上身摆动时产生断裂。
- 附件选择稳定的语义父轨。必须与父件同缩放、旋转和位移时使用 follower 或子 Animator；不要挂到摆幅不相称的手、头或地面轨。
- 当前 Animator 的 `SetTrackOffset` 是在轨道 transform 之后追加的未旋转平移，透明画布内的大额 padding 会在轨道缩放时放大轴心漂移。优先使用紧边界分件，或让附件偏移走父轨仿射变换；需要修改这个约定时先读 `Animator::Draw/DrawBuffered` 的两条路径。
- 同一轨道只有一个 follower 槽；已有挂件时使用附加子 Animator，不要覆盖，也不要退回对象外随意直绘。
- 换姿态贴图要对齐“手握点、颈根、茎秆插口、轮轴”等语义连接点，而不是对齐 Alpha 包围盒。倾斜或伸长的变体通常需要独立画布内偏移。
- reanim 已经为原版换图编写的 X/Y/缩放补偿仍然生效。替换素材时先观察轨道关键帧，避免贴图内部再做一次相同补偿而产生二次跳动。

## 资源制作闭环

- ImageGen 只生成高分辨率、非写实的 PvZ 手绘卡通母图或分件图集；最终运行图必须重新检查描边、透明边缘、轮廓和低分辨率可读性。
- 用仓库脚本确定性地裁切、去背景、缩放和输出分件。脚本锁定源图、参考 reanim 与最终输出 SHA-256，生成结果漂移时显式失败。
- 闭合“文件/清单 → `resources.xml` 或 loader → 实际资源键 → `HasReanimation`/`GetTexture(key,false)` 断言”。manifest 中出现文件不代表运行时换图键可用。
- 卡槽图是独立构图，不要用修改场上骨架尺寸的办法缩卡图。

## 时序、循环和存档

- 记录片段的全局帧范围与播放倍率，检查片段内部、换图帧、返回片段和循环末帧到首帧四类接缝。
- `PlayTrackOnce` 的 return track、blend、speed 与待返回状态若跨存档，必须由现有 Animator/GameInfoSaver 合同完整恢复；实体额外阶段和倒计时另入 `SaveExtraData/LoadExtraData`，已提交的一次性效果不得读档重触发。
- 不要用 `Pause()/Play()` 模拟环境暂停；遵循项目现有“本帧不推进”的 Animator 约定。

## 可见验收

1. 默认使用 `clang-release`，从 `build/clang-release/` 在主人当前桌面可见运行。
2. 专项 AutoTest 先断言 reanim 与每张运行时换图纹理已加载，再用 `set_timescale` 慢放；在换图前、换图帧、连续缩放帧、循环接缝和最终淡出分别截图。
3. 检查最终实机合成中的连接、握持、遮挡和轮廓，不以高分辨率母图或单张分件为验收。
4. 默认实例路径与 `-NoInstance` 各跑相关用例；涉及层级或 follower 时，两条路径都要截图。运动对象不要断言不稳定的绝对 X/Y，优先用同状态相对投影和同步截图。
