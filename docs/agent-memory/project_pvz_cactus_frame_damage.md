---
name: project_pvz_cactus_frame_damage
description: 经典仙人掌、固定逻辑帧伤害尖刺、四目标穿透、对象池与存档契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-07-30
---

# 仙人掌与帧伤尖刺

2026-07-30 完成地面形态。`PLANT_CACTUS` 为 125 阳光、7.5 秒冷却、300 生命，约每 1.5 秒索敌并播放 `anim_shooting`；主人指定在动画第 26 帧从稳定视觉锚点 `(30,-27)` 发射 `BULLET_SPIKE`。高形态及气球僵尸尚未实现，`anim_shootinghigh` 第 70 帧仅保留 TODO。

## 尖刺伤害与穿透

- 尖刺对空中和陆地目标使用同一碰撞链；以 1x 下每次 `onTriggerEnter/onTriggerStay` 造成 3 点植物伤害为基准。
- 持门加固铁门通过 `Zombie::ModifySpikeFrameDamage` 把自己的 1x 每碰撞帧基础伤害限为 1；门掉后恢复 3。目标特例在倍速额度累计前生效，不能依赖后续拆成多个 `TakeDamage(1)` 的普通单击上限。
- 每发以僵尸稳定 ID 记录不同目标；同一目标持续重叠可逐逻辑帧受伤，但只在首次接触登记和播放命中反馈。第四个不同目标先承受本帧伤害，再令子弹消失。
- 项目固定逻辑步为 60Hz。渲染帧率不改变帧伤；`timeScale` 只缩放每步时间/位移，不改变每现实秒的逻辑步数，因此必须为每个目标累计 `damage × GetDeltaTime()/GetFixedStep()` 的小数伤害额度再取整。基础伤害 3 下，0.5x 每帧累计 1.5 点、1x 消费三个、2.0x 消费六个独立 1 点额度，使相同游戏时长的总伤害和逐次受击语义一致；不能把多个额度合成一次 `TakeDamage(N)`。

## 对象池与存档

- `BulletPool` 为 `BULLET_SPIKE` 分配独立槽位；`Reset()` 恢复 3 点基础伤害、贴图/速度/阴影，并清空已穿透 ID 和逐目标伤害余额。
- `GameInfoSaver` 保存 `piercedZombieIDs` 与对齐的 `spikeDamageRemainders`；旧档缺字段时视为空/0。读档按不可变 `poolType` 取得池槽后再恢复名单，保持 `fromPool=true`。
- AutoTest 状态导出提供 `spikeBulletCount`、`piercedZombieCount`、`piercedZombieIDs` 和 `spikeDamageRemainders`，避免依赖无序子弹数组的位置。

## 验证

`smoke_cactus.json` 已同步覆盖植物注册、费用/冷却、地面射击第 26 帧、连续帧伤、第三目标后的存读档、第四目标承伤后消弹、对象池复用清零、0.5x/1x/2.0x 各 90 点等伤、加固铁门持门 1 点/掉门 3 点和 2-9 奖励解锁。此前 1 点版本的可见专项与截图通过，2 点版本仅完成 `clang-playtest` 增量构建；2026-07-30 主人把基础帧伤调至 3、穿透上限调至 4，并明确由主人自行编译。加固铁门专项脚本仅完成静态同步，本次未编译、未运行 AutoTest。
