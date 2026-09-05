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

## 高坚果阻拦补充（2026-07-28）

撑杆接触植物时不再立即查询阻拦，而是先锁定当前格顶层植物并播放 `anim_jump`。`ZombieUpdate`
在动画进度 60% 的 C# 原版唯一节点查询一次；若是高坚果，则撤回精英已补的额外位移、恢复碰撞和阴影，
弃杆进入 `WALKING`，播放植物侧 Bonk/星星并开始啃食。起跳目标和检查标志入档，跳跃快照恢复后继续
原动画进度，不会直接落地绕过阻拦。

精英的 `OnVaultBlocked(Plant&)` 先在同排同 X 创建普通撑杆，再对阻拦植物调用
`TakeDamage(500, DamageSource::ZOMBIE)`；500 的承受者是高坚果，不是精英本体。普通关高坚果
从 9000 降至 8500，精英保持 450 并进入 `anim_eat`。该伤害走标准僵尸对植物链，生存模式的
僵尸增伤和植物韧性继续统一生效。

`smoke_tallnut_elite_pole.json` 先断言精英处于 `JUMPING/anim_jump`、高坚果仍9000、Bonk为0；
越过60%后断言精英仍450、正在啃食，高坚果为8500、实际跳距和额外补偿均归零，并存在召唤的普通撑杆。

2026-08-05 修复长跳只复查起跳目标 ID 的盲区：父类保存起跳 X，在 60% 节点用完整 250px 跳距构造
僵尸碰撞框扫掠区，沿房屋方向选择最先遇到的声明式阻拦植物；普通植物后方的高坚果会让精英落到其
迎敌面、撤销额外 100px、召唤普通撑杆并对高坚果结算 500 伤害，不再直接越过。

## 普通撑杆临界死亡修复（2026-08-27）

普通撑杆的接触回调此前只检查 `RUNNING && !hasVaulted`，漏掉 C# 原版起跳前的 `mHasHead` 资格。
本体降到 166 后已经掉头，但无头流血仍继续计时，旧实现还能在接触植物时进入 `anim_jump`，随后恰在
落地附近死亡，外观上像“跳完突然消失”。现在碰撞入口与 `StartJump()` 都拒绝掉头、垂死、死亡或失活
实体新提交跳跃；已合法起跳后才受到致命伤的实体不回滚动作，而由 `anim_death` 接管。`EndJump()` 也
只接受仍处于 `JUMPING` 且未进入终止状态的回调，避免迟到的落地事件覆盖死亡轨。没有新增动画帧事件。

`clang-release` 构建与 378 项 Win7 导入审计通过。当前桌面可见
`smoke_polevaulter_death_boundary.json` 默认实例化与 `-NoInstance` 均 exit 0：接触前受 334 伤害后为
`hasHead=false/RUNNING/hasVaulted=false`，健康起跳后受 465 致命伤则保持
`isDying=true/anim_death` 并正常回收；`smoke_polevaulter_vault_walk.json` 父类回归仍为 150000 跳距并
落入 `WALKING`。状态、日志与截图均已检查。

2026-09-05 矿道换行起跳修复：普通与继承该入口的精英撑杆只在实际Y抵达行基线后起跳，不以提前更新的mRow作为完成标志。矿场持续碰撞在到行后重试，避免enter被拒后永久不跳；起跳清理旧矿道节点，落地从新位置规划。没有新增帧事件或修改跳距。
