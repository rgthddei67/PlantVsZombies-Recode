# 第八大关听雪草

## 当前合同（2026-08-28）

- `PLANT_LISTENINGGRASS` 是 8-1 通关奖励，75 阳光、20 游戏秒卡牌冷却、300 生命；内部侦听初始就绪，两个成功响应共享 6 游戏秒冷却，每次最多处理一个目标。
- 就绪时先枚举本行活动、未垂死、未魅惑僵尸，按世界 X 从小到大（最靠近房屋）、稳定 ID 从小到大排序，再逐个调用目标拥有的 `ForceSurfaceFromGroundHazard()`；首个接受者立即出雪并取消自然冲击，听雪草不附加伤害。
- 没有目标接受时，调用 Board 权威 `SealSnowHole(row)` 封闭本行形成中或活动雪穴。已经提交的延迟雪穴出生事务不回滚；事务提交时继续由既有 Board 合同检查入口，失效则回退右侧出生。
- 雪盲三格自动索敌限制和极夜竖风不参与这次行扫描；合法魅惑目标被排除。再次潜雪的 0.8 秒前摇不是地下状态，因此听雪草不把它当作可迫出目标，也不承担中断语义。

## 动画、资源与存档

- 运行时完整复用经典 `Umbrellaleaf.reanim` 的 `anim_idle`/`anim_block` 时间轴和轨道名，只把本体与七片叶子确定性映射为薄荷面部、冷青叶片和浅霜边。`scripts/generate_listening_grass_assets.ps1` 锁定全部经典输入 SHA-256；不使用 AI 合成整株，不新增动画帧事件。
- 玩法在函数调用边沿已经提交，`anim_block`、`SOUND_BLEEP`、`ListeningGrassPulse`/`ListeningGrassHoleSeal` 只承担反馈。内部冷却由 `SaveExtraData` 保存，读档钳位到 0～6 秒且不重播反馈或重复扫描。
- 资源闭环为 `ListeningGrass.png`、八个实际引用分件（body + leaf1..7）与 `ListeningGrass.reanim`；经典空 blink 轨没有引用贴图，不为测试改写原时间轴。

## 验证入口

- 默认 Vulkan 可见 `clang-release` 的 `smoke_listening_grass.json` 覆盖资源键、75/20/300 数值、无伤迫出、最近房屋优先、共享冷却、快照往返、活动/形成雪穴、魅惑排除、雪盲竖风和 8-1 奖励；四张专项截图目验原版画风及两类粒子。
- 父级 `smoke_snow_burrow_zombie.json` 与 `smoke_polar_night_core.json` 同步通过。没有修改渲染后端，因此不扩到 `-NoInstance` 或 OpenGL。
