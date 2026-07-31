---
name: project_pvz_elite_jack_in_the_box_zombie
description: 2026-07-30 精英小丑的单盒跨行投掷、双阵营伤害、紫金资源、每波上限、存档、4-4与可见验证
metadata:
  node_type: memory
  type: project
---

# 精英小丑僵尸

## 当前实现

- `ZOMBIE_ELITE_JACK_IN_THE_BOX` 继承经典小丑时间线，900 HP、65 点啃咬基础伤害、
  固定 0.61 行走速度；不注册第 66 帧开盒事件，因此永不自爆。继续复用第 45 帧
  啃食和第 89 帧死亡，以及普通小丑的共享手摇盒循环声所有权。
- 每次随机等待 6～10 游戏秒后只投 1 个盒子；冻结、无头或死亡动画阻止开始新投掷，
  已离手盒子仍以 0.75 秒、120px 高抛物线飞完。未魅惑时枚举自身与相邻有效行的
  植物占用格。2026-07-31 起默认由 `Board::PickMonteCarloPlantBlastTarget` 采集
  当前实体、实际卡槽/冷却和正式合法格，再由纯数值模块以 32 rollout、12 秒时域、
  0.25 秒步长、最多 12 只当前僵尸比较“不投盒”与各爆点的玩家效用损失。僵尸只按
  真实行/X/移速/分层生命/攻击力模拟，不按品种写状态机；未来植物只来自玩家已选卡，
  每 2 游戏秒尝试一次种植，射手用 gamedata 等效 DPS，产能植物用简化产光率。
  `GameAPP::mEnableMonteCarloAI`
  默认 true 且进入玩家配置存档；关闭或推演失败时回退到原贪心（爆区阳光总价、后排
  1.2 倍、产能植物 +300）。魅惑后仍不做价值判断，从同三行的敌方僵尸中随机选一只。
  没有合法阵营目标时不空投，每 0.5 游戏秒重试。
- 投出时隐藏 `Zombie_jackbox_box`，落地后恢复；爆炸复用 `JackExplode` 和 explosion
  音效，100px 半径内造成 50 基础伤害。未魅惑只伤植物，魅惑只伤非魅惑僵尸；
  阵营在投出瞬间锁定，飞行中再魅惑不会改写已发攻击。
- 倒计时、飞行状态/进度、起终点、目标行和投出阵营进入派生存档；读档与
  `ZombieItemUpdate()` 按飞行状态重建盒子显隐，快照往返不会重复结算。

## 资源与断肢

- 外观为午夜紫礼服、双侧紫袖、金色绑带与紫金盒子。权威资源由
  `scripts/recolor_elite_jack_in_the_box.ps1` 从普通小丑生成并锁定 18 个 SHA-256，
  包括五张手臂贴图、掉落断臂粒子图和独立 `EliteJackBox.reanim`。
- 父类通过虚入口选择断臂后的本体 lower2 材质与飞出粒子；精英使用
  `ZombieEliteJackboxArmOff`，普通小丑仍使用原资源。`ZombieItemUpdate()` 复用同一
  材质入口，避免读档后紫袖变回白袖。

## 出怪与验证

- gamedata：`weight=2500`、`appearWave=10`、`survivalRound=12`。正式波次每波最多
  2 只，计数入档，新波与生存轮清时归零。
- 冒险 4-4（内部 31）保留主人现有 30 波综合池，在末位追加精英小丑：
  普通、路障、铁桶、读报、舞王、鎏金冰车、气球、精英小丑。游玩 4-4 时图鉴隐藏，
  通关后成为第 21 个条目。
- `smoke_elite_jack_in_the_box` 覆盖数值、无目标不空投、后排 150×1.2 战胜前排
  175 阳光的贪心权重、单盒飞行、50 伤双阵营规则、魅惑随机敌方目标、飞行中快照、
  盒子轨道、紫袖断臂粒子、掉头与死亡；`smoke_elite_jack_economy_targeting` 覆盖
  50 阳光向日葵凭 300 经济分优先于 275 阳光三线射手；`smoke_elite_jack_row_bounds`
  覆盖六行地图顶/底行只向合法相邻行投掷；`smoke_elite_jack_in_the_box_wave_cap`
  覆盖每波 2 只上限、快照与新波重置。
- `smoke_elite_jack_monte_carlo_targeting` 锁定默认开关、32 rollout、当前候选/
  僵尸/实际卡槽数、植物画像加载和经济植物落点；旧经济专项显式关闭开关并断言
  `GREEDY`，保证低配路径没有被蒙特卡洛替换。
- `smoke_fog_spawnlists_4_1_to_4_4`、`smoke_elite_jack_almanac`、普通小丑行为和
  循环声回归均在主人当前桌面可见运行 exit 0；默认实例化与 `-NoInstance` 截图均确认
  紫色双臂和紫金盒子正常。
