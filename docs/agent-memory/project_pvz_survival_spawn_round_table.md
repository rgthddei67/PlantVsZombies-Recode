---
name: project_pvz_survival_spawn_round_table
description: "生存模式 BuildSurvivalSpawnList：轮次解锁表+随机子集池+原版调制；2026-07-18 增加最多8种与基础数量随机±1~2，并排除零权重召唤单位"
metadata:
  node_type: memory
  type: project
  originSessionId: 132f8f6c-42be-4d79-aa63-5b98f9f2e55b
---

2026-06-25 完成并 FF 合并 master（提交 2ac099c..d655609，**未 push**）：把生存无尽(1000/1001)的 `Board::BuildSurvivalSpawnList` 从硬编码 `if(round>=N)` 改为**数据驱动轮次表 + 随机子集池**，并复刻原版两条调制。spec/plan 在 `docs/superpowers/{specs,plans}/2026-06-25-survival-spawn-round-table*`。brainstorm→writing-plans→subagent 驱动(6 Task,每 Task 双审 spec+质量)+opus 终审 READY；真实跑游戏二进制逐轮 dump 断言验收。

**轮次表**=`ZombieInfo.survivalRound`(GameDataManager,镜像 `appearWave`,0=不进生存)+`GetZombieSurvivalRound()`；**主人最终调值**：普通1/路障2/撑杆3/铁桶3/读报5/快桶6/加报7(spec 初值 4/3/5/6 已 stale,以代码为准)。

**建池**：round1=`{普通}`、round2=`{普通,路障}` 确定性；round3+ = 普通 + **随机子集**(部分 Fisher-Yates 无放回,`extra=1+(round-3)/2` 每2轮+1种,cap 候选数)；候选=`survivalRound∈[1,round]`(经旗数递减)。逐波生成主干 `PickZombieType`/点数预算/血量增长/**存档全不动**(池每轮由 round 重建,不入存档)。

**核心 foot-gun=`weight` 一物两用**：`GetZombieWeight` 既是 `GetWeightedRandomZombie` 的抽中权重,又是 `PickZombieType`/`TrySummonZombie` 的**点数成本**(原版分 `mPickWeight`/`mZombieValue` 两字段,本仓库合并)。故**杂兵稀释只能改抽中侧**——新增 `Board::GetSurvivalPickWeight`(NORMAL→base/10、CONE→base/4,实际权重1000/1500 故>>0)仅供 `GetWeightedRandomZombie`,成本侧绝不动,否则普通僵尸每波数量异常膨胀。

**调制**=文件静态 `SurvivalCurveLerp`(复刻 `TodCommon.TodAnimateCurve` Linear:钳制线性插值+lround,flags=mSurvivalRound-1)；杂兵稀释复刻 `TodAnimateCurve(10,50,..)`；**旗数递减**(深局提前解锁,`eff=max(survivalRound-reduction,1)`)复刻 `(18,50,0,15)`,当前7僵尸阵容(max survivalRound 7、18旗才起步)下**休眠**,为未来高 survivalRound 僵尸预留(主人选忠实复刻)。

**`NUM_ZOMBIE_TYPES` 移到 `ZOMBIE_FASTPAPER` 后(=7)**：使 `[0,NUM)` 只覆盖7个已实现僵尸,随机抽取永不碰 DOOR..REDEYE(值变8+);已核实全部用法是哨兵/边界,图鉴走 `GetAllZombieTypes()` 不受影响;副作用=`Board::LoadSpawnListFromJson` 上界收紧(良性)。

**AutoTest**：`dump_state` 加 `survivalRound`+`spawnList`(经新 public `Board::GetSpawnZombieList()`,非生存关守 `mIsSurvival?:-1`);新 op `force_survival_round`(设轮+重建池,确定性逐轮验证不必打通整轮)。

**坑**：① AutoTest 脚本实体在**仓库根 `autotest/scripts/`** 不在 `PlantVsZombies/autotest/scripts/`(plan 误写多套一层源码目录,导致 canonical `..\..\autotest\scripts\` 找不到,已 16765a7 修;见 [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md))；② 主人自己编译时 `ninja: no work to do` 是因 obj/exe mtime 已新于源(确属已编译),非未编译。

[adding-survival-perk](../../.agents/skills/adding-survival-perk/SKILL.md) 同族生存系统;升级 Gloom-shroom 等可复用此数据化范式。

**2026-07-18 种类上限与随机波动**：主人指出深轮候选持续增长、已实现僵尸又有限，最终会每轮固定同一整套阵容。保持第1~2轮确定性；第3轮起先按原公式算基础总种类，再夹到 **8种（含普通）**，随后随机取非零 `±1` 或 `±2`，最终夹到 `[2,min(8,候选+普通)]`。因此深轮正波动停在8，负波动落到6~7，不再固定全员出场。

候选新增 `GetZombieWeight(t)>0` 过滤：`ZOMBIE_BACKUP_DANCER` 是舞王召唤单位且 weight=0，过去会占 Fisher-Yates 名额却永远无法被加权抽中，造成“池子计数有它、实际出怪没有它”的假多样性。普通仍必出，无放回抽样、旗数解锁递减、抽中权重调制、点数成本和预算均不动。

AutoTest `dump_state.spawnTypeCount` + `smoke_survival_spawn_round.json` 固定 Seed42：round1/2 为1/2种，round3=4、round6=2、round13=8（上限）、round40=6（深轮负波动），每次 `spawnList[0]` 都是普通僵尸。构建与权限说明同 [project_pvz_perk_system](project_pvz_perk_system.md) 的 2026-07-18 段。
