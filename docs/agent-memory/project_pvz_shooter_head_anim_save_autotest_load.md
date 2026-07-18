---
name: project-pvz-shooter-head-anim-save-autotest-load
description: Shooter 子 Animator 完整存读档，以及 AutoTest 显式只读加载真实关卡存档
metadata:
  node_type: memory
  type: project
---

# Shooter 头部动画存档与 AutoTest 只读读档

2026-07-18 修复 `level1001_data.json` 轮间选卡读档后 row=0 双发射手无限吐豆。

## 根因与修复

- `Shooter::mHeadAnim` 是附加在主 Animator 上的独立子 Animator，不经过 `GameInfoSaver::SaveAnimState/RestoreAnimState`。旧实现只保存 `headAnimTrack/headAnimFrame`，读取一律 `PlayTrack`，把进行中的 `PlayTrackOnce("anim_shooting")` 强制改成 `PLAY_REPEAT`。
- 选卡阶段 `Plant::Update` 有意继续推进 Animator、但冻结 `PlantUpdate`；第 64 帧持久事件因此每轮触发一次 `ShootBullet`。不要通过在选卡阶段恢复 `PlantUpdate` 来修，否则生产、计时等植物行为也会继续。
- Shooter 现保存头部轨道、帧、base/clip 速度、播放状态、目标轨道/回切速度以及 playing 标记，并按状态恢复。缺少新字段的旧存档若位于 `anim_shooting`，按一次性动画恢复并回到 `anim_head_idle`，绝不再默认循环。
- Repeater 另存 `pendingSecondShot/isSecondShot`，保证新存档能精确续完两发之间的瞬态；旧存档缺字段时默认尚未发出第一颗。

## AutoTest 特殊读档模式

- 新启动参数：`-AutoTestLoadSave`。仅在同时使用 `-AutoTest <script>` 时生效，只放行 `LoadLevelData` 从当前构建目录的 `./saves/` 读取真实关卡存档。
- `LoadPlayerInfo` 仍短路，避免玩家全屏、VSync 等设置污染测试环境；`SavePlayerInfo`、`SaveLevelData` 和 `DeleteLevelData` 始终短路。因此该模式严格只读，默认 AutoTest 的确定性隔离行为不变。
- `dump_state/assert_state` 新增 `plantCount`、`bulletCount`、`repeatingShootingHeadCount`，Shooter 植物条目新增 `headTrack/headAnimPlaying/headAnimPlayState`。

## 验证证据

- 脚本：`autotest/scripts/repro_shooter_save_resume.json`，运行时必须显式加 `-AutoTestLoadSave`，且本地 `build/clang-release/saves/level1001_data.json` 必须存在。
- RED（仅加入读档开关、未修 Shooter）：exit 1，`repeatingShootingHeadCount=3`，row=0 三株双发射手均为 `anim_shooting + PLAY_REPEAT`；截图可见持续豌豆。
- GREEN：exit 0；旧存档中尚未越过第 64 帧的三株双发射手各续发一颗，随后全部回 `anim_head_idle + PLAY_REPEAT`；等待 8 秒后 `bulletCount=0`、`repeatingShootingHeadCount=0`。
- 问题存档测试前后 SHA-256 均为 `D45EE2F17662DE4E5CC0A26D840EB568BD392B1CF0B1665E2714209125FF5B0E`。不带新参数的 `demo_peashooter.json` exit 0，全部 4 个真实存档哈希不变。

## 维护注意

- 新增其他附加子 Animator 时，不能假设主 Animator 的通用存档会覆盖它；任何一次性轨道都必须保存播放状态机本身，而不只是轨道/帧。
- AutoTest 声称“不写存档”时必须同时审查保存和删除入口；本次补上了过去遗漏的 `DeleteLevelData` AutoTest 守卫。
