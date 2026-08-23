---
name: project_pvz_winter_garden
description: 第七大关冬日花园背景、寒潮温度、冻融线、降雪表现和台风禁用契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-24
---

# 第七大关冬日花园、寒潮与冻融线

## 关卡与地图

冒险 55～63 显示为 7-1～7-9，统一使用 `Background::WINTER_GARDEN`、5 行 × 9 列平地和白天音乐。`AdventureProgression::LAST_ADVENTURE_LEVEL` 为 63，七大关当前九关均为 `NO_PLANT_REWARD`；`spawnlists.json` 已逐关登记既有僵尸，主人明确把特色僵尸留到后续实现。

正式背景资源为 `resources/image/background_wintergarden.png`，以原版白天草坪构图和当前 1100×600 网格为基准改造：保留房屋、五行草坪和 80×100 逻辑格对齐，在屋顶、边缘灌木、石板与草坪外围铺积雪，并让右侧棕色裸地也被不规则积雪覆盖。资源键为 `IMAGE_BACKGROUND_WINTERGARDEN`，需经 `resources.xml` 注册和 AutoTest 加载断言闭环。

## 寒潮温度与冻融线

温度是独立于 `RainIntensity` 的 Board-owned 环境维度，且只由寒潮状态机控制：`CALM` 保持 +6°C 35～60 秒，`COOLING` 用 12 秒降到 -12°C，`COLD` 保持 45～70 秒，`THAWING` 用 15 秒回到 +6°C。阶段、余时、温度和初始化标志进入关卡快照；旧档以 +6°C / `CALM` 中性恢复。

0°C 起从僵尸侧向房屋推进冻融线：0°C 冻最右 1 列，此后每降低 2°C 多冻 1 列，-12°C 达到 6 列上限，因此 1～3 列永久安全、4～9 列为最大冻土区。逻辑只按整列查询，视觉可以跨过边界少量出霜。冻土拒绝新的正式种植和直接正式创建；已经存在的植物保留，读档/初始化/预览路径不因恢复顺序被误删。

冻土表现使用 `IMAGE_WINTER_FROST_OVERLAY_V2`：一张透明、连续、无网格的雪斑/霜枝/冰晶纹理按当前冻结宽度绘制，左缘带 24px 不规则溢出；alpha 从 0°C 的 75 平滑增加到 -12°C 的 245。不要恢复逐格蓝色矩形或格线。该素材由内置 ImageGen 生成，正式提示词要求：透明背景、横向连续的有机霜雪覆盖、参差左缘、细霜枝/冰晶/薄雪斑、轻松手绘游戏质感、无草地/格线/蓝色面板/文字；游戏内仍须分别检查 1 列浅冻和 6 列重冻截图。

## 降雪与台风

低于或等于 0°C 时，同一 `RainIntensity` 只把降水表现派生为 `SnowLight/SnowMedium/SnowHeavy`；雨势阶段、持续时间、植物/僵尸倍率和导演权重保持不变。跨过 0°C 时先停止旧降水效果，再用当前雨势重建雨或雪，避免叠加。雪天显式关闭循环雨声、地面雨滴水花和普通大雨闪电/雷声；雪粒子必须覆盖整个 600px 战场高度，不能只聚在屏幕上沿。

冬日花园 `SupportsTyphoon()` 恒为 false。pending 预抽/消费、开始、恢复、逐帧更新、阵风、测试强制入口和概率投影都服从该门禁；不是仅把新台风概率设为 0。进入地图或恢复快照时不得留下活动或待生效台风。

## 资源与验证契约

权威资源只放在 `build/clang-release/resources/`：背景、透明冻土覆盖、三个 Snow 粒子 XML 和 `resources.xml` 注册。其他 preset 通过目录联接共享，不维护副本。AutoTest 用 `set_cold_wave` 固定阶段/温度，并导出 `weather.winter` 的温度十分位、阶段、余时、冻土列数/首列、是否下雪、实际降水特效名和台风资格。

专项 `smoke_winter_garden.json` 覆盖：7-1/五行九列/背景与纹理加载、+6°C 无冻土、0°C 一列浅冻截图、-12°C 六列重冻、既有植物保留、冻土拒绝新种且安全区可种、大雪效果与粒子、雨声/闪电/台风禁用、升温恢复同档大雨、寒潮快照往返和重载截图。最终交付需用 `clang-release` 在默认实例路径与 `-NoInstance` 两路径桌面可见运行并检查 exit code、`run.log`、状态 JSON、断言和截图。

2026-08-24 当前交付证据：`clang-release` 全量配置、LTO 构建退出 0，Win7 导入审计通过 378 项；`smoke_winter_garden.json` 在默认实例路径与 `-NoInstance` 当前桌面可见运行均执行到 command 53、`status=passed`、exit 0，两个 `run.log` 均为 `script finished OK` 且 0 ERROR/WARN。两路径的 0°C 浅冻和 -12°C 大雪截图已目验：浅冻仅最右列淡霜，重冻为无格线的参差雪斑/霜枝连续覆盖，既有植物仍可见。`adding-rain-weather` 与 `adding-particle` 的更新均通过 skill-creator `quick_validate.py`。
