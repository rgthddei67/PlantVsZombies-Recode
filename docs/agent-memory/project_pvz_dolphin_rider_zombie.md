---
name: project_pvz_dolphin_rider_zombie
description: 2026-07-28 普通与精英海豚、岸上抛豚与C#分段裁剪、换轨锚点、3-7/3-8接入、跳跃阻拦和水中断肢约定
metadata:
  node_type: memory
  type: project
---

# 海豚僵尸

## 当前实现

`ZOMBIE_DOLPHIN_RIDER` 已接入冒险 3-7（level 25），只允许在 `WATER_POOL` 的第 2/3 水路生成。
3-7 为 20 波 `{normal, cone, bucket, dolphin rider}`；图鉴按既有冒险进度规则在通关 3-7 后解锁，
预览固定播放 `anim_idle`。

状态机为 `APPROACHING → ENTERING_POOL → RIDING → JUMPING → SWIMMING`，离开水面后进入
`WALKING_WITHOUT_DOLPHIN`。本体 500 HP；带豚陆地步行按原版 `0.9px/tick × 47tick/s` 对齐为约
42px/s，弃豚后约 14px/s。陆地根运动以资源 12 FPS 为时间基准，分别使用 `2.25/0.75` clip 倍率
同步脚步与位移，禁止再把期望世界速度直接写进 `mSpeed` 造成滑行。首次遇到可吃植物
才跳跃，普通落地后弃豚并开始慢速游泳/步行。`ZombieJumpType` 与
`Plant::BlocksZombieJump(ZombieJumpType)` 已成为植物声明式阻拦入口；普通植物默认放行，
海豚在跳跃进度 30% 查询一次，为高坚果实现保留基础。撑杆僵尸也改为经同一入口查询，
默认行为不变。若一路没有遇到植物，海豚骑出泳池左边界后回到 `APPROACHING/anim_walkdolphin`，
不会把 `anim_ride` 滑上岸；这条换轨必须使用零 blend，因为阶段切换同帧撤销骑乘视觉补偿，
若保留通用 0.2 秒混合，旧骑乘姿态会按陆地坐标短暂垂挂到骑手脚下。

主人确认的全时间线帧事件为：啃食 `182/201`，死亡 `288`，代码直接使用，不再减一。
带海豚阶段断头直接死亡；弃豚后走 `anim_death`。手臂掉落延迟到弃豚后，再按 C# 阈值补结算。
断头隐藏 `anim_head1/anim_head2`，断臂隐藏手和下臂并将上臂换成 `UPPER2`。水中断头/断臂均不发射
任何掉落粒子；陆地断头使用 `ZombieDolphinRiderHeadOff`，断臂使用 `ZombieArmOff`。

## 坐标与音效约定

该 reanim 的 `gamedata` 视觉偏移为 `[-50,-85]`，不能把 C# 绝对点或视觉偏移再次叠加到粒子发射点。
陆地断头/断臂粒子从逻辑 `GetPosition()` 发射，由粒子配方自己的 `Position` 场完成局部头部偏移；
若从 `GetVisualPosition()` 发射，海豚头会被额外上移 85px 并跑到画面顶部。

`anim_ride` 相对弃豚游泳水线低约 45px，骑乘阶段使用 `-45px` 视觉抬升。海豚只在右岸外
0～20px 启动 `anim_jumpinpool`；动画结束后提交 70px 根位移并调用 `UpdatePoolState()`，因此
抛豚发生在岸上而不是水中。首次入水从 56% 水花节点到骑乘末端用 smoothstep 连续补偿，同时按
C# `GetDrawPos/DrawReanim` 语义仅在 0.56～0.65 与 0.75～结束启用派生类低位裁剪底线，当前项目
校准为逻辑 Y 下方 126/136px；中间窗口关闭裁剪。不能隐藏五条海豚部件轨，也不能复用普通水线
把骑手横切。`Zombie::TryGetDrawClipBottom` 的默认实现保持所有其他水中僵尸原行为，海豚只覆写
`ENTERING_POOL`。

海豚跳跃在 49% 落水后继续平滑补足剩余 15px。C# 的 94px 是逻辑位移，不能直接用作当前 reanim
换轨提交量：同一身体部件的 `anim_dolphinjump` 末帧与 `anim_swim/anim_ride` 首帧锚点连同阶段
视觉补偿计算后，当前项目分别提交 104px（弃豚）与 106px（保豚），否则落地会向右倒退约
10～12px。骑乘动画身体轨自身向左约 70px，因此 collider 保持基类逻辑偏移即可对齐，不再额外
平移 70px。战斗碰撞重建必须跳过 `mIsPreview`，否则会覆盖图鉴为条目设置的点击框，造成海豚条目
可见但点不中。影子偏移为 `(8,42)`，图鉴 idle 预览也应用。

