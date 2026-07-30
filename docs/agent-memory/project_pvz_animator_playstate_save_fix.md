---
name: project-pvz-animator-playstate-save-fix
description: PlayTrackOnce 播放状态、目标轨道、返回速度与返回混合时间的统一存读档契约
metadata:
  node_type: memory
  type: project
  originSessionId: 3f6f0e3b-ed16-491d-ad62-561d8a63f83e
---

2026-06-23 commit `8aee406` (master，未 push)：修复「正在 PlayTrackOnce 的单位存档退出后读档会把那条一次性轨道当 PLAY_REPEAT 永远循环、再也切不回目标轨道」的 BUG。

**根因**：存档只持久化 `animTrack`(当前轨道)+`animFrame`(当前帧)，读档统一 `PlayTrack(track)`，而 `PlayTrack` 会把 `mPlayingState` 强写成 `PLAY_REPEAT`。即「存了状态机的输出、没存状态机本身」。`mPlayingState`/`mTargetTrack`/`mTargetTrackSpeed` 全丢。

**修复**（3 处）：
- `Animator` 暴露 `GetPlayingState()`/`GetTargetTrack()`/`GetTargetTrackSpeed()`；`AnimatedObject` 加同名空安全透传。
- `GameInfoSaver.cpp` 匿名命名空间加 `SaveAnimState`/`RestoreAnimState`，统一 4 存 + 4 读站点(plant/mower/zombie/sun)：存 播放状态+目标轨道+回切速度+base/clip 速度；读档按状态用 `PlayTrackOnce` 重建一次性播放，否则 `PlayTrack`。
- 旧存档无新字段(`animPlayState`/`animTargetTrack`/`animTargetTrackSpeed`)→默认 `PLAY_REPEAT`、`animSpeed` 缺省时不动 base→逐位向后兼容。

**架构信号**：修复前已有 **3 个类各自在 LoadExtraData 手写同款补丁**(`Chomper`/`PaperZombie`/`PotatoMine`，注释都点名这个 BUG)——典型「缺了通用层 → N 处重复绕过」。这些补丁保留：它们在 `RestoreAnimState` **之后**运行仍可覆盖，且还负责本层无法序列化的东西(帧事件回调重注册、PaperZombie 有意跳过 gasp)。

**对未来的影响(关键)**：**新增用 `PlayTrackOnce` 的植物/僵尸，读档不再需要自己在 LoadExtraData 里手动重建动画防死循环**——通用层已兜。只有需要恢复**帧事件回调**或想**有意改写**动画时才需自定义 LoadExtraData。被本次自动修好的(原先无补丁)：Polevaulter(anim_jump→anim_walk)、PuffShroom(anim_shooting→anim_idle)、SunShroom(anim_grow→anim_bigidle)。

验证：仅 clang-release 编译通过(0 warning,exit 0)；主人指示「不用 AutoTest」，未跑运行期存读档闭环。三层速度模型见 [project_pvz_animator_clip_speed](project_pvz_animator_clip_speed.md)；存读档异常边界见 [project_pvz_perk_system](project_pvz_perk_system.md)。

## 2026-07-30：返回混合时间成为显式状态

三叶草暴露出原 API 的缺口：调用方传入的 `blendTime=0` 只控制进入一次性轨，串行和并行
返回路径仍把目标轨混合硬编码成 0.5 秒，导致相邻包装轨在返回阶段插值出资源中不存在的姿态。

- `PlayTrackOnce` 新增第六参数 `returnTrackBlendTime`，顺序与 IDE 提示保持为
  `speed/blendTime/returnSpeed/returnTrackBlendTime`；默认 0.5 秒，旧调用行为不变。
- Animator 保存 `mTargetTrackBlendTime`，串行和并行返回路径都消费它；转轨后复位为历史默认。
- `GameInfoSaver`、Shooter 头部和 ThreePeater 额外头部都保存目标轨混合时间。旧档缺字段时
  回退 0.5 秒，无需提升 schema。
- 仍不持久化已经开始的 blend 剩余进度；那是既有视觉瞬态限制，与“尚未返回时必须保存未来
  返回参数”是两件事。
