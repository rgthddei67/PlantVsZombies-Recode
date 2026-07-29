---
name: project_pvz_seashroom
description: 2026-07-29 经典海蘑菇实现、直接落水规则与可见 AutoTest 验证
metadata:
  node_type: memory
  type: project
---

# 经典海蘑菇

## 当前契约（2026-07-29）

- `PLANT_SEASHROOM` 由 `SeaShroom : PuffShroom` 实装，复用小喷菇的 1.5 秒攻击间隔、
  300px 本行短程索敌、`BULLET_PUFF`、`SOUND_PUFF` 和射击计时存档。
- `SeaShroom.reanim` 为 12fps；包装轨分别为 idle 0～24、shooting 25～38、
  sleep 39～63、aquarium 64～88。主人指定全局第 33 帧发射，其他包装轨不会经过该帧。
- 海蘑菇只能直接种在空水格，不需要睡莲，陆地禁种；它占 `Cell::normal`，因此已有睡莲
  或其他植物的水格也拒绝。睡莲仍是唯一占 `under` 的植物。
- 海蘑菇不绘制陆地植物阴影；本体与孢子出生点使用公共水面视觉锚点跟随 ±2px 浮动。
- `gamedata.json` 为 0 阳光、10 秒冷却，视觉偏移 `[-37.6,-5.0]`；图鉴名称使用“海蘑菇”。
- 冒险 3-9 奖励表原本已经指向 `PLANT_SEASHROOM`，本次注册后正式可实例化。

## 验证

`smoke_seashroom.json` 在主人当前桌面可见运行：

- 夜间泳池断言空水 true、陆地 false、睡莲水格 false，normal/under 层级正确且无阴影；
- 注册表投影锁定 0 阳光与 10000ms 冷却；
- 日间泳池断言 `sleeping=true`、`anim_sleep`；
- 夜间 300px 内静止目标触发 `anim_shooting`，生成对象池 `BULLET_PUFF` 且
  `puffSoundRequestCount=1`；
- 默认实例化与 `-NoInstance` 都 exit 0、日志无错误标记，四张同步截图逐张正常。
  两路径的海蘑菇世界包围盒均为 left/top/right/bottom `422/312/468/360`。
