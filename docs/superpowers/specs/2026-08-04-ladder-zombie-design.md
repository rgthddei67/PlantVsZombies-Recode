# 扶梯僵尸设计

日期：2026-08-04

## 目标与边界

按 `PlantsVsZombies.NET-master/Lawn_Shared/Lawn` 的 C# 实现还原经典扶梯僵尸及其共享扶梯机制。主人已确认完全参照 C#，并确认动画事件帧直接使用：携梯啃食 85、死亡 131、卸梯后啃食 194。

本次包含扶梯僵尸、已放置扶梯、通用攀爬、磁力菇吸取、植物死亡/压扁与火爆辣椒清梯、存档、资源闭环和 AutoTest。冒险 `spawnlists.json` 暂不改：当前进度只编排到 5-2（内部 level 38），尚未包含原版扶梯首次登场的 5-3；提前塞进已有的 4-3 气球教学或其他首次教学关会违反仓库“一关一个重点威胁”的编排契约。扶梯仍以原版权重进入生存抽取，并可由开发菜单、图鉴和 AutoTest 创建。

## 扶梯僵尸状态机

`LadderZombie` 有三个阶段：

- `CARRYING`：500 本体生命、500 扶梯护盾，播放 `anim_ladderwalk`；保存 C# `mVelX=0.79～0.81 px/tick`，按原版 `mVelX × 47 × frameCount / groundDistance` 动态换算步频。当前 47 帧、66 px 根轨、12 FPS 资源对应 clip `2.20～2.26`，根运动换算基准固定为 12，位移与脚步同步，不能把世界速度直接塞进 `mSpeed`。
- `PLACING`：遇到同一行、前方接触范围内且尚无扶梯的坚果、高坚果或南瓜头时停止啃食与移动，播放一次 `anim_placeladder`，速度为资源 12 FPS 的 2 倍。
- `NORMAL`：扶梯被破坏、被磁力菇吸走或成功放置后，回落到普通 `anim_walk` / `anim_eat`；普通 `mVelX=0.23～0.37` 对应 walk clip `0.64～1.03`，两套啃食均按 C# 36 FPS 使用 clip `3.0`。

放梯动画结束时重新验证目标。目标仍有效时由 Board 在其格子创建扶梯、播放 `SOUND_LADDER_ZOMBIE`、立即让放梯者开始攀爬并卸下护盾；目标消失则回到 `CARRYING`。魅惑后严格遵循 C#：`UpdateLadder` 不再放梯。

携梯时啃其他植物使用 `anim_laddereat` 和第 85 帧；卸梯后使用 `anim_eat` 和第 194 帧。死亡播放 `anim_death`，第 131 帧回收。

## 护盾、断肢与磁力菇

扶梯是二类护盾，继承既有正反向弹丸命中与独立白光规则。耐久低于 2/3、1/3 时依次替换 `Zombie_ladder_1` 为 damage1、damage2；破盾时卸梯并生成原版 `ZombieLadder` 掉落粒子。

断臂隐藏外侧手掌与小臂，把 `Zombie_ladder_outerarm_upper` 换成断臂贴图；掉头隐藏 `anim_head1/anim_head2` 并使用 `ZombieLadderHead.png`。两者沿用断肢音效。

磁力菇优先沿用现有“僵尸装备”目标选择：携梯可按当前损坏阶段吸走，吸取不生成破盾掉落粒子。没有僵尸装备目标时，再按 C# 的两格 Chebyshev 范围和行偏置评分吸走已放置扶梯。

## 已放置扶梯与通用攀爬

`Ladder` 是 Board 持 weak_ptr 簿记、GameObjectManager 持所有权的轻量场景对象。每格最多一架，绘制使用 `IMAGE_ZOMBIE_LADDER_5`，锚点由当前 Board 格子中心换算，不照抄 800×600 的绝对坐标。

普通地面僵尸接触带梯植物时不啃食，而进入共享攀爬状态：

- `CLIMBING`：每游戏秒上升 80 px；水平根运动继续，慢速僵尸额外向前 50 px/s；目标高度 90 px。
- `FALLING`：到顶或扶梯消失后每游戏秒下降 100 px，落地恢复普通高度。

视觉位置叠加负高度，碰撞与阴影保留地面锚点。攀爬状态属于 Zombie 基类并存档，因此普通、路障、铁桶、卸杆跳跳、撑杆等地面步行僵尸复用；飞行、地下、车辆、蹦极和仍持跳杆的跳跳不使用扶梯。死亡中的攀爬者转为下落。

## 清除与存档

非悬浮植物死亡或被压扁时移除同格扶梯；火爆辣椒清除整行扶梯。清除后正在使用该梯的僵尸转为下落。

存档新增可选 `ladders` 数组（row/column），并在 Zombie 公共状态中保存 climb phase、altitude 和 use-ladder column。旧档缺字段时默认为无扶梯、未攀爬，不提升全局存档版本。

## 数据与资源

- `ZOMBIE_LADDER` 移到 `NUM_ZOMBIE_TYPES` 前并追加注册，避免改动已有实现类型的数值。
- 追加 `ANIM_LADDER_ZOMBIE`、`REANIM_LADDER_ZOMBIE`、四张扶梯运行时贴图键、扶梯音效与粒子配方。
- `gamedata.json` 使用 C# 原版：weight 1000、appearWave 10（当前单关最大波次口径）、survivalRound 10；偏移由本项目 1100×600 场景截图校验。
- 使用主人提供的 `Zombie_ladder.reanim` 和配套图片；资源必须从文件/清单经 loader 到实际键形成闭环。

## 验证

AutoTest 覆盖资源加载、两套啃食帧、死亡帧、护盾损坏/掉落、携梯与场景扶梯的磁力吸取、放梯、普通僵尸攀爬、清梯下落、植物死亡和辣椒联动，以及扶梯和攀爬中状态的保存/恢复。另以草坪和屋顶双坚果路径断言翻梯时 `anim_walk` 持续播放、接触第二株后切入 36 FPS `anim_eat` 并造成伤害。最终从 `build/clang-release` 在主人当前桌面可见运行，并检查退出码、`run.log`、状态 JSON、断言与截图。
