# 黑夜随机雨势天气

## 当前实现（2026-07-20）

黑夜关用间歇式小/中/大雨替代墓碑机制；白天背景不启用。战斗开始后 35～50 秒出现首场雨，新雨首段持续 30～45 秒，切档尾雨持续 12～22 秒，完全放晴后间隔 35～55 秒，初始三档权重 50%/35%/15%。初始小雨有一次 30% 升中雨、15% 升大雨的机会；切档后只允许“大→中/小”“中→小”衰减，尾端小雨必定放晴，故不会无限循环。天气强度、阶段剩余时间、大雨闪电计时和小雨增强资格随关卡存档；瞬态粒子与循环音不存档，`StartGame()` 按剩余天气重建。

倍率口径：僵尸为 1.05/1.10/1.15，统一进入 `Zombie::UpdateAnimSpeed()` 的 extra 层，组合式为 `mExtraSpeed × slowFactor × rain`，冻结优先置 0，因 `_ground` 位移读有效动画速度而不会滑步。植物为 1.03/1.06/1.09，只通过 `Plant` 辅助方法加速攻击、生产、成长和恢复；不改全局 delta，也不加速索敌、受伤、死亡、爆炸清理或卡片冷却。攻击速度与生存词条相乘，动画 clip 和计时阈值一起缩放。

视觉：`RainLight/Medium/Heavy.xml` 位于两个 `build/<preset>/resources/particles/config/`，复用 WhitePixel，使用细、低透明、冷蓝、逐滴亮度不同的斜雨丝。实玩反馈后小雨生成率由 28 提至 40、初始粒子 22→34、alpha 峰值 .12→.16，并提高少量尺寸/亮度；三档发射配额与 XML 默认时长扩到 60 秒，修复新雨段超过旧 26 秒后粒子停发而只剩声音。`ParticleRotation` 是本次新增通用 XML 标签；显式使用时 `ParticleEmitter::Draw()` 走“世界中心旋转后再拉伸”的矩阵路径，避免旧 `DrawTextureRegion` 先非等比缩放再旋转导致细长贴图角度被压回竖直。暗幕只覆盖世界层，不压 UI，alpha 小/中/大为 20/40/56；仅大雨有短促低峰值白闪，无动态灯光/反射。

声音：对照 C# 原版 `Challenge.InitLevel()` 的 `PlayFoley(FoleyType.Rain)`、`TodFoley` 中 `SOUND_RAIN` 的 flags=5（Loop + MuteOnPause）以及 `Board.DisposeBoard()` 的 `StopFoley(Rain)`。复用原版 `rain.ogg`，本实现通过 `AudioSystem::PlayLoopingSound/StopLoopingSound` 按资源键唯一追踪循环声道；每场雨开始/结束及 Board 销毁时同步启停，音量小/中/大为 0.18/0.28/0.45，仍乘主音量与音效音量。

## 资源与验证

- 雨 XML 与 `rain.ogg` 都在忽略的 build 资源树，提交时必须 `git add -f` 两个 preset；`resources.xml` 两份需保持一致。
- `adding-particle` skill 已补 `ParticleRotation` 标签语义；再改 loader/emitter 消费端要同步技能。
- `smoke_night_rain.json` 用 `set_weather`/`advance_weather_phase`/`trigger_lightning` 固定状态，断言三档倍率、暗幕、雨声启停、词条组合、减速、冻结优先级、大雨闪电及“小→大→中→小→晴”完整链，并产出七张截图；另用 0.2 秒真实天气倒计时验证尾端小雨自动复位倍率并停止声音，用第 28 秒截图验证长雨段仍持续发射粒子。
- 2026-07-20 `clang-release` 零警告；可见 `smoke_night_rain` 76 命令通过；既有白天 `smoke_perks_attackspeed` 通过且天气倍率保持 1.0。

详细设计见 `docs/superpowers/specs/2026-07-20-night-rain-weather-design.md`。
