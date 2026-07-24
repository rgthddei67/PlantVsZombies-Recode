# 精英撑杆僵尸设计 spec

日期：2026-07-24｜状态：已实现并通过可见专项验证

## 目标

新增 `ZOMBIE_ELITE_POLEVAULTER`：把普通撑杆僵尸体表的红色与蓝色材质统一换为绿色，血量 600，动画速度为普通撑杆的 1.2 倍，跳跃距离为普通撑杆的 2 倍；完成跳跃并落地后，在自身落点生成一名普通撑杆僵尸。该精英替代未制作的潜水僵尸，进入冒险 3-3 与 3-4。

## 本体行为

- `ElitePolevaulterZombie` 继承 `Polevaulter`，调用父类 `SetupZombie()`，复用已经验证的跑步、跳跃、落地、啃食、死亡、断肢断头与存读档契约，不注册新帧事件。
- 基础血量由普通撑杆的 500 覆盖为 600。
- 通过 `GetAbilityAnimSpeedMultiplier()` 返回 `1.2`，让现有统一速度层把倍率与减速、冻结、雨势正确组合；普通撑杆的随机基础动画速度与各阶段切轨保持不变。
- 将普通撑杆落地流程抽成“虚拟跳距 + 落地钩子”：普通跳距继续为 150px，精英返回 300px。实际位移完成后才调用落地钩子。
- 精英落地钩子读取自身最终逻辑坐标，并调用 `Board::CreateZombie(ZOMBIE_POLEVAULTER, row, x)` 生成普通撑杆。生成单位与精英同排、同落点，由 Board 重新派生 Y 坐标。
- 撑杆僵尸在持杆和跳跃阶段本来就不可魅惑，因此落地生成发生时精英不可能已经被魅惑；无需绕过普通撑杆的魅惑限制。
- 中途存档仍保存父类 `vaultState/hasVaulted`。若在跳跃中读档，父类既有逻辑会完成跳跃，并经同一个落地钩子补生成普通撑杆；落地后的普通撑杆则作为独立实体正常入档。

## 绿色资源

- 新 reanim `ElitePolevaulter.reanim` 完整复用 `Polevaulter.reanim` 的时间线、剪辑和轨道，只替换确实含红色或蓝色材质的图像键。
- `scripts/recolor_elite_polevaulter.ps1` 使用 `System.Drawing` 按 HSV 选择红色与蓝色材质，映射为高饱和绿色；保留 Alpha、亮度、阴影、高光、白边、皮肤、黄色装饰与黑色描边。
- 换色覆盖球衣、短裤、鞋、袜带、腕带、头带与撑杆红色装饰。无红蓝材质的部件继续共享普通资源。
- 独立生成绿色 `ZombieElitePoleVaulterHead.png`，并复用普通掉头粒子的运动参数创建 `ElitePolevaulterHeadOff`；断臂掉落物不含红蓝材质，继续沿用普通配置。

## 注册与数值

- 新枚举追加在全部已实现僵尸之后、`NUM_ZOMBIE_TYPES` 之前；旧僵尸整数 ID 不移动。
- 新动画枚举追加在 `AnimationType` 末尾，并注册独立 reanim 名 `ElitePolevaulterZombie`。
- `gamedata.json` 使用 `weight=2300`、`appearWave=5`、`survivalRound=5`、`offset=[-50,-85]`、`scale=1.0`。`weight` 同时作为抽取权重与波次成本。

## 冒险 3-1 至 3-4 编排

| 关卡 | 波数 | 首次主题 | 完整池 | 进入时可用关键植物 |
|---|---:|---|---|---|
| 3-1 | 10 | 泳池基础 | 普通、路障 | 睡莲 |
| 3-2 | 15 | 铁桶复习 | 普通、路障、铁桶 | 睡莲、倭瓜 |
| 3-3 | 15 | 精英撑杆独立教学 | 普通、路障、精英撑杆 | 睡莲、倭瓜、三线射手 |
| 3-4 | 20 | 撑杆家族综合与高速压力 | 普通、路障、普通撑杆、铁桶、粉色橄榄球、精英撑杆 | 上述植物、缠绕水草 |

3-3 撤下铁桶和普通撑杆，保证绿色精英及其落地生成单位是清晰主题；3-4 再把普通撑杆、铁桶、粉色橄榄球和精英组合起来，以粉色橄榄球补足高速压力。两关数组都保持“基础怪 → 旧机制 → 本关重点”的预览顺序。

## AutoTest 与验收

- 状态 JSON 为所有撑杆僵尸增加 `vaultState`、`hasVaulted` 与 `lastVaultDistanceOn1000`，避免用不稳定的绝对 X/Y 断言跳距。
- 专项脚本断言：精英血量 600、统一速度层 120%、落地记录距离 300px、落地后存在一名普通撑杆、死亡动画正常消失且无 WATCHDOG。
- 冒险出怪脚本逐关覆盖 3-1 至 3-4 的背景、波数、完整出怪池与预览截图。
- 从 `build/clang-playtest/` 在主人当前桌面可见运行，保留退出码、`run.log`、状态 JSON、断言与截图；最终再做 `clang-release` LTO 发布构建。

2026-07-24 实测 `smoke_elite_polevaulter.json`、`smoke_pool_spawnlists_3_1_to_3_4.json` 与
`smoke_polevaulter_vault_walk.json` 均在可见“植物大战僵尸中文版”窗口中退出码 0，三份日志均
`script finished OK` 且无 `ERROR/FAIL/WATCHDOG`。精英状态证据为 600/600 HP、动画倍率 120%、
跳距整数投影 300000；落地后同时存在精英与普通撑杆，普通撑杆回归记录 150000。
