---
name: project-pvz-jalapeno
description: "2026-07-26 火爆辣椒：主人指定本体第19帧引爆、火焰第12帧消失、整行灰烬并把冰道缩短到0.2秒"
metadata:
  node_type: memory
  type: project
---

# 火爆辣椒（Jalapeno）

2026-07-26 新增 `PLANT_JALAPENO`：125 阳光、50 秒冷却，使用主人放入权威资源目录的
`Jalapeno.reanim`、`Fire.reanim`、卡图、拆件贴图和原版音效。当前辣椒素材是主人故意保留的
0..19 帧裁剪版，不得用外部 25 帧素材恢复。

## C# 行为与动画契约

实现参考 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`：

- 非预览实体直接播放 `anim_explode`；主人提供的帧号已是代码口径，在全局第 19 帧点燃，
  充能期间免疫啃食伤害，只显示受击闪光。
- 点燃时播放 `reverse_explosion`、`jalapeno` 和 `juicy`，触发 `(3, -4)` 屏幕抖动，
  并在本行对所有非魅惑僵尸先清除冻结/减速，再走 `TakePlantAshDamage(1800)`。
- 火焰是 `Fire.reanim` 驱动的 12 个 `AnimatedObject`，从
  `CELL_INITALIZE_POS_X` 起在 750px 内等距横铺；每段随机 0.7..1.3 倍速、
  0.9..1.1 缩放和水平翻面，并按主人指定在全局第 12 帧回收。
- 水路僵尸沿用统一灰烬入口，因此切入 `anim_waterdeath`，不会生成陆地烧焦残影；下层睡莲保留。
- 冰车系统合入后，`Jalapeno::IgniteRow()` 会调用 `Board::ShortenIceTrail(mRow, 0.2f)`，
  把同行现有冰道的剩余寿命压到 0.2 秒；没有冰道时保持无副作用。

`Zombie::RemoveColdEffects()` 是本次补充的公共入口，同时清空减速和冻结计时，
重新计算动画速度并撤掉非魅惑僵尸的寒冷 overlay。

## 验证证据

`clang-playtest` 与启用 LTO 的 `clang-release` 均编译通过。专项
`autotest/scripts/smoke_jalapeno.json` 在主人当前桌面可见运行，窗口标题为
“植物大战僵尸中文版”、退出码 0，覆盖：

- 第 19 帧点燃、整行 12 段火焰及第 12 帧全部回收。
- 同行普通僵尸化灰，异行僵尸不受影响。
- 冻结加固铁门先解除寒冷状态，再按其灰烬伤害上限扣除护盾。
- 泳池僵尸进入 `anim_waterdeath`、不生成烧焦残影，且睡莲仍占下层。
- 冰车专项 `smoke_zamboni.json` 另行断言辣椒点燃后同行冰道寿命不超过 200ms，
  随后冰道消失且该格恢复可种植。

全部最终截图已人工检查；主人明确确认从 `CELL_INITALIZE_POS_X` 开始的火焰位置正确。
