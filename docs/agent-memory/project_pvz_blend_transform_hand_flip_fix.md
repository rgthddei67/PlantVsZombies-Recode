---
name: project_pvz_blend_transform_hand_flip_fix
description: 僵尸/植物启用混合动画(blend)时肢体(手)翻转的根因与修法——GetDeltaTransform blend 分支 >180° 吸附方向反了
metadata:
  node_type: memory
  type: project
  originSessionId: af92488b-048b-494e-853f-016d2840f896
---

2026-06-28 commit 1bf662a(未push)：修"僵尸启用 blend(混合动画)时右手翻转"。

**根因**：`Reanimation.cpp` 的 `GetDeltaTransform` 有**两套角度插值算法**——普通帧间(`useDestFrame=false`)走最短弧(`while diff±360` 绕回[-180,180])，blend(`useDestFrame=true`)复刻原版"某轴需转过 >180° 时不平滑插值、直接吸附"(避免转半圈/穿过退化扭曲态)。bug：blend 分支把吸附目标写成了 **tSrc(blend 起始旧帧)** 而非 **tDst(当前帧)**。对照 C# 权威 `Sexy.TodLib/Reanim/ReanimatorXnaHelpers.cs::BlendTransform`：它吸附到 `theTransform1`=当前帧(`num2=t1.SkewY`)，使该关节整段 blend 立即采用当前姿态；旧实现吸附到旧帧后 `tOutput≡旧值`→该关节整段 blend 卡在切轨道前朝向、blend 结束(计数器归零走回最短弧分支)才弹回。reanim 里**肢体翻转常编码成 ky=kx+180°**(渲染矩阵第二列由 ky 决定=Y轴镜像，见 Animator.cpp:372-377)；当只有 ky 跨过 >180° 阈值时 blend 期间 kx 正常过渡、ky 冻结→"右手翻转"。

**指纹/诊断**：①只在 `blendTime>0` 时出现(blendTime=0→`mReanimBlendCounter=-1`→永远走最短弧分支，对翻转无碍)；②"肢体只在开启 blend 时翻"→直接查 `GetDeltaTransform` blend 分支的吸附目标。blend 已默认用在僵尸 walk↔eat(Zombie.cpp:525/545,0.2s)、death(:276,0.3s)等转场。

**修法**：吸附目标 `tSrc`→`tDst`，kx/ky 对称，非吸附(差≤180°)路径不变。`GetInterpolatedTransform`(Animator.cpp:895)是唯一调用者，普通 + GPU instanced 两条 Draw 路径全覆盖，单点改。

**验证手法(可复用)**：autotest/scripts/repro_blend_hand.json——把 NORMAL 僵尸生成在 wall-nut 格上(x≈col6中心,触发 walk→eat blend)，`set_timescale 0.2` 把 0.2s blend 拉成~60 渲染帧(deltaTime=unscaled*timeScale,DeltaTime.h:50;wait_frames 按渲染帧不受 timescale 影响)，转场窗口连拍。再 git stash 修复→重建旧版→**同 seed 同脚本对新旧二进制逐像素 diff**(System.Drawing)→差异仅落在 blend 中段帧(blendRatio≈0.5)的手部 bbox、其余逐位相同=因果钉死。白天关卡僵尸小、该转场跨阈值的是小关节→diff 仅~17px(主人夜晚特写更夸张但同机制)。

相关：[project_pvz_animator_clip_speed](project_pvz_animator_clip_speed.md) [project_pvz_gpu_instancing_reanim](project_pvz_gpu_instancing_reanim.md) [project_pvz_animator_playstate_save_fix](project_pvz_animator_playstate_save_fix.md)

## 2026-07-19：blend 离散帧属性契约收口

`GetDeltaTransform` 的连续变换本来就有两套策略，离散的 `f/image` 也必须与之一致：blend (`useDestFrame=true`) 取 `tDst`，普通帧间插值取 `tSrc`。旧实现的 `f` 已区分，但 `image` 无条件取 `tSrc`；`Animator::GetInterpolatedTransform` 后面又用 `frameBefore.image` 补写，恰好遮住当前唯一调用点的 blend 错误，但 helper 契约本身是错的，且有一次冗余指针写入。

终态：把 `f/image` 合并到角度策略已必经的 `if (useDestFrame)` 两个分支中，blend 取 `tDst`、普通取 `tSrc`；删除 `GetInterpolatedTransform` 的补写。不采用“统一取 after”：普通路径若为迎合 after 而反转参数及 `1-fraction`，会扩大角度最短弧的审计面，而且这里本来就必须按 blend 分支；合并离散属性不新增分支，还删掉了后置补写。

验证：`clang-release` 全量配置/构建通过；临时契约程序直接链接实际 `Reanimation.cpp.obj`，断言普通=`src.f/src.image`、blend=`dst.f/dst.image`，exit 0；当前桌面可见运行 `repro_blend_hand.json -Seed 42`，exit 0，`run.log` finished OK，状态为 `ZOMBIE_NORMAL/anim_eat/animPlaying=true`，逐段抽查 t00/t05/t10/t15/settled 截图无旧贴图残留、部件翻转或稳定态异常。
