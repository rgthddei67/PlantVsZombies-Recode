---
name: project-pvz-winter-melon
description: 6-5 冰瓜紫卡升级、100/33 三行伤害、10秒群体减速与独立冰屑资源闭环
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-14
---

# 6-5 冰瓜（Winter Melon）

2026-08-14 完成。冒险内部关卡 50（显示 6-5）奖励 `PLANT_WINTERMELON`；它是
`PLANT_MELONPULT` 的普通层紫卡升级，费用 200、冷却 50 秒、生命 300。升级规则集中在
`PlantUpgradeRules`，沿用 Board 的原子替换：先把 Cell 普通层切到新株，再让旧西瓜失活，
花盆承载层与南瓜层保持不变。

## 行为契约

- `WinterMelon : MelonPult` 只覆写投射物品种；射击计时、存档、同行索敌、35fps
  `anim_shooting`、主人确认的全局第 44 帧发射、1.2 秒解析抛物线与 210px 拱高均复用父类。
- 当前普通西瓜是 120 直击、40 次要溅射；主人确认冰瓜较它少 20，因此
  `BULLET_WINTERMELON` 为 100 直击，实际进入命中行及上下相邻行 60px 水平窗口的目标承受
  `floor(100/3)=33`，仍保留七倍次要总伤害预算。
- 直击与实际进入溅射集合、承伤后仍有效且 `CanBeChilled()` 的目标获得 10 秒减速；重复命中取
  更长剩余时间。冰车等明确免疫减速的目标只承伤，不获减速。
- 伤害沿用西瓜的 `penetrateShield=true`：二类盾与后层同伤。后层既已承伤时，减速显式用
  `bypassShield=true`，避免仍存在的铁门/报纸/梯子再次挡住同次命中的状态。
- 发射请求 Throw/Throw2 与 `snow_pea_sparkles`，命中/落空继续请求 melonimpact Foley。
  飞行物用 `IMAGE_REANIM_WINTERMELON_PROJECTILE`；命中/落空只发
  `WinterMelonSplash`，不得串出绿色 `MelonSplash`。

## 资源与表现

- 主人提供 `WinterMelon.reanim` 与 `WinterMelon_projectile.png`；卡图和全部八张独立 reanim
  部件随任务一并纳入版本控制。reanim 另引用三张 PeaShooter 共享叶片，专项对全部直接键做
  `GetTexture(key,false)` 断言。
- 冰屑使用原资源包的九帧水平条 `WinterMelon_particles.png`，SHA-256 为
  `28BCFB78D253836B19DC864DDEECF1AAFD1A9075787DF1DEA4FE2D3515E2E807`；
  `WinterMelonSplash.xml` 仅按当前引擎改为秒制、效果名和 `PARTICLE_*` 注册。
- 卡图沿用西瓜家族 0.80 倍和 `(+3,+1)` UI 偏移；战场仍由独立 gamedata offset/scale 决定。

## 验证证据

- `clang-release` 完整配置、编译、LTO 链接和 Win7 x64 378 项导入审计通过，最终构建无警告。
- `smoke_winter_melon.json` 共 161 条命令，默认实例化与 `-NoInstance` 都从
  `build/clang-release` 在主人当前桌面可见运行，窗口标题“植物大战僵尸中文版”，两路径
  exit 0、`status.json=passed`、`run.log` 无 FAIL/ERROR/WARN/missing。
- 定量覆盖资源键、紫卡门禁与叠层、帧 44 前后、100/33 三行伤害、三行减速、铁门盾
  `1100→1000` 与本体 `270→170` 同时减速、冰车免疫、在途快照、落空反馈、池槽复位、
  黑夜屋顶组合与 6-5 奖励推进到 51。
- 两条渲染路径的 8 张同步截图均逐张检查；普通 `smoke_melonpult.json` 另回归 120/40、
  铁门盾/本体 `1100→980` 与 `270→150`，防止派生改动串型。

## 复用注意

紫卡升级同帧内旧基础株虽已失活，仍可能短暂留在实体表。按格布置射击周期的 AutoTest 夹具必须
过滤 `IsActive()`，状态断言优先使用 `normalPlantsByCell/topPlantsByCell`。互斥粒子计数需为基础和
派生效果预置显式零键，否则“基础效果为 0”会因 JSON 路径不存在而无法断言。
