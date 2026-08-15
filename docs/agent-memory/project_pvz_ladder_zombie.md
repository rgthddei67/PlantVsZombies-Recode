---
name: project_pvz_ladder_zombie
description: 2026-08-04 经典扶梯僵尸、Board 共享扶梯、通用攀爬、磁吸与植物生命周期的实现和验证契约
metadata:
  node_type: memory
  type: project
---

# 经典扶梯僵尸与共享扶梯

## 当前实现（2026-08-04）

- `ZOMBIE_LADDER` 由 `LadderZombie` 实装：500 本体、500 二类扶梯护盾。携梯保存 C# `mVelX=0.79～0.81`、卸梯保存 `0.23～0.37`；固定 `mSpeed=12` 承担资源 FPS 根运动换算，再由 `mVelX × 47 × 47 / (66 × 12)` 得到 walk clip（携梯 `2.20～2.26`、卸梯 `0.64～1.03`），避免位移与脚步脱节。两套啃食按 C# 36 FPS 使用 clip `3.0`。主人确认的动画全局帧直接使用：`anim_laddereat` 85、`anim_death` 131、`anim_eat` 194，不再减一。
- 状态机为 `CARRYING → PLACING → NORMAL`。携梯遇坚果、高坚果或南瓜头且该格无梯时，以 24 FPS 播放一次 `anim_placeladder`；完成后重新验证目标，成功则放梯、播放 `SOUND_LADDER_ZOMBIE`、自身立即攀爬并卸盾，目标消失则恢复携梯。魅惑中严格保持 C# `UpdateLadder` 早退语义。
- 扶梯护盾低于 2/3、1/3 时使用 damage1/damage2；破盾生成 `ZombieLadder`，磁吸卸梯不生成破盾粒子。断臂换 `Zombie_ladder_outerarm_upper2` 并发 `LadderArmOff`，掉头隐藏两头轨并发 `ZombieLadderHeadOff`。

## Board 扶梯与交互

- 已放置 `Ladder` 是轻量 GameObject：GameObjectManager 持所有权，Board 用 weak_ptr 做唯一格查询和存档。绘制以当前格中心/行高换算原版 `cellLeft+25, cellTop-4, scale=0.8`，资源键为 `IMAGE_ZOMBIE_LADDER_5`。
- `Zombie` 基类保存 `NONE/CLIMBING/FALLING`、高度和已用列；攀爬 80 px/s、慢速地面僵尸额外前移 50 px/s、目标高度 90 px，下落 100 px/s。Digger、飞行/蹦极、仍持跳杆的 Pogo 等阶段通过现有地面能力门禁拒绝；撑杆必须在起跳前先检查扶梯并保留撑杆。
- C# `UpdatePlaying` 每帧只对 `FindPlantTarget` 的单一目标处理扶梯；本项目的碰撞回调可能同帧覆盖相邻两格。因此 `TryStartLadderClimb` 只在当前啃食目标与梯子宿主同格时停止啃食，已经爬过的旧梯子格不得取消下一格植物的啃食，否则会每帧硬切 `anim_walk` 并产生无动画平移。
- 非咖啡豆植物正式死亡或压扁移除同格扶梯；Jalapeno 移除整行。扶梯消失时正在攀爬的僵尸转为下落。MagnetShroom 先选附近僵尸装备，若无可吸装备再按 C# 两格 Chebyshev 评分吸已放置扶梯。
- 强/超强台风成功搬运植物组合时，Board 在同一逐格事务中把附着扶梯切到目标格；扶梯绘制与磁吸起点直接消费目标格宿主植物的二维 `mGridMoveVisualOffset`，因此平地和屋顶 0.45 秒追赶都不会留在源格或与植物漂移。阻挡时仍留源格，出界/弹坑继续由植物死亡链移除。
- 关卡存档可选 `ladders[{row,column}]`，Zombie 公共存档保存攀爬瞬态；旧档缺字段默认为无梯、未攀爬。扶梯僵尸自身 phase、护盾破损阶段、放置目标格和当前 C# walk velocity 也入档。

## 数据、冒险与验证

