---
name: project_pvz_jack_in_the_box_zombie
description: 2026-07-30 经典小丑僵尸的随机开盒状态机、共享循环音效、范围爆炸、残肢、资源、4-1/4-2 接入与可见验证
metadata:
  node_type: memory
  type: project
---

# 经典小丑僵尸

## 当前实现

- `ZOMBIE_JACK_IN_THE_BOX` 为 500 HP，行走速度实例随机 `0.66～0.68`；常规开盒倒计时
  为 `(450～749) / mVelX * 2` 厘秒，`1/20` 概率再除以 3。啃食时仍倒计时，
  冻结时暂停。
- 主人提供的帧号按仓库口径直接注册：`anim_eat` 第 45 帧啃食，`anim_pop`
  第 66 帧爆炸，`anim_death` 第 89 帧消失。
- RUNNING → POPPING 时停止啃食与移动，停止手摇盒循环声并播放 `boing`；
  0.3 秒后按 2:1 播放两条 surprise 之一。爆炸帧播放 `explosion`，伤害 1800，
  僵尸半径 115px、植物半径 90px，使用圆与 collider 矩形相交，不照抄 C# 的
  800×600 绝对坐标。
- 未魅惑小丑杀范围内植物和魅惑僵尸，保留普通僵尸；魅惑小丑保留植物及魅惑僵尸，
  只杀普通僵尸。两侧统一按“只伤害敌对阵营僵尸”筛选。POPPING 拒绝普通伤害、
  冻结与重新啃食，爆炸后自身立即回收。
- 南瓜头的特殊僵尸范围拦截不改变经典小丑：未魅惑经典小丑仍对爆区内每株植物直接
  `Die()`，因此睡莲、内层植物和南瓜头整组清除；`smoke_pumpkin_zombie_area_damage`
  与完整 `smoke_jack_in_the_box` 均锁定此豁免。
- `jackinthebox` 是物种共享循环声：实例用静态引用计数申领和释放，避免一只小丑
  掉头/开盒时误停其他实例。读档恢复 RUNNING 循环声，不重播出生、开盒、惊吓、
  爆炸等一次性声音。
- 断臂把 `zombie_jackbox_outerarm_lower` 换成 `lower2` 并发射专属手臂粒子。
  C# 原版掉头没有小丑专用覆写，当前沿用自带完整下巴的普通 `ZombieHeadOff`；
  掉头同时停止循环声，随后走无头流血和第 89 帧死亡。
- 2026-08-02 接入磁力菇装备剥离：仅 RUNNING 普通小丑可被吸走盒子，随后进入持久化
  `DISARMED`，停止循环声与开盒倒计时、隐藏盒子/把手，并以 `0.23～0.37 px/tick`
  普通步速继续走路和啃食；剥离状态与速度参与快照，读档不恢复盒子或循环声。

## 资源与粒子

- 权威资源位于 `build/clang-release/resources`；其他 preset 通过 Junction 共享。
  包含 `Zombie_JackBox.reanim`、46 张 reanim 部件、4 条音效、专属手臂图、
  `Sproing` 和 `JackExplode`/`ZombieJackboxArmOff` 两份配置。
- 小丑与樱桃的爆炸云曾因把 C# 粒子阻力曲线 `.15,40 1` 直接迁入本引擎，
  在逐帧 `velocity *= 1 - friction` 语义下迅速停在中心。现改为低恒定阻力：
  小丑云 `0.015/0.02`，樱桃云 `0.015/0.02`，并把爆发粒数、半径、速度和寿命
  配成真正向外扩散。
- `smoke_cherry_explosion_spread` 实测樱桃效果 27 quad，世界包围盒
  `265×284px`，生命周期结束后粒子数量归零。

## 关卡与验证

- 4-1（内部 28）为普通、路障、铁桶、小丑；4-2（内部 29）为普通、路障、小丑、
  普通橄榄球。
  小丑在 4-1 游玩时尚未提前显示在图鉴，通关 4-1、进入 4-2 后成为第 19 个条目。
- `smoke_jack_in_the_box` 在可见窗口完整覆盖循环声、存读档、不重播一次性声音、
  第 45 帧啃食、boing/surprise/explosion、双向爆炸阵营规则（每侧同时放普通与
  魅惑僵尸，并核对植物）、专属大范围粒子、
  POPPING 免伤、断臂/掉头、普通完整头粒子、第 89 帧死亡以及魅惑小丑。
- `smoke_jack_loop_ownership` 同时生成两只小丑，断言一只开盒和爆炸后另一只仍维持
  循环声，最后一只掉头后循环声才停止。
- `smoke_jack_almanac` 在可见窗口断言 4-1/4-2 有序出怪池和图鉴解锁边界；
  三条脚本均 exit 0 且 `run.log` 以 `script finished OK` 结束。
