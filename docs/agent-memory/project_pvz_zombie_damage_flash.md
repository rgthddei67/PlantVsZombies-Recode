---
name: project_pvz_zombie_damage_flash
description: 僵尸本体/头盔/飞行额外生命与二类护盾按实际承伤层独立闪白，复刻原版双计时器语义
metadata:
  node_type: memory
  type: project
---

# 僵尸分层受击闪烁

## 当前契约

- `mJustGotShotCounter` 负责本体、头盔和飞行额外生命层，`mShieldJustGotShotCounter` 只负责报纸、铁门、梯子等二类护盾。普通正面子弹只伤盾时只闪盾；从背后追上的子弹完全绕盾并只闪后层；大喷穿透或破盾溢出确实伤到后层时才同时闪。
- `Zombie::TakeDamage` 不能在分层前统一开整身白光；必须围绕虚调用比较扣血前后值。这样 `DoorZombie::TakeBodyDamage` 等覆写仍能被准确观测，0 伤害、免伤和未实际承伤的层不会误闪。
- 本体层继续复用 `AnimatedObject::mGlowingTimer`；二类护盾用独立计时器。`Animator::SetTrackGlowOverride` 让盾轨不继承整体高亮：铁门/加固门为 `anim_screendoor`，报纸为 `Zombie_paper_paper`，梯子预留 `Zombie_ladder_1`。门前手臂和拿报纸的手仍属于本体层。
- `TrackExtraInfo` 的高亮覆盖同时由实例化快路径与 `-NoInstance` 慢路径消费；覆盖只改变 additive 高亮，不影响冻结/减速 overlay、轨道可见性、附件顺序或世界变换。
- 加固铁门持门时仍由 `BlocksFumePiercing` 把大喷改为只伤门，因此只闪盾；掉门后大喷只伤本体，因此只闪本体。
- 特殊弹丸可经 `TakeProjectileDamage` 显式请求完全绕过二类护盾；目标用
  `BlocksProjectileShieldBypass` 声明不可绕过状态。卷心菜命中普通铁门/报纸时应只伤并
  闪本体，加固铁门持门时否决请求并只伤盾；这与 `penetrateShield` 的盾、本体同时承伤不同。
- 受击白光是短暂表现状态，不进入关卡存档；读档后的 `SetupZombie` 会重新把仍存在的二类护盾轨道配置为独立且默认不亮。

## 验证抓手

- AutoTest 僵尸状态导出 `bodyHitFlashing`、`shieldHitFlashing`、`bodyTrackGlowing`、`shieldTrackGlowing`，并用 `hitFlashMask` / `renderedHitGlowMask` 的 bit0=本体、bit1=二类护盾做单断言。
- `smoke_zombie_damage_flash.json` 覆盖普通铁门与报纸的 `2→0→3→0`、加固铁门的 `2`、普通僵尸的 `1`，同步截图逐张确认实际像素表现。
- 2026-08-01 `clang-release` 配置/构建零警告；专项默认实例化与 `-NoInstance` 均在主人当前桌面可见运行且退出 0。既有 `smoke_door_fume_death.json` 与 `smoke_reinforced_door.json` 也可见退出 0，`run.log` 无 FAIL、Fatal、WATCHDOG 或资源缺失。
- 2026-08-03 卷心菜绕盾扩展完成 `clang-release` LTO 构建（exit 0）；对应受击层断言已写入
  `smoke_cabbagepult.json`，但按主人要求本次未运行 AutoTest。
