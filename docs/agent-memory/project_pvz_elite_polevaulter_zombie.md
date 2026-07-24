---
name: project_pvz_elite_polevaulter_zombie
description: 2026-07-24 绿色精英撑杆僵尸；450 HP、1.1 动画倍率、250px 跳距、落地生成普通撑杆，并接入冒险 3-3/3-4
metadata:
  node_type: memory
  type: project
---

# 绿色精英撑杆僵尸

## 当前行为（2026-07-24）

- `ElitePolevaulterZombie` 继承 `Polevaulter`，复用普通撑杆帧事件与状态机；基础血量 450，
  `GetAbilityAnimSpeedMultiplier()` 返回 1.1，因此会与冻结、减速、雨势速度层正确组合。
- `Polevaulter.reanim` 的 `anim_jump` 自带约 150px 水平视觉位移。普通撑杆不再额外移动 Transform；
  精英把超出这 150px 的额外 100px 按 `anim_jump` 的实际帧进度逐帧补到 Transform，动画提速、减速或暂停时
  位移会自然同步。`EndJump()` 只结算动画内置 150px 与未消费尾差，不再在落地端点瞬移。
- `Polevaulter::EndJump()` 通过虚函数取得逻辑跳距：普通为 150px，精英为 250px。位移、碰撞与阴影
  恢复后调用落地钩子；精英用最终 Transform 的 X 在同排创建一名独立的普通撑杆。
- `Zombie::UpdatePoolState()` 会在跨越泳池边界时调用走路动画权威入口。精英跳跃期间真实 Transform
  会逐帧前进，可能在 `JUMPING` 中跨界；`Polevaulter::PlayWalkAnimation()` 因而必须在该状态早退，
  只更新入水视觉而不让 `anim_walk` 抢占承载第 92 帧落地事件的 `anim_jump`。
- 精英只在一次真实落地时召唤。跳跃中读档继续沿父类路径完成落地并召唤；落地后两只僵尸各自正常存档。
  跳跃存档同时记录已应用的额外距离，读档结算不会重复移动。
  持杆/跳跃阶段沿用普通撑杆的不可魅惑契约，因此不会出现魅惑精英生成敌对单位的中间态。
- 正式波次经 `Board::ResolveWaveZombieType` 计数，每波最多生成 2 只精英撑杆；第三只起返回
  `NUM_ZOMBIE_TYPES`，由挑选循环继续抽取且不扣预算。计数随存档恢复，只在新波或生存轮清时归零；
  开发者模式和 AutoTest 的 `spawn_zombie` 直造不占配额。

## 资源与注册

- `ElitePolevaulter.reanim` 保持普通撑杆完整时间线，只把实际含红、蓝运动服材质的 13 张图替换为绿色版本；
  肤色、白色、黄色、黑色与 Alpha/明暗保持不变。绿色掉头贴图和 `ElitePolevaulterHeadOff` 粒子独立注册。
- `scripts/recolor_elite_polevaulter.ps1` 使用 `System.Drawing` 生成 15 个输出并逐文件锁定 SHA-256。
  Windows PowerShell 5 会把无 BOM UTF-8 脚本中的中文误解为本地代码页，脚本注释保持 ASCII 可避免解析失败。
- 僵尸枚举追加在已实现类型末尾、`NUM_ZOMBIE_TYPES` 之前；动画枚举追加在 `AnimationType` 末尾，避免移动旧值。
  `gamedata.json` 为 `weight=2300 / appearWave=5 / survivalRound=5 / offset=[-50,-85] / scale=1`。

## 冒险编排

- 3-3：15 波 `{normal, cone, elite polevaulter}`，作为精英独立教学。
- 3-4：20 波 `{normal, cone, polevaulter, bucket, pink football, elite polevaulter}`，
  用粉色橄榄球补高速压力，再与撑杆家族形成复合威胁。

## 验证

2026-07-24 `clang-playtest` 构建成功，并在主人当前桌面的“植物大战僵尸中文版”窗口可见运行：

- `smoke_elite_polevaulter.json`：退出码 0；断言 450/450 HP、动画 110%、实际跳距投影 250000、
  落地后存在普通撑杆；截图确认运动服原红蓝区域统一为绿色。
- `smoke_pool_spawnlists_3_1_to_3_4.json`：退出码 0；逐关断言波数/出怪池，3-4 同时包含粉色橄榄球与精英撑杆。
- `smoke_polevaulter_vault_walk.json`：退出码 0；普通撑杆父类回归保持通过。专项状态中的召唤普通撑杆
  记录跳距投影 150000，精英记录 250000。
- `smoke_elite_polevaulter_wave_cap.json` 已补回归脚本：断言同波仅前 2 只精英撑杆生成、第三个候选被跳过，
  普通候选不受影响且新波归零后可再次生成；主人本次明确要求不运行 AutoTest，因此仅完成 JSON 解析检查。

此前三份 `run.log` 均以 `script finished OK` 结束且无 `ERROR/FAIL/WATCHDOG`；证据位于
`build/clang-playtest/autotest/out/<脚本名>/`。

当前调参及泳池边界修复后重新完成 `clang-playtest` 构建，并可见运行专项：

- 退出码 0，窗口标题为“植物大战僵尸中文版”。
- `smoke_elite_polevaulter.json` 按当前 450 HP、1.1 动画层、250px 跳距重新同步断言；中段保持
  `JUMPING/anim_jump`，额外距离投影 8433，落地后为 100000，总跳距投影 250000，普通撑杆正常生成；
  四张截图已逐张检查，死亡完成后仅保留召唤的普通撑杆，日志无 `ERROR/FAIL/WATCHDOG`。
- `smoke_pool_elite_polevaulter_lilypad_edge.json` 固定 3-1 的 `row=2,col=8` 仅放睡莲，从 x=1000
  让精英先起跳再跨入水界。修复前稳定得到 `JUMPING + anim_walk + hasVaulted=false`，额外 100px
  已走完但落地事件永不到；修复后中段保持 `anim_jump`，最终 `WALKING`、250000/100000 两项距离
  投影正确并生成普通撑杆，退出码 0。
- 既有 `smoke_pool_polevaulter_stacked_plant.json` 同步可见回归退出码 0，普通撑杆仍为 150000，
  证明这次早退只保护一次性跳跃轨道，没有改变已验收的组合植物行为。
