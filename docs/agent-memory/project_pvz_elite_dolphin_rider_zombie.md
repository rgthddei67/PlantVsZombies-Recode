---
name: project_pvz_elite_dolphin_rider_zombie
description: 2026-07-28 精英海豚骑士的两次越障、高坚果500撞击、3-8每波上限、独立换色资源与验证
metadata:
  node_type: memory
  type: project
---

# 精英海豚骑士僵尸

## 当前实现

`ZOMBIE_ELITE_DOLPHIN_RIDER` 是 `DolphinRiderZombie` 的数据/规则变体：700 HP，速度、声音、
入水、断肢和跳跃时间线全部复用父类。普通海豚的越障容量为1，精英为2；第一次成功跳跃直接让
一次性动画返回 `anim_ride`，提交106px换轨位移并保留海豚，第二次返回 `anim_swim`、提交104px
并弃豚。`successfulJumpCount` 与 `jumpRetainsDolphinOnLanding` 进入品种存档，跳跃中快照往返
不会重复计数或切错返回轨。

高坚果仍只在海豚跳跃进度30%的唯一节点声明阻挡并播放 Bonk/星星。父类先恢复碰撞、阴影、弃豚
与啃食，再把当前格顶层阻拦植物传给派生钩子；精英调用
`Plant::TakeDamage(500, DamageSource::ZOMBIE)`，因此生存模式僵尸增伤与植物韧性继续统一生效，
精英本体不扣血。普通海豚的钩子为空。

3-8（level26）为20波
`{normal, cone, bucket, dolphin rider, elite dolphin rider}`，只允许两条水路。正式波次每波
最多创建1只精英海豚；计数进入关卡存档，同波快照恢复不能绕过上限，新波与生存轮清时归零。
图鉴按既有冒险进度规则在通关3-8后解锁。

## 资源

骑手红色潜水服换为深海蓝，海豚灰色换为粉白/紫粉色，保留肤色、白腹、眼睛、轮廓、阴影与
Alpha。`scripts/recolor_elite_dolphin_rider.ps1` 从普通海豚权威资源生成23张 reanim 部件、
独立 `EliteDolphinRider.reanim` 与 `ZombieEliteDolphinRiderHead.png`，共25个输出逐文件锁定
SHA-256；直接运行受系统策略限制时用
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/recolor_elite_dolphin_rider.ps1`。
粒子配置 `EliteDolphinRiderHeadOff.xml` 复用普通掉头物理参数但指向独立蓝色头贴图；断臂
`outerarm_upper2` 也经父类虚资源键切到精英材质，水中仍不发射头/臂粒子。

## 验证

2026-07-28 `clang-playtest` 与 `clang-release` LTO 构建通过。以下脚本均从
`build/clang-playtest/` 在主人当前桌面可见运行，退出码0且 `run.log` 以
`script finished OK` 结束：

- `smoke_elite_dolphin_rider.json`：700 HP、第一次保豚、跳跃快照、第二次弃豚/上岸、蓝色骑手、
  粉色海豚与独立蓝色头粒子。
- `smoke_tallnut_elite_dolphin.json`：30%节点前高坚果9000、无 Bonk/星星；节点后精英700不变、
  高坚果精确8500并开始啃食，Bonk/星星各一次。
- `smoke_elite_dolphin_rider_wave_cap.json`：同波第二只跳过、计数快照恢复、新波归零后可再生成。
- `smoke_elite_dolphin_rider_almanac.json`：通关3-8后解锁并显示独立 idle 预览。

普通 `smoke_dolphin_rider.json` 与 `smoke_pool_zombie_visuals.json` 同时通过，证明父类入水、
上岸与派生裁剪修正没有改变其他水中僵尸。详细公共动画契约见
[project_pvz_dolphin_rider_zombie](project_pvz_dolphin_rider_zombie.md)。
