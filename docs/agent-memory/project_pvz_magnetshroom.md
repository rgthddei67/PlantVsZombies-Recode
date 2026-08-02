---
name: project_pvz_magnetshroom
description: 2026-08-02 经典磁力菇的搜索、装备剥离、充能、离体物绘制、变体免疫、资源、存档与可见验证
metadata:
  node_type: memory
  type: project
---

# 经典磁力菇与僵尸装备剥离契约

## 当前实现

- `PLANT_MAGNETSHROOM` 为 100 阳光、7.5 秒卡片冷却、300 生命的夜间植物；白天沿用
  蘑菇 `anim_sleep`。READY 从上下各两行搜索，常规范围 270px、啃食目标 320px，
  以直线距离加每跨行 80px 选最近目标。
- 锁定当帧立即调用目标侧 `HasMagneticItem/ExtractMagneticItem` 原子剥离装备并播放
  `anim_shooting` 与 `SOUND_MAGNETSHROOM`，不增加帧事件；射击轨结束返回
  `anim_nonactive_idle2`，总充能从锁定当帧起计 15 游戏秒，完成后回到随机 10～15 FPS
  `anim_idle`。
- 普通/快速/水路铁桶、普通/粉色橄榄球头盔、普通铁门、弹跳杆、普通小丑盒和普通
  矿工镐可被吸取。加固铁门、精英小丑和爆破工头按主人平衡要求固定免疫；魅惑、死亡、
  无头或已失去装备的僵尸不是目标。
- 剥离不复用会喷粒子或结算附加能力的派生破甲入口；粉色头盔不会产生范围伤害，普通
  小丑进入可存档的 `DISARMED` 普通步行态，地下矿工复用正式 `LosePickaxe()` 状态机。

## 视觉、资源与存档

- 离体装备从当前装备轨道世界位置起飞，每固定逻辑步移动剩余向量的 5%；当前宽屏资源
  以植物动态视觉锚点为基准再向右校正 25px，解决初版落点偏左。贴图使用装备当前损伤
  阶段，抵达后保留到 15 秒充能结束。
- 植物存档保存 phase、剩余充能、贴图键、离体世界坐标、目标偏移和缩放；目标僵尸保存
  自己的无装备 phase。快照往返不会复原装备、重放音效或丢失飞行物。
- 权威资源为 `Magnetshroom.reanim`、`PlantImage/Magnetshroom.png`、对应 reanim 分层 PNG
  与 `sounds/Plant/magnetshroom.ogg`；`resources.xml`、动画/资源键、工厂、gamedata、图鉴
  和 AutoTest 资源加载断言形成闭环。

## 验证

- 2026-08-02 `clang-release` 配置/构建通过。当前桌面可见运行
  `smoke_magnetshroom`，窗口确认、exit 0、`run.log` 以 `script finished OK` 结束；覆盖
  最近目标、15 秒充能、离体物/小丑存档、粉色头盔无副作用、普通门/杆/盒/镐与三类
  免疫目标、白天睡眠和资源闭环。
- 同日可见回归 `smoke_jack_in_the_box`、`smoke_elite_jack_in_the_box`、
  `smoke_digger`、`smoke_elite_digger` 均 exit 0；最终提交前仍应以当前源码重新核实。
