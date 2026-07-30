---
name: project_pvz_balloon_zombie
description: 经典气球僵尸、空地命中层、独立螺旋桨附件、水道击破与存档契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-07-30
---

# 经典气球僵尸

2026-07-30 参考 `PlantsVsZombies.NET-master` 的 `Zombie.cs` 接入 `ZOMBIE_BALLOON`。正式出生为 FLYING，20 点气球额外生命层、270 点本体生命，飞行速度随机 23～37 px/s；气球层承伤后把溢出继续传给本体，并随生存模式全局生命倍率缩放。冒险首次加入 4-3（level 30，wave 10 可出），`gamedata.json` 权重 2000、生存轮次 11。

## 动画与阶段

- 主 Animator 使用 `idle/swing/anim_pop/anim_eat/anim_walk/anim_death`；主人指定全局啃食事件帧 70、80，死亡回收帧 152。
- 资源末尾 `propeller` 轨不是废帧：第二个同 reanim Animator 只循环该轨，按 `hat` 首帧变换的逆锚点 `(1.875,-42.75)` 挂到帽子。主轨切换后清除附件继承的 clip 速度覆写，但保留冻结/减速 extra 层；断头时必须显式隐藏并暂停附件，因为隐藏父锚点不会隐藏 attachment。
- FLYING 只被对空弹丸命中、不能啃食或进入地面水池状态；气球归零后陆地进入 POPPING，`anim_pop` 结束才成为 WALKING。POPPING 两层都不可命中；WALKING 恢复地面命中、碰撞框、啃食、冻结与断肢阈值。
- `Zombie::TakeExtraProtectionDamage` 让气球生命在头盔/护盾/本体前消费；`ApplyExtraHealthMultiplier` 扩展全局生命倍率。飞行期间延迟的断臂/断头阈值在落地后统一补结算。

## 水道与存档

- 对齐 C# `LandFlyer`：泳池行击破气球时播放爆裂声后直接 `Die()`，不会播放落地或站在水面；专项关卡使用 level 19 的水道行断言 `zombieCount=0`。
- 主人指定灰烬致死统一直接 `Die()`：飞行、爆裂和落地阶段都不生成 `ZombieCharred`，也不进入普通死亡轨；只有足以覆盖当前本体加飞行气球层的最终灰烬伤害才直消，高血量生存模式下的非致死爆炸仍走正式 `PLANT_ASH` 扣血链。
- 存档保存 phase、气球当前/上限、飞行速度、螺旋桨帧与播放态；Load 按 phase 重建碰撞框、掉落许可、轨道可见性和附件终态，不重播出生/爆裂声音。
- 掉头粒子使用独立 `particles/ZombieBalloonHead.png`，按 `ParticleTextures` 生成的资源键为 `PARTICLE_ZOMBIEBALLOONHEAD`；不能继续复用 reanim 的单层头片，否则会缺少主人提供的完整头部造型。掉臂使用主人对照原版确认最接近的 `Zombie_polevaulter_innerhand.png`，由专属 `ZombieBalloonArmOff` 粒子预加载并发射，不复用通用 `ZombieArmOff`。
- `clang-playtest` 已编译通过且无警告。`smoke_balloon_cactus.json` 在主人当前桌面可见运行，153 条命令、exit 0、`run.log` 以 `script finished OK` 结束；覆盖出生声、螺旋桨、空地命中互斥、爆裂中存读档、陆地落地、水道直接移除、啃食、断臂、专属掉头、第 152 帧死亡，以及飞行/落地两态致死灰烬直消；另以 100 层僵尸生命词条锁定非致死灰烬仍保留 1970 本体生命。12 张同步截图已人工核对。
- `smoke_fog_spawnlists_4_1_to_4_2.json` 同样可见 exit 0，确认 4-3 的完整有序出怪池为普通、路障、气球，选卡预览能正确绘制气球僵尸。

## 2026-07-30 三叶草连续吹飞

三叶草第 44 帧只为仍在 FLYING 的气球启动连续吹飞态，不再瞬移。朝屋后以 600 px/s 累计
精确移动 400 px 后停止吹飞；重复同向效果继续累加 400 px。朝前线以同速移动到
`SCENE_WIDTH + 80` 后才 `Die()`。该状态优先于普通飞行速度和台风漂移，并保存吹飞标志、
方向与剩余距离；气球爆裂时主动清除吹飞态。连续版本已通过 `clang-playtest` 编译，主人要求
停止自动启动游戏，当前手感和完整专项由主人亲测。
