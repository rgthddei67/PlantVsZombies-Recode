---
name: project_pvz_bungee_zombie
description: 2026-08-22 蹦极僵尸的原版随机与蒙特卡洛单株移除、跨格植物承载层门禁、抱取绘制、存档和可见 AutoTest
metadata:
  node_type: memory
  type: project
---

# 蹦极僵尸

## 当前实现

- `ZOMBIE_BUNGEE` 已移入 `NUM_ZOMBIE_TYPES` 哨兵前并注册 `BungeeZombie` 工厂，450 本体生命；下降期为空中层，不吃普通弹丸、灰烬、土豆雷、缠绕水草、冻结、魅惑、台风位移或进家判定，落地等待与抓取阶段才允许正常承伤。Bungee Blitz 固定五只玩法仍未实现；普通蹦极已进入既有 5-4，并在新增 5-6 与扶梯、投篮车综合。
- 状态机为 `DIVING → AT_BOTTOM → GRABBING → RISING`。下降初版 240 px/s 被主人实机判定过慢，最终提高到 480 px/s；落地等待 5 游戏秒。抓取使用 `PlayTrackOnce(anim_grab → anim_hold)`，上提使用 `PlayTrackOnce(anim_raise)`，没有新增动画帧事件。主人视觉复核后，上升提高到 1600 px/s；最终携带植物偏移定为 `(-20,-12)`，其中 X 从可见测试的 `-12` 再向左调 8px，按主人要求只完成 clang-release 编译、留给主人实机看最终效果。
- 落地节点会先按目标逻辑格查询空中防御保护者；叶子保护伞覆盖时播放 `boing`、触发 `anim_block`，清空 `mTargetPlantID` 并直接进入 `RISING`，从而不进入等待/抓取且不会在离场 `Die()` 误删目标。保护范围外的原落地流程不变。
- 植物侧保存抓取者 ID、抓取/上升状态与视觉偏移。被盯上后暂停行为、免疫伤害、不可啃食并关闭碰撞/影子；上升时跳过常规植物绘制，由蹦极按“后层身体 → 植物 → 前臂专用 Animator”顺序绘制。抓取未离地时击杀蹦极会释放植物，上升后击杀或离场会结算植物移除；双方关系、阶段、高度、计时与动画状态支持关卡快照往返。

## 选点与资源

- `GameAPP::mEnableMonteCarloAI=false` 时复刻原版网格加权随机：未被别只蹦极预订的有植物格权重 10000、空格权重 1，并保留最后一株未被预订向日葵；同 Seed 42 的专项锁定普通豌豆格。开关为 true 时，`Board::PickMonteCarloPlantRemovalTarget` 使用最多 48 rollout、16 秒时域和最多 16 只当前僵尸，为每个候选格按 normal → pumpkin → under 选实际会带走的一株，用 `PlantDefenseMonteCarlo::Candidate::targetPlantId` 在推演起点精确移除该实体，选择对僵尸方未来收益最大的目标；失败回退原版随机。蹦极单次选点以 384 个“候选×样本”总评估量封顶，候选少时仍保留 48 rollout。并列最高分使用局部 seeded RNG，不消费正式 `GameRandom`。
- 2026-08-22 起，普通层若存在活动且明确 `CanBeTargetedByBungee()==false` 的植物，该实体会遮住同格下方承载层，蹦极不得越过双格玉米加农炮去抱走任一花盆；这只改变蹦极选层，不改变巨人逐层砸击。
- C# 参考和年度版素材库补齐 `ZombieBungi.reanim` 已有注册所需资源：`BungeeCord.png`、`BungeeTarget.png`、`grassstep.ogg` 与三条 `bungee_scream*.ogg`。权威注册位于 `build/clang-release/resources/resources.xml`，资源键位于 `ResourceKeys.h`；运行专项逐项断言 reanim、贴图和音效可加载。
- gamedata 当前为 `weight=1800`、`appearWave=10`、`survivalRound=15`、`offset=[-46,-92]`、`scale=1.0`。主人屋顶实机图指出本体略偏左与绳索断口后，整身向右调 6px，绳索末端由视觉原点 `-38` 延到 `+24`，多余绳段在本体后方遮住。同格组合植物由 normal 层优先，其次 pumpkin、最后 under；各蹦极之间按目标格和实体 ID 排他预订。

