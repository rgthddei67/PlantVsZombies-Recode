---
name: project_pvz_coffeebean
description: 经典咖啡豆、短时 flying overlay 层、蘑菇睡眠 Z 指示、唤醒状态与毁灭菇激活的当前契约
metadata:
  node_type: memory
  type: project
---

# 经典咖啡豆与蘑菇唤醒

## 2026-08-11 当前实现

- `PLANT_INSTANT_COFFEE` 为 75 阳光、7.5 秒卡冷却、300 基础生命画像且 `persistent:false`；冒险 5-3 奖励沿用既有显式奖励表。运行资源使用 `Coffeebean.reanim`、独立卡图与 `sounds/Plant/coffee.ogg`，并通过 reanim/card/sound 加载断言闭合注册链。
- 咖啡豆占 `Cell::overlay` 短时 flying 层，只能叠在仍沉睡、尚未开始唤醒且未被蹦极抓取的普通层蘑菇上。overlay 画在普通层上方，但故意不参与顶层啃食或铲除；本体无阴影、无 collider、不可啃食并忽略地面植物伤害。
- 种下后等待 1 游戏秒，再重新查询同格普通层，调用目标唯一 `BeginWakeUp(1.0f)`，以 `22/12` clip speed 播放一次 `anim_crumble` 并请求 `SOUND_COFFEE`；碎裂轨结束后咖啡豆自行销毁。不新增动画帧事件。
- 目标仍保持 sleeping 直到 1 秒倒计时归零；剩余 0.6 秒时请求 `SOUND_WAKEUP`，最后 0.7 秒按原版 `EaseSinWave` 围绕视觉 Y+80 枢轴做 `1.0→0.8` 纵向弹性。普通 `Shroom::OnWakeUp` 回到 `anim_idle`；`DoomShroom` 覆写后进入与夜种相同的 reverse explosion + `anim_explode` 充能入口。
- 白天沉睡植物由 `Plant` 基类显示独立 `Z.reanim`。C# 的功能语义保持为：睡着时以 6～8fps、随机起相循环，醒来即移除；实现改用当前 `GetVisualAnchorPosition()`、搬运视觉偏移和按品种映射后的局部偏移，不照抄 800×600 左上角世界坐标。指示器是 sleeping 的派生显示状态，压扁时移除、失活后随宿主销毁，读档只静默重建，不新增存档字段或动画帧事件。
- 咖啡豆保存 phase/wait timer，植物基类保存 sleeping/wake timer。反序列化先用无副作用恢复入口重建权威状态，不重播咖啡、唤醒或充能音效，再由通用 Animator 状态恢复当前轨。旧档缺 wake timer 时默认 0。
- 台风把 `under/normal/pumpkin/overlay` 的全部活跃层作为同格组合搬运、出界或投入弹坑；新增层时不能只扩创建与清理而漏掉阵风生命周期。

## 验证证据

- `clang-release` 增量构建与链接退出 0。
- 2026-08-11 `clang-release` 配置、编译和链接退出 0；当前桌面可见 `smoke_coffeebean.json` 默认与 `-NoInstance` 各 113 条命令退出 0，`run.log` 均以 `script finished OK` 结束。新增断言闭合 `Z` reanim/贴图资源、睡眠显示、相对锚点、等待快照保留、醒后移除和台风搬运；默认与 `-NoInstance` 的睡眠/醒后截图均已目验，睡眠时白色 Z 清楚可见。
- 当前桌面可见既有 `smoke_doomshroom.json` 与 `smoke_doomshroom_same_cell_plants.json` 均退出 0，确认夜间充能、白天睡眠、弹坑阻种及同格承载层清除未回归。

关联 [project_pvz_doomshroom_crater](project_pvz_doomshroom_crater.md)、[project_pvz_night_rain_weather](project_pvz_night_rain_weather.md)、[project_pvz_render_coordinate_evidence](project_pvz_render_coordinate_evidence.md)。
