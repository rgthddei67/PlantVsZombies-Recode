# 植物压扁与复合 Animator 世界缩放

## 当前实现（2026-07-26）

- `Plant::Squish()` 是通用压扁残影入口；冰车和投篮车直接使用，巨人在 `anim_smash` 第 93 帧改为逐层派发 `Plant::ResolveGargantuarSmash()`，其默认实现再调用 `Squish()`。特殊植物因此能立即正式结算或忽略锤击，且不影响同格其他层独立受击；清醒寒冰菇立即执行正式冻结，睡眠寒冰菇仍走默认压扁。
- 进入压扁态时冻结当前视觉坐标和动画，跳过植物行为与承伤，禁用碰撞、隐藏影子并释放上下层占格。残影销毁时只清仍指向自身的 ID，不会误删同格后来种下的植物。
- 表现对齐 C#：X 保持 `1.0`，Y 压到 `0.5`，以冻结视觉原点下方一个当前地图格高为底边锚点；随机播放现有 `SOUND_ZOMBIE_EAT` / `SOUND_ZOMBIE_EAT2`。
- 主人将总保留时间从约 8.33 秒调整为 5 秒，末段仍占 20%，即最后 1 秒线性渐隐。计时只在 `BoardState::GAME` 推进。
- 存档保存压扁标记、剩余秒数和冻结坐标；恢复顺序位于派生类额外状态之后，确保终态暂停与透明度不被覆盖。

## 复合 Animator 绘制契约

2026-07-23 起，默认 `Animator::DrawInternal` 会按父轨道原顺序递归实例化根与任意深度附件；`-NoInstance` 才让整棵附件树走矩阵慢路径。外层 `Graphics` 变换栈仍不被默认实例路径消费，因此不能用变换栈实现整株世界缩放。

通用入口仍是 `Animator::SetRenderScale`：同一世界锚点缩放同时烘入 `InstanceRecord` 与 `-NoInstance` 的 `glm::mat4`，递归同步全部现有附件；`AttachAnimator` 让以后动态附加的子级立即继承。不要在 `Shooter` 或各植物子类逐一特判。

## 验证证据

可见 `clang-playtest` AutoTest `smoke_plant_squish.json`（Seed 42）退出码 0：

- 根 Animator 与射手头部 `renderScaleYPct/headRenderScaleYPct` 均为 50，`PauseSubtree()` 令两者播放状态均为暂停。
- 压扁前后及 4.5 秒渐隐观察点，根帧固定在约 7.365、头部帧固定在约 32.365。
- 进入状态后原格可立即重新种植；剩余约 0.47 秒时 alpha 约 47%，5 秒后植物数量归零。
- `before_squish.png`、`squished.png`、`squished_fading.png` 已逐张检查，头、茎、叶整体以底边锚点同步压扁。

递归实例化收口后同脚本默认与 `-NoInstance` 均可见退出 0；默认截图逐张检查通过，静止/压扁图与慢路径最大通道差 1。`smoke_animator_recursive_instancing.json` 另行验证父身体与子头部同步 glow、恢复后轨道仍为 `anim_idle` / `anim_head_idle`。

2026-08-15 `clang-release` 可见 `smoke_gargantuar_special_plant_smash.json` 与 `smoke_gargantuar_actions.json` 均 exit 0：普通植物仍压扁，索敌倭瓜不压扁但花盆独立压扁，睡眠毁灭菇保留普通压扁；三类引爆植物不生成压扁残影。

同日后续补齐清醒寒冰菇的立即冻结分支，`clang-release` 构建和 Win7 导入审计通过；按主人要求未跑 AutoTest，因此没有寒冰菇锤击的新增运行截图或状态断言。