## 验证

- `clang-release` 配置/构建成功。主人当前桌面可见运行 `smoke_bungee_zombie.json`，窗口标题“植物大战僵尸中文版”、exit 0、`run.log` 为 `script finished OK`；覆盖资源、蒙特卡洛与原版随机分支、空中/落地承伤、尖叫、抓取禁用、快照恢复、快速上升、离场移除和抓取中死亡释放。该次携带截图使用 `cargoX=-12`，主人随后认为仍略偏右，最终 `cargoX=-20` 只重新完成 clang-release 编译，未按主人要求再跑 AutoTest。
- `smoke_bungee_roof_visual.json` 在主人当前桌面可见运行 exit 0，屋顶截图确认绳索从顶部连续伸入僵尸背后，整身与目标花盆中心对齐；该验证不依赖之后只改携带植物的 `cargoX=-20`。
- 共享蒙特卡洛回归 `smoke_elite_jack_monte_carlo_targeting.json` 同样在当前桌面可见运行 exit 0，原有爆炸候选的 32 rollout、候选数、目标行/X 与植物存活断言保持通过。
- 2026-08-14 共享 rollout 硬上限从 12 提高到 16；`smoke_zombie_monte_carlo_cap` 以 15 只普通僵尸加蹦极在当前桌面可见断言样本数 16，`smoke_bungee_zombie` 父回归继续 exit 0。
- 2026-08-14 植物选点预算与急救员选疗拆分后，蹦极改为 64 rollout，16 秒时域和 16 只详细样本上限不变；`clang-release`、378 项 Win7 导入审计与三项 CTest 通过。本次数值调整按主人要求未运行 AutoTest，脚本预期已同步到 64。
- 2026-08-08 接入叶子保护伞后，`smoke_bungee_zombie.json` 104 条命令再次在主人当前桌面可见运行 exit 0、`script finished OK`，携带坚果与随机/蒙特卡洛选点截图保持正常；`smoke_umbrella_leaf.json` 另断言保护区内蹦极空手上升、目标 300 生命保留、快照往返不重播伞声或 `boing`。
- 2026-08-22 `clang-release`、LTO 与 378 项 Win7 导入审计通过；当前桌面可见 `smoke_bungee_zombie.json` 124 条命令 exit 0。6-8 初始 15 个花盆仍为合法候选，玉米炮下新增两只花盆不增加候选数且保持活动，截图 `04_bungee_cob_support_blocked.png` 已目验。
- 2026-08-30 卡顿专项：同一固定步生成 5 只蹦极、15 个候选植物和 16 只详细僵尸时，旧实现把 5 次推演叠在一帧，`GOM_Update` 单次峰值 78.60ms。现在 Board 每 3 个固定逻辑步按对象稳定顺序发放一次蹦极推演名额，未领取者停留屏幕外；再以 384 总评估量把 15 候选压到 25 rollout、11 候选压到 34 rollout。相同 `-Profile` 压力夹具最终 `GOM_Update` 峰值 9.83ms、单次 `MC.PlantTarget.Total` 最大 9.77ms，约降低 87.5%。`stress_bungee_monte_carlo`、`smoke_bungee_zombie`、`smoke_monte_carlo_support_compression` 可见通过，CTest 3/3 通过；精英小丑和冰像处刑者未接该蹦极专属预算。

## 可复用契约

- 单株移除能力不能复用半径爆炸近似：候选必须携带目标实体 ID，数值模拟只删除那一株；关闭蒙特卡洛开关时必须回到品种自己的原版选择算法。
- 分层选取不能把“顶层明确拒绝此能力”理解成“继续向下找下一层”；跨格重型植物的拒选能力同时遮蔽其 footprint 下的支撑层。
- 跨对象“身体后层/植物/前臂”夹层必须由持有者在一次 Draw 中显式提交；对象 draw order 无法表达同一僵尸内部的夹层。读档后需同时重建显式 Shadow 附件和 reanim 自带地面轨显隐，避免空中残留黑影。
