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
- 非咖啡豆植物正式死亡或压扁移除同格扶梯；Jalapeno 移除整行。扶梯消失时正在攀爬的僵尸转为下落。MagnetShroom 先选附近僵尸装备，若无可吸装备再按 C# 两格 Chebyshev 评分吸已放置扶梯。
- 强/超强台风成功搬运植物组合时，Board 在同一逐格事务中把附着扶梯切到目标格；扶梯绘制与磁吸起点直接消费目标格宿主植物的二维 `mGridMoveVisualOffset`，因此平地和屋顶 0.45 秒追赶都不会留在源格或与植物漂移。阻挡时仍留源格，出界/弹坑继续由植物死亡链移除。
- 关卡存档可选 `ladders[{row,column}]`，Zombie 公共存档保存攀爬瞬态；旧档缺字段默认为无梯、未攀爬。扶梯僵尸自身 phase、护盾破损阶段、放置目标格和当前 C# walk velocity 也入档。

## 数据、冒险与验证

- 生存数据为 `weight=1000`、`appearWave=10`、`survivalRound=10`。当前冒险表只到 5-2（内部 level 38），未到原版首次出现的 5-3；不提前挤入已有首次教学关，等 5-3 编排时再接入。
- 主人提供的 `Zombie_ladder.reanim` 与全部 `Zombie_ladder_*` 贴图、`ZombieLadderHead.png`、扶梯音效及三个粒子 XML 均走权威 `build/clang-release/resources`，专项逐项断言 reanim/运行时换图/声音加载，不能只看 manifest。
- `clang-release` 构建通过。主人当前桌面可见运行 `smoke_ladder_zombie.json`，窗口标题“植物大战僵尸中文版”、exit 0、208 条命令、`run.log` 为 `script finished OK`；除携梯、放梯、共享攀爬、死亡与磁吸外，草坪和屋顶双坚果路径都断言翻梯 walk 持续播放、第二目标以 36 FPS 开吃并实际受伤，截图目验坡面/花盆/扶梯对齐。现有 `smoke_polevaulter_vault_walk.json`、`smoke_magnetshroom.json`、`smoke_jalapeno.json` 同样可见 exit 0。
- 台风附着修复后，新增可见屋顶专项 `smoke_ladder_typhoon.json`：严重台风把第 5 列花盆+坚果+扶梯组合移到第 6 列，滑动中扶梯二维偏移非零且与宿主误差恒为 0，结束归零；同步截图目验跨坡与落点均贴合。`clang-release`、该专项、完整 `smoke_ladder_zombie.json` 与 `smoke_typhoon.json` 均 exit 0、窗口标题确认、`script finished OK`。

## 可复用契约

- 僵尸部署、全体共享的格对象归 Board 所有；部署者只创建/移除，消费者的共享移动瞬态放基类，特殊移动品种在自己的接触入口先查通路再处理起跳或其他能力。
- Board 格对象的闭环不止“能画”：必须包含唯一格 API、对象所有权、植物死亡/压扁与行级清除、外部吸取、旧档中性默认、部署者与消费者瞬态存档，以及资源加载和可见截图。
- 随植物格附着的 Board 对象必须加入植物阵风的唯一逐格结算点，并共享宿主视觉偏移；只更新对象逻辑格会在 0.45 秒追赶期提前跳到目标，只共享视觉偏移而不换格则会永久浮在源格。
