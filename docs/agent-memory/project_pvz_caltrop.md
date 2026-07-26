---
name: project-pvz-caltrop
description: "2026-07-26 经典地刺的25帧范围攻击、免啃食、冰车爆胎事件与精英冰车覆写入口"
metadata:
  node_type: memory
  type: project
---

# 经典地刺（Caltrop / Spikeweed）

## 行为契约

- 现有枚举仍使用 `PLANT_SPIKEWEED`，具体类为 `Caltrop`；卡图键 `IMAGE_CALTROP`，
  动画资源名 `Caltrop`。100 阳光、7.5 秒冷却、300 生命，不带普通植物阴影。
- `CanBeEaten()` 固定返回 false，普通僵尸不能把地刺选为啃食目标；车辆等特殊交互走各自入口。
- 同排僵尸碰撞框与地刺逻辑中心 `[-20,+10]px` 的 30px 窄攻击带重叠时起播
  `anim_attack`。动画按 C# 的 18fps 播放，主人指定的全局第 25 帧对攻击带内全部
  非魅惑、非垂死目标结算 20 伤害；已经断头但仍活着且碰撞有效的僵尸仍会受伤。
  约 0.99 秒攻击周期和动画均响应植物行动速度倍率。
- 地刺系不能种在水路，即使格上已有睡莲也不行。最后一类尚未实装的地形当前按普通地面；
  `Board.cpp::IsSpikeweedTerrainRestricted` 保留集中入口，以后只在这里启用新地形规则。
- 攻击冷却写入植物额外存档。

## 冰车联动

- 地刺命中 `ZamboniZombie` 时不走普通 20 伤害，调用车辆拥有的虚函数
  `HandleCaltropHit(Caltrop&)`。后续精英冰车覆写该入口即可改变“被扎后直接进入死亡”
  的默认语义，不需要修改地刺类。
- 普通冰车默认让地刺消失、车速归零、碰撞关闭、切到二段破损和扁胎贴图，
  以 1/4 条件概率选择 `anim_wheelie2`，否则播放 `anim_wheelie1`；2.8 秒后走现有爆炸退出。
- 原版 `FoleyType.TirePop` 实际映射 `balloon_pop.ogg`。爆胎事件只请求一次
  `SOUND_BALLOON_POP`，并在车辆稳定视觉原点 `(+29,+114)` 生成 `ZamboniTire`：
  10 枚泥土碎屑和 6～10 团短烟。原版 `DirtSmall` 是 8×2 网格，本项目按
  `Texture Column="8" Row="2"` 拆分后随机选取，不能直接当横排 `ImageFrames`。
- 受扎标记和剩余死亡时间进入僵尸额外存档；读档恢复停止移动、禁用碰撞与扁胎终态，
  不重新抽取特殊动画。

## 验证证据

`clang-playtest` 零警告构建。`smoke_caltrop.json` 在主人当前桌面可见运行，窗口标题
“植物大战僵尸中文版”、Seed 42、退出码 0，`run.log` 以 `script finished OK` 结束：

- 普通僵尸保持 `isEating=false`，地刺健康 300；第 25 帧后单次扣 20，下一周期再扣 20。
- 冰车命中后地刺同帧消失，冰车 `bodyHealth=0`、车速 0、轨道 `anim_wheelie1`，
  剩余计时样本 2683ms，爆胎声音请求恰好 1 次，轮胎粒子活跃；2.8 秒后爆炸退出。
- 可见截图确认地刺贴地且攻击帧命中闪白；扁胎车轮、特殊动画和泥土碎屑落在车轮附近，
  延时结束后蓝色爆炸云覆盖车辆位置。
- 泳池陆格允许种植，水格有无睡莲均拒绝；level 37 当前 `ROOF` 占位背景按普通地面放行。

同轮可见 `smoke_zamboni.json` 退出码 0，原有移动碾压、冰道、两段破损烟雾、普通爆炸和
灰烬第 53 帧回收全部回归通过。