`SOUND_DOLPHIN_APPEARS` 在每只海豚僵尸正式刷新成功时立即播放一次，不绑定入水或其他状态节点。
出生音效经 `Zombie::PlaySpawnSound()` 从 `Board::CreateZombie` 的正式新建路径调用，预览和
`CreateZombieWithID` 读档恢复不会误响。C# `UpdateZombieDolphinRider` 还在发现目标、切入跳跃的
同一节点依次播放 `DolphinBeforeJumping` 与 `PlantWater`；该跳跃叫声与出生叫声是两个独立资源。
AutoTest 导出两枚请求计数，专项断言 appears 只随正式直造增加，before-jump 只在真正开始越障时增加，
骑乘/跳跃快照往返均不重复播放。

2026-08-03 对齐 C# 两个水花节点：`anim_jumpinpool` 56% 与 `anim_dolphinjump` 49% 均调用通用
`Splash.reanim + PlantingPool` 双层视觉并播放一次 `ZombieEnteringWater`；开始越障时已有的
`PlantWater` 保留，49% 节点不再额外重播第二次。入水动作结束调用 `UpdatePoolState(false)`
只提交介质/裁剪，避免与 56% 节点重复。更新后的 `smoke_dolphin_rider.json` 在当前桌面可见
`clang-release` 运行 exit 0，节点前两层计数为 0、节点后各为 1、骑乘稳态归零，快照往返不重响。

## 验证

`smoke_dolphin_rider.json` 覆盖 3-7 出怪表、仅水路兼容、陆地根运动步频、右岸抛豚、入水
0.56～0.65/0.65～0.75/0.75～结束三个时间窗、骑乘、跳跃锚点、骑乘/跳跃快照往返、弃豚啃食、
水中断肢零粒子、死亡帧、陆地专属断头粒子、无植物时骑出泳池及上岸首帧零 blend，以及刷新声与
起跳声的独立触发次数；
主人当前桌面可见运行
exit 0，日志 `script finished OK`。同步截图确认骑乘水线、碰撞、入水/落水连续性和断头粒子位置。
`smoke_dolphin_rider_almanac.json` 覆盖 3-7 通关前后解锁与 idle 预览；`smoke_polevaulter_vault_walk.json`
可见回归 exit 0，证明默认植物未改变撑杆翻越。生存候选池现按当前背景至少存在一个兼容行再纳入类型，
因此保留海豚原版 `survivalRound=10`，但草地/黑夜无尽不会抽到无法落行的海豚；
`smoke_survival_spawn_round.json` 可见回归 exit 0。

## 精英变体与高坚果阻拦补充（2026-07-28）

`TallNut` 已实现 `Plant::BlocksZombieJump(DOLPHIN_RIDER)`。海豚仍只在跳跃进度 30% 的既有
单次判定点查询；命中后调用植物侧 `OnZombieJumpBlocked` 播放 Bonk 与星星，再走
`FinishJump(true)` 弃豚回到 `SWIMMING`。Bonk 不再由 `FinishJump` 自己播放，避免不同阻拦
植物或调用路径重复反馈。`smoke_tallnut_dolphin.json` 在主人当前桌面可见运行退出码 0，
断言阻拦检查一次、Bonk 一次、跳跃前音效一次和 `TallNutBlock` 一次。

`EliteDolphinRiderZombie` 复用父类完整时间线与全部修正，700 HP，`GetDolphinJumpCapacity()=2`：
第一次成功越障直接返回 `anim_ride` 并保留海豚，第二次才弃豚；成功次数和当前跳跃返回语义进入
快照。被高坚果挡下时无论第几跳都弃豚并开始啃食，再由派生钩子对顶层高坚果结算
`TakeDamage(500, DamageSource::ZOMBIE)`。3-8 为20波
`{normal, cone, bucket, dolphin rider, elite dolphin rider}`，精英正式波次每波最多1只且计数
保存/恢复、新波和生存轮清归零。完整数据、资源与验收见
[project_pvz_elite_dolphin_rider_zombie](project_pvz_elite_dolphin_rider_zombie.md)。
