# 植物压扁与复合 Animator 世界缩放

## 当前实现（2026-07-23）

- `Plant::Squish()` 是巨人砸击、冰车和投篮车以后共用的植物侧入口；本次尚未实现三类僵尸的命中调用。
- 进入压扁态时冻结当前视觉坐标和动画，跳过植物行为与承伤，禁用碰撞、隐藏影子并释放上下层占格。残影销毁时只清仍指向自身的 ID，不会误删同格后来种下的植物。
- 表现对齐 C#：X 保持 `1.0`，Y 压到 `0.5`，以冻结视觉原点下方一个当前地图格高为底边锚点；随机播放现有 `SOUND_ZOMBIE_EAT` / `SOUND_ZOMBIE_EAT2`。
- 主人将总保留时间从约 8.33 秒调整为 5 秒，末段仍占 20%，即最后 1 秒线性渐隐。计时只在 `BoardState::GAME` 推进。
- 存档保存压扁标记、剩余秒数和冻结坐标；恢复顺序位于派生类额外状态之后，确保终态暂停与透明度不被覆盖。

## 复合 Animator 陷阱

`Animator::DrawInternal` 遇到附件时，根 Animator 为保持父轨道变换和交错绘制顺序会走慢路径；递归进入无附件的子 Animator 后仍会走 GPU 实例化快路径。因此外层 `Graphics` 变换只会改变射手身体，不会改变头部。

通用修复是 `Animator::SetRenderScale`：同一世界锚点缩放同时烘入慢路径 `glm::mat4` 和快路径 `InstanceRecord`，递归同步全部现有附件；`AttachAnimator` 让以后动态附加的子级立即继承。不要在 `Shooter` 或各植物子类逐一特判。

## 验证证据

可见 `clang-playtest` AutoTest `smoke_plant_squish.json`（Seed 42）退出码 0：

- 根 Animator 与射手头部 `renderScaleYPct/headRenderScaleYPct` 均为 50，`PauseSubtree()` 令两者播放状态均为暂停。
- 压扁前后及 4.5 秒渐隐观察点，根帧固定在约 7.365、头部帧固定在约 32.365。
- 进入状态后原格可立即重新种植；剩余约 0.47 秒时 alpha 约 47%，5 秒后植物数量归零。
- `before_squish.png`、`squished.png`、`squished_fading.png` 已逐张检查，头、茎、叶整体以底边锚点同步压扁。
