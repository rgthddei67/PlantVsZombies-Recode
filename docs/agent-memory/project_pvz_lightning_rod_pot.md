---
name: project-pvz-lightning-rod-pot
description: 6-4避雷花盆under层紫卡升级、雷荷和劫持者保护、同排放电增伤及资源验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-15
---

# 第六大关避雷花盆

## 最终契约

- `PLANT_LIGHTNINGRODPOT` 是 6-4（内部 49）奖励：150 阳光、50 秒冷却、700 生命。
- 紫卡必须覆盖普通花盆并原位替换 `under` 层；同格普通植物、南瓜和咖啡覆盖层全部保留。
  `PlantUpgradeRules` 用 `PlantUpgradeLayer::NORMAL/UNDER` 集中表达替换层，创建、读档创建、屋顶门禁、
  上层 5px 抬升和径流承载判断都消费 `IsUnderPlantLayerType/IsRoofSupportPlant`，禁止继续按具体类型散落分支。
- 有效本体且其格子正在承载目标普通层实体或南瓜时，永久保护该宿主免受普通雷荷离散停机与劫持者处决。
  单格宿主仍是同格保护；玉米加农炮等多格宿主只需任一 footprint 占格下有有效避雷花盆，就保护同一实体
  的完整 footprint。不消耗生命，不清除连续径流暂停，不减少电荷、不取消劫持者、不保护僵尸；空盆不生效。
- 锁定行有至少一个正在承载植物的有效避雷花盆时，本次普通雷击对合格地面僵尸伤害翻倍：
  干坡 `75→150`，湿坡 `120→240`；麻痹仍为 `0.75/1.2` 秒，多盆取最大值而不连乘。
  绝缘僵尸继续走自身承接/过载语义，不新增品种免疫。
- 本体仍承受外部普通伤害；被摧毁只释放 under 层，上层植物按现有分层生命周期保留。

## 性能、存档与表现

- 保护只在劫持者处决快照和 `WARNING→DISCHARGING` 边沿查询；单格植物为同格 O(1)，多格植物按
  固定上限 footprint 扫描占格。同排增伤只在放电边沿扫描固定列数并取最大倍率，没有新增逐帧全场遍历。
- 不新增品种额外玩法字段；类型、生命、Animator 待返轨和继承花盆咬伤保护由既有存档恢复。
  “当前正在保护”完全由 Cell 派生，不入档；恢复 `DISCHARGING` 不重复结算。
- 轻量蒙特卡洛中保持完整详细植物画像，不使用普通承载层的 `simulation.supportOnly` 压缩；因此其
  700 生命、战略价值和特殊 under 层阻挡会继续占详细植物预算并参与推演。
- imagegen 源图为深蓝陶盆、铜箍、陶瓷绝缘子、右后避雷针和紫电芯；确定性脚本导出低分辨率
  本体/卡图/光层。主人实机反馈后战场图缩至原版约 74%，卡图缩小并上移 6px；本体再按盆口中心
  右移 7px、下移 8px，避免含避雷针外接框造成上层植物偏心。玩法没有动画帧事件。

## 当前验证

- `clang-release` 配置、编译、LTO 链接和 Win7 x64 导入审计通过。
- 可见 `smoke_lightning_rod_pot.json` 默认与 `-NoInstance` 各 112 命令 exit 0；覆盖资源/数值、
  under 原位升级、上层和南瓜保留、空盆 no-op、多盆不叠加、干湿坡、外部摧毁、劫持者豁免、
  动画和快照往返。可见 `smoke_lightning_rod_pot_reward.json` 17 命令 exit 0，锁定 6-4 奖励并推进 6-5。
- 既有 `smoke_night_roof_charge_effects`、`smoke_hijacker_execution`、`smoke_grounding_shroom` 回归 exit 0。
  `smoke_flowerpot` 在旧的普通花盆阴影断言处失败：脚本要求 X=-4，当前源码既有常量为 X=2；
  失败早于新升级段，未在本任务擅自改普通花盆视觉。
- 2026-08-15 可见 `smoke_cob_cannon_night_roof.json` 87 命令 exit 0；分别验证左、右任一占格下的
  单侧避雷花盆都能保护整株玉米加农炮，读档后保护关系仍由 Cell 派生，无盆时整株只结算一次停机。
