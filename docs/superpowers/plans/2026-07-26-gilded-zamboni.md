# 鎏金冰车僵尸实现计划

**目标：** 实现金黄色高级冰车、三路黄色冰道、速度场叠层、无伤阶梯加速、特殊植物克制和冒险 3-6 编排。

**技术路线：** 派生普通冰车并虚拟化铺冰/车速/碾压行范围；Board 独立持有黄色冰道环境状态；通用僵尸速度层按活车来源计层；独立换色资源与可观测 AutoTest 完成验收。

**设计依据：** `docs/superpowers/specs/2026-07-26-gilded-zamboni-design.md`

## 任务 1：注册与黄色资源

- [x] 追加僵尸与动画枚举、ResourceKey、工厂注册、开发面板和 AutoTest 名表。
- [x] 新增可复现换色脚本，生成金色车体、金色衣帽、独立 reanim 和黄色冰道贴图。
- [x] 补齐 `gamedata.json` 五字段并验证资源清单加载。

## 任务 2：本体行为

- [x] 从普通冰车抽出虚拟铺冰、基础车速、损坏贴图前缀和碾压行范围。
- [x] 实现 2200 HP、普通车速 `0.72×` 与 6/10/14 秒 `x2/x4/x8` 无伤阶梯。
- [x] 任意本体受伤重置加速；地刺每次 100、大嘴花每次 50 且均不执行普通秒杀语义。
- [x] 保存无伤时间与本车独立黄色冰道来源左缘。

## 任务 3：黄色冰道与速度场

- [x] Board 独立保存、更新、渲染和存读档黄色冰道；水路拒绝铺设。
- [x] 相邻三路共用车辆水平攻击带碾压，包含相邻水路植物。
- [x] 黄色材质覆盖普通冰道重叠段，两种冰道共同参与禁种与辣椒融化。
- [x] 通用 Animator、步行风向位移和冰车手动位移统一逐因子放大。
- [x] 活车来源支持多层重叠；鎏金冰车可彼此加速，自身无伤能力最终仍封顶 `x8`。

## 任务 4：波次与冒险 3-6

- [x] 正式候选每波最多生成 1 辆，计数随存档恢复并在下一波/生存轮重置。
- [x] level 24 配置为 20 波 `[普通, 路障, 铁桶, 普通冰车, 鎏金冰车]`。
- [x] 保持普通与鎏金冰车禁水路、禁屋顶。

## 任务 5：AutoTest 与交付

- [x] 增加黄色冰道、速度场层数、无伤阶段倍率和每波计数状态抓手。
- [x] 可见专项脚本覆盖所有本体、环境、叠层、特殊植物和上限行为。
- [x] 可见 3-6 脚本覆盖背景、波数、阵容及预览截图。
- [x] `clang-playtest` 构建通过且专项脚本退出码 0。
- [x] `clang-release` LTO 构建通过。
- [x] 更新项目记忆与技能经验并完成审查；提交与 push 在交付步骤执行。

## 任务 6：试玩反馈修复

- [x] 普通冰车与鎏金冰车按品种选择各自的死亡爆炸粒子。
- [x] 新增鎏金车盖、车轮、滚刷碎片与金黄色爆炸云配置。
- [x] 按主人要求不运行 AutoTest；以构建和粒子 XML/资源键静态校验交付给主人复验。

## 任务 7：速度场增强

- [x] 单层从“相对 1 放大幅度”改为加速倍率乘二、减速倍率除二，中性倍率保持 1。
- [x] 保持能力、寒冰、雨势、台风逐因子组合和多来源逐层叠加。
- [x] 鎏金冰车无伤能力继续最终封顶 `x8`，同步专项脚本精确断言。
- [x] 双 Clang 预设零警告构建；可见 `smoke_gilded_zamboni.json` 退出码 0，日志与关键截图验收通过。

## 最终调参表

| 位置 | 参数 | 值 | 含义 |
|---|---|---:|---|
| `GildedZamboniZombie.cpp` | `kGildedZamboniHealth` | 2200 | 本体血量 |
| `GildedZamboniZombie.cpp` | `kGildedBaseDriveMultiplier` | 0.72 | 相对普通冰车速度曲线 |
| `GildedZamboniZombie.cpp` | 三段无伤阈值 | 6 / 10 / 14 秒 | 最终 `x2/x4/x8` |
| `GildedZamboniZombie.cpp` | `kCaltropHitDamage` | 100 | 地刺每次命中伤害 |
| `GildedZamboniZombie.cpp` | `kChomperBiteDamage` | 50 | 大嘴花每次咬合伤害 |
| `GildedZamboniZombie.cpp` | `kMutualInfluenceLeftPadding` | 80 px | 活车车身互相进入速度场的距离 |
| `Board.cpp` | `kGoldenIceTrailDuration` | 30 秒 | 黄色冰道刷新寿命 |
| `Board.cpp` | `kGildedZamboniMaxPerWave` | 1 | 正式波次生成上限 |
| `Zombie.cpp` | 单层效果放大 | `m>1 ? 2m : m<1 ? m/2 : 1` | 加速倍率乘二、减速倍率除二，中性不变 |
| `Zombie.cpp` | `kMaxGoldenIceEffectStacks` | 8 | 极端调试生成安全上限 |
| `gamedata.json` | `weight / appearWave / survivalRound` | `3200 / 8 / 8` | 抽取权重兼成本、最早波次、生存轮次 |
| `spawnlists.json` | 3-6 | `20 / [普通,路障,铁桶,普通冰车,鎏金冰车]` | 高级冰车综合教学 |
