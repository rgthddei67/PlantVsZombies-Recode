---
name: project_pvz_projectile_shield_direction
description: 所有 Bullet 按命中时实际水平来向判断二类护盾正背面，背击绕盾只伤后层
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-01
---

# 子弹按来向命中二类护盾

## 当前契约

- 所有 `Bullet` 对僵尸的直接伤害统一走 `Zombie::TakeProjectileDamage`，禁止按 `BulletType` 或 `Backwards/Homing` 等运动模式维护绕盾白名单。以后猫尾草等追踪弹只要持续更新命中时真实 `velocityX`，无需新增铁门特例。
- `Zombie::ShouldProjectileBypassShield` 用子弹 `velocityX` 与目标 `IsMovingRight()` 判断命中面：二者同向表示子弹从背后追上，完全绕过铁门、报纸、梯子等二类护盾，护盾不扣血，伤害直接进入飞行额外层、头盔和本体；反向表示正面命中。`velocityX == 0` 保留 AutoTest 历史正面口径。
- `bypassShield` 与大喷的 `penetrateShield` 不同：背击不伤盾、只伤后层；穿透会伤盾并把完整伤害继续传给后层。`discardShieldOverflow` 仍只影响正面护盾结算。
- 加固铁门的持门普通伤害上限与尖刺帧伤上限只适用于正面命中；背击绕门后恢复完整伤害。寒冰豆背击身体时允许减速并播放对应反馈；门/梯抗火也只在火豆实际命中正面护盾时生效。
- 正面命中只亮 `hitFlashMask` bit1，背击只亮 bit0；命中音效同样以实际承伤层为准。

## 验证

- `smoke_projectile_shield_direction.json` 可见专项覆盖 Pea、Snowpea、Fireball、Star、Puff 的正向/反向与静止口径，普通铁门、加固铁门、报纸的盾/本体生命与受击层，并用真实双向射手两发后豆确认铁门保持 1100、本体 270→230。
- 2026-08-01 `clang-release` 配置与 LTO 构建零警告；专项在主人当前桌面显示“植物大战僵尸中文版”并退出 0，三张截图与状态 JSON 已逐项复核。
- 同轮可见回归 `smoke_splitpea_fireball_direction.json` 与 `smoke_zombie_damage_flash.json` 均退出 0，分别证明反向火豌豆溅射/命中特效方向和既有穿透双层白光语义未回归。
