# 寒冰大喷菇 IceFumeShroom 设计（2026-07-15）

自创变种（原版无），大喷菇的寒冰版。主人规格：整体蓝色覆盖（僵尸减速同色）、
射程短 30、蓝色孢子云、每喷 10 伤害 + 2.5s 减速；阳光/冷却/攻速由 Claude 定。

## 数值（Claude 定）

| 项 | 值 | 依据 |
|---|---|---|
| 阳光 | 125 | 大喷菇 100；伤害减半(10)但附带减速，+25 |
| 冷却 | 7.5s | 常规卡 |
| 攻速 | 1.5s/喷 | 与大喷菇一致（继承不改） |
| 射程 | 360px | 大喷菇 390 − 30 |
| 伤害 | 10，穿透二类护盾 | 走 TakeDamage(penetrateShield=true)，同大喷菇 |
| 减速 | 2.5s | Zombie::SetCooldown(2.5)，重复命中取 max（不叠加，同寒冰射手） |

## 交互语义（沿用现有系统，无新状态）

- 减速入口守卫：`CanBeChilled()`（垂死/魅惑/预览豁免）；**持盾（铁门/报纸）守卫在
  SetCooldown 内部 no-op** —— 穿透伤害照吃、减速不吃，与寒冰射手同语义。
- 魅惑僵尸：FumeAttack 现有过滤整体跳过（伤害+减速都不吃）。
- 蓝色覆盖 = `SetOverlayColor(80,80,255,240)`，与僵尸减速/冻结同色；是**身份视觉**，
  预览/图鉴同样生效（skill 教训 9：视觉挂自己入口，不搭减速状态便车）。

## 实现方式

- `FumeShroom` 常量提升为 protected 成员 `mFumeDamage/mFumeReach` + 两个覆写点
  `FumeParticleName()` / `OnFumeHit(Zombie*)`；帧事件仍是既有第 27 帧（沿用，无新帧号）。
- `IceFumeShroom : FumeShroom` 只改数值 + overlay + 粒子名 + OnFumeHit 挂减速。
- reanim 复用 "FumeShroom"；卡图 `PlantImage/IceFumeShroom.png` = 原图 R,G×0.354、B 不变
  （数学上等价 overlay(80,80,255,a=240) 的两遍 alpha 混合）。
- 粒子 `IceFumeCloud.xml` = FumeCloud + `<ParticleRed/Green>.35</>` `<ParticleBlue>1</>`，
  X 位移场 300/310 → 270/280（射程短 30 同步）。
- 枚举位：`PLANT_ICEFUMESHROOM` 插在 ICESHROOM(13) 与 DOOMSHROOM 之间占 **14**——
  Trophy 用 `static_cast<PlantType>(mLevel)` 解锁，14 号位 = 下一关（黑夜第 5 关）解锁卡，
  正合"先不做毁灭菇"；已实现植物枚举值全部不动，旧存档 havecards 兼容。
- dump_state 僵尸新增 `slowed` bool 投影字段（slowCooldown 浮点不可 assert equals）。

## 验证

smoke_icefumeshroom.json（夜=level 10）：种植→喷射命中普通僵尸 assert slowed=true →
2.5s 后 assert slowed=false；铁门僵尸 assert slowed=false 且护盾掉血（穿透）；
白天 level 1 assert anim_sleep。截图核对蓝色本体/蓝色孢子云/站位影子。
