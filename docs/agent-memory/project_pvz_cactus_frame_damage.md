---
name: project_pvz_cactus_frame_damage
description: 经典仙人掌、空地分层索敌、固定逻辑帧伤害尖刺、四目标穿透、对象池与存档契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-07-30
---

# 仙人掌与帧伤尖刺

> 2026-08-12 当前源码复核：`Bullet.cpp::kSpikeFrameDamage` 已为 2，但既有
> `smoke_cactus.json` 仍保留历史 3 点断言并会在首个 `baseDamage` 处失败。以下旧结论中的 3 点数值
> 已过时；正式调回 3 或接受 2 前应先由主人定案并同步脚本。绝缘僵尸专项按当前 2 点基线验证干甲减半为每帧 1 点。

2026-07-30 已完成空/地双形态逻辑。`PLANT_CACTUS` 为 125 阳光、7.5 秒冷却、300 生命；主人指定地面 `anim_shooting` 第 26 帧从稳定视觉锚点 `(30,-27)` 发射，空中 `anim_shootinghigh` 第 70 帧从 `(53,-100)` 发射。空中目标优先：低姿态发现飞行目标后经 `anim_rise` 进入 HIGH，高姿态失去飞行目标后经 `anim_lower` 回到 LOW；伸缩或射击中不抢占动画，帧回调同时核对 phase 和当前轨道。姿态缓存可持续扫描，但普通攻击仍保持既有节奏：1.5 秒冷却满足后才启动 0.6 秒目标确认，避免新缓存让首发提前。

## 空地索敌契约

- `Plant::CanAcquireZombie` 默认只接受地面命中层，`Board::CanPlantAcquireZombie` 先调用它，再叠加原有雾可见性；Cactus 覆写为接受两层并分别缓存地面/空中目标。
- `Bullet::mTargetsFlying` 在 Cactus 发射时按姿态写入，碰撞时向僵尸查询当前阶段；POPPING 过渡期两层都不可命中。字段随正式存档保存，并在对象池 `Reset()` 恢复 false。
- AutoTest 用 `flyingTargetSpikeCount`、`groundTargetSpikeCount` 及对应穿透目标聚合数取证，禁止依赖无序 `bullets.N` 数组。

## 尖刺伤害与穿透

- 尖刺对空中和陆地目标使用同一伤害链，但只碰撞发射时指定的高度层；以 1x 下每次 `onTriggerEnter/onTriggerStay` 造成 3 点植物伤害为基准。
- 持门加固铁门通过 `Zombie::ModifySpikeFrameDamage` 把自己的 1x 每碰撞帧基础伤害限为 1；门掉后恢复 3。目标特例在倍速额度累计前生效，不能依赖后续拆成多个 `TakeDamage(1)` 的普通单击上限。
- 每发以僵尸稳定 ID 记录不同目标；同一目标持续重叠可逐逻辑帧受伤，但只在首次接触登记和播放命中反馈。第四个不同目标先承受本帧伤害，再令子弹消失。
- 项目固定逻辑步为 60Hz。渲染帧率不改变帧伤；`timeScale` 只缩放每步时间/位移，不改变每现实秒的逻辑步数，因此必须为每个目标累计 `damage × GetDeltaTime()/GetFixedStep()` 的小数伤害额度再取整。基础伤害 3 下，0.5x 每帧累计 1.5 点、1x 消费三个、2.0x 消费六个独立 1 点额度，使相同游戏时长的总伤害和逐次受击语义一致；不能把多个额度合成一次 `TakeDamage(N)`。

## 对象池与存档

- `BulletPool` 为 `BULLET_SPIKE` 分配独立槽位；`Reset()` 恢复 3 点基础伤害、地面命中层、贴图/速度/阴影，并清空已穿透 ID 和逐目标伤害余额。
- `GameInfoSaver` 保存 `targetsFlying`、`piercedZombieIDs` 与对齐的 `spikeDamageRemainders`；旧档缺字段时视为地面/空名单/0。读档按不可变 `poolType` 取得池槽后再恢复名单，保持 `fromPool=true`。
- AutoTest 状态导出提供空/地尖刺数量与各层穿透目标聚合数，以及逐弹诊断字段，稳定断言不依赖无序子弹数组的位置。

## 验证

`smoke_cactus.json` 覆盖植物注册、费用/冷却、地面射击第 26 帧、连续帧伤、第三目标后的存读档、第四目标承伤后消弹、对象池复用清零、0.5x/1x/2.0x 各 90 点等伤、加固铁门持门 1 点/掉门 3 点和 2-9 奖励解锁。`smoke_balloon_cactus.json` 新增高姿态第 70 帧、空/地弹互斥、伸缩状态、气球爆裂落地、水道直接移除与存读档覆盖。2026-07-30 两条脚本均在主人当前桌面可见运行，分别 129/109 条命令、exit 0，`run.log` 均以 `script finished OK` 结束。
