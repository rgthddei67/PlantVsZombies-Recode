---
name: project_pvz_dolphin_rider_zombie
description: 2026-07-27 海豚僵尸、首次入水与越障状态机、3-7 接入、跳跃阻拦扩展点和水中断肢约定
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
`WALKING_WITHOUT_DOLPHIN`。本体 500 HP；带海豚阶段约 54px/s，弃豚后约 18px/s。首次遇到可吃植物
才跳跃，普通落地后弃豚并开始慢速游泳/步行。`ZombieJumpType` 与
`Plant::BlocksZombieJump(ZombieJumpType)` 已成为植物声明式阻拦入口；普通植物默认放行，
海豚在跳跃进度 30% 查询一次，为高坚果实现保留基础。撑杆僵尸也改为经同一入口查询，
默认行为不变。若一路没有遇到植物，海豚骑出泳池左边界后回到 `APPROACHING/anim_walkdolphin`，
不会把 `anim_ride` 滑上岸。

主人确认的全时间线帧事件为：啃食 `182/201`，死亡 `288`，代码直接使用，不再减一。
带海豚阶段断头直接死亡；弃豚后走 `anim_death`。手臂掉落延迟到弃豚后，再按 C# 阈值补结算。
断头隐藏 `anim_head1/anim_head2`，断臂隐藏手和下臂并将上臂换成 `UPPER2`。水中断头/断臂均不发射
任何掉落粒子；陆地断头使用 `ZombieDolphinRiderHeadOff`，断臂使用 `ZombieArmOff`。

## 坐标与音效约定

该 reanim 的 `gamedata` 视觉偏移为 `[-50,-85]`，不能把 C# 绝对点或视觉偏移再次叠加到粒子发射点。
陆地断头/断臂粒子从逻辑 `GetPosition()` 发射，由粒子配方自己的 `Position` 场完成局部头部偏移；
若从 `GetVisualPosition()` 发射，海豚头会被额外上移 85px 并跑到画面顶部。

`anim_ride` 相对弃豚游泳水线低约 45px，骑乘阶段使用 `-45px` 视觉抬升。首次入水从 56% 水花节点到
骑乘末端用 smoothstep 连续补偿；海豚跳跃在 49% 落水后继续平滑补足剩余 15px，避免换轨瞬间先沉到
池底再跳回。骑乘动画身体轨自身向左约 70px，因此 collider 保持基类逻辑偏移即可对齐，不再额外
平移 70px。战斗碰撞重建必须跳过 `mIsPreview`，否则会覆盖图鉴为条目设置的点击框，造成海豚条目
可见但点不中。影子偏移为 `(8,42)`，图鉴 idle 预览也应用。

按主人最终确认，`SOUND_DOLPHIN_APPEARS` 在每只海豚僵尸正式刷新成功时立即播放一次，不绑定入水
或其他状态节点；越过植物也不播放 `SOUND_DOLPHIN_BEFORE_JUMPING`，只保留植物/落水声。出生音效经
`Zombie::PlaySpawnSound()` 从 `Board::CreateZombie` 的正式新建路径调用，预览和
`CreateZombieWithID` 读档恢复不会误响。AutoTest 导出两枚请求计数，专项断言每次直造后 appears
立即加一、入水/越障/骑乘与跳跃快照往返均不再增加，before-jump 始终为 0。

## 验证

`smoke_dolphin_rider.json` 覆盖 3-7 出怪表、仅水路兼容、首次入水、骑乘、跳跃、骑乘/跳跃快照往返、
弃豚啃食、水中断肢零粒子、死亡帧、陆地专属断头粒子、无植物时骑出泳池以及刷新音效次数；
主人当前桌面可见运行
exit 0，日志 `script finished OK`。同步截图确认骑乘水线、碰撞、入水/落水连续性和断头粒子位置。
`smoke_dolphin_rider_almanac.json` 覆盖 3-7 通关前后解锁与 idle 预览；`smoke_polevaulter_vault_walk.json`
可见回归 exit 0，证明默认植物未改变撑杆翻越。生存候选池现按当前背景至少存在一个兼容行再纳入类型，
因此保留海豚原版 `survivalRound=10`，但草地/黑夜无尽不会抽到无法落行的海豚；
`smoke_survival_spawn_round.json` 可见回归 exit 0。