- 生存数据为 `weight=1000`、`appearWave=10`、`survivalRound=10`。当前冒险 5-3（内部 level 39）首次接入普通扶梯；5-4（level 40）继续包含普通/精英扶梯，二者均保留原阵容而不加入投篮车。新增 5-6（level 42）再以普通扶梯与蹦极、投篮车做综合。
- 主人提供的 `Zombie_ladder.reanim` 与全部 `Zombie_ladder_*` 贴图、`ZombieLadderHead.png`、扶梯音效及三个粒子 XML 均走权威 `build/clang-release/resources`，专项逐项断言 reanim/运行时换图/声音加载，不能只看 manifest。
- `clang-release` 构建通过。主人当前桌面可见运行 `smoke_ladder_zombie.json`，窗口标题“植物大战僵尸中文版”、exit 0、208 条命令、`run.log` 为 `script finished OK`；除携梯、放梯、共享攀爬、死亡与磁吸外，草坪双坚果路径断言第二目标正常开吃，屋顶专项则锁定“旧梯子碰撞回调不得取消相邻植物啃食”。该专项只覆盖第一种平移，不能代表所有“动画不动但仍移动”均已解决。
- 主人后来更新的 `level37_data.json` 暴露第二种同症状根因：倭瓜处于 `RISING`、自身 collider 已关闭，但 Cell 物理顶层仍是倭瓜；其下花盆持续产生碰撞。旧 `StartEat` 每帧无条件把花盆替换为倭瓜，下一帧目标校验又因倭瓜 collider 关闭而停吃并恢复走路，随后碰撞再次重播 `anim_eat`，于是第 179 帧冻结而 X 按普通行走速度移动。C# `CanTargetPlant(Chew)` 只在 `EatingOrder` 顶层自身仍合法时阻挡下层，且 `SquashRising/Falling/DoneFalling` 均由 `NotOnGround()` 排除。现已让 `Squash::CanBeEaten` 精确排除 RISING/FALLING/LANDED，并把 `Zombie::IsPlantValidEatTarget` 改为最高**有效**层递归语义；2026-08-04 `clang-release` 编译通过，按主人要求未跑 AutoTest、待亲自实机验证。
- 台风附着修复后，新增可见屋顶专项 `smoke_ladder_typhoon.json`：严重台风把第 5 列花盆+坚果+扶梯组合移到第 6 列，滑动中扶梯二维偏移非零且与宿主误差恒为 0，结束归零；同步截图目验跨坡与落点均贴合。`clang-release`、该专项、完整 `smoke_ladder_zombie.json` 与 `smoke_typhoon.json` 均 exit 0、窗口标题确认、`script finished OK`。

## 可复用契约

- 僵尸部署、全体共享的格对象归 Board 所有；部署者只创建/移除，消费者的共享移动瞬态放基类，特殊移动品种在自己的接触入口先查通路再处理起跳或其他能力。
- Board 格对象的闭环不止“能画”：必须包含唯一格 API、对象所有权、植物死亡/压扁与行级清除、外部吸取、旧档中性默认、部署者与消费者瞬态存档，以及资源加载和可见截图。
- 随植物格附着的 Board 对象必须加入植物阵风的唯一逐格结算点，并共享宿主视觉偏移；只更新对象逻辑格会在 0.45 秒追赶期提前跳到目标，只共享视觉偏移而不换格则会永久浮在源格。
- 把 C# 单目标轮询改成多碰撞回调时，任何“停止当前动作”的分支都必须先验证当前动作目标与本次回调对象是同一实体或同一格；相邻旧通路只能跳过自身宿主，不能清掉下一目标状态。回归应在两格碰撞重叠窗口内断言状态和动画，而不是长等到最终仍会开吃。
- C# `EatingOrder` 返回物理顶层不等于物理顶层永远遮挡下层；`CanTargetPlant(Chew)` 会递归验证顶层资格。组合格回归必须覆盖上层临时离地、下层 collider 仍活跃的逐帧窗口，同时观察 `isEating/eatPlantID/animFrame/X`，只看最终截图或单帧 track 会漏掉“停吃→走一步→同帧重开”的振荡。

## 2026-08-15 植物爆炸清除扶梯

- 原版 `Board::KillAllZombiesInRadius` 对僵尸使用像素圆与行范围，却把扶梯另按爆心所在格的方形
  `rowRange` 清除：樱桃炸弹和玉米加农炮为中心格 ±1，毁灭菇为 ±3。当前实现收口到
  `Board::RemoveLaddersInBlastSquare`，以显式逻辑行和爆点 X 解析中心格，避免泳池美术下沉或屋顶坡面
  把梯子范围带偏。
- `smoke_cherry_explosion_spread` 从 4 把清到 2 把并保留 `(0,4)/(2,7)`；
  `smoke_doomshroom` 保留水平距离 4 格的 `(2,0)/(2,8)`；`smoke_cob_cannon_core` 保留
  `(0,4)/(2,7)`。三份脚本均在 `clang-release` 当前桌面可见运行、exit 0、`status=passed`，
  `run.log` 为 `script finished OK`，同步截图与状态一致。
