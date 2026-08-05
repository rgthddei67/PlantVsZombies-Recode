---
name: project_pvz_tallnut
description: 2026-07-28 经典高坚果、普通与精英撑杆/海豚跳跃阻拦、坚果家族啃食碎屑和裂纹存档恢复
metadata:
  node_type: memory
  type: project
---

# 经典高坚果

## 当前实现

`PLANT_TALLNUT` 已注册为 `TallNut : WallNut`，使用主人导入的 `Tallnut.reanim` 与卡图。基础生命
9000，阳光 125，冷却 30 秒；普通坚果保持 4000 生命。两者共用按生命值派生的三阶段材质：
高于 2/3 为完整体，不高于 2/3 与 1/3 时分别切换 `cracked1/2`。首次跨入裂纹阶段喷出
`WallnutEatLarge`；快照读回仅重建材质和阶段，不重放碎屑。

高坚果卡图在 `CardDisplayComponent` 的通用 `0.64` 绘制倍率之上再乘 `0.70`，保持既有卡图
矩形中心后再上移 5px，避开底部阳光文字；该倍率只影响卡槽和选卡界面，不改变草坪本体。

`image/reanim/` 的目录预加载键为 `IMAGE_<文件名大写>`；只有被 reanim XML 直接引用的图片才会
额外以 `IMAGE_REANIM_*` 键加载。`Tallnut_cracked1/2.png` 未被 `Tallnut.reanim` 正文引用，
动态换图必须使用 `IMAGE_TALLNUT_CRACKED1/2`。`WallNut` 的裂纹图同理。

僵尸每次真正执行植物啃食伤害前调用 `Plant::OnZombieBite`。普通植物默认无反馈，`WallNut`
及高坚果喷出 `WallnutEatSmall`，因此不需要在僵尸侧维护植物类型表。碎屑条在
`resources.xml` 以 `Column=5` 拆为五张静态纹理并随机取样，不能作为 `ImageFrames` 循环动画。

高坚果图鉴现已说明台风能力：强台风与超强台风中锚定阵型，每直接挡住一个植物格移动一格
便承受 800 点伤害。图鉴正文只修改权威 `build/clang-release/resources/info.txt`，
`clang-playtest` 与 `msvc-debug` 继续通过资源目录联接共享。

## 跳跃阻拦

高坚果通过 `BlocksZombieJump` 阻挡 `POLEVAULT` 与 `DOLPHIN_RIDER`，每次确实阻拦时由植物侧
统一播放 `SOUND_BONK` 并喷出 `TallNutBlock` 星星。

- 普通与精英撑杆在接触植物时都先锁定当前格顶层目标并播放 `anim_jump`，到 C# 原版的 60%
  动画进度节点才检查一次高坚果。阻拦前保持 `JUMPING/anim_jump`，不得提前 Bonk、喷星星或扣血；
  阻拦后撤回精英已逐帧补过的额外距离，弃杆进入 `WALKING` 并开始啃食，实际跳距保持 0。
- 起跳目标 ID 与是否已检查进入快照；读档恢复原 `anim_jump` 帧并继续到 60% 节点，不能像旧实现那样
  直接 `EndJump()` 绕过高坚果。起跳仍解析当前格战斗顶层，阻拦检查则独立按格内层级查询能力。
- 2026-08-05 起阻拦查询与啃食顶层分离：南瓜包住高坚果时继续向内找到高坚果能力；撑杆按起跳碰撞框
  到完整落地碰撞框的扫掠区选择移动方向上最先遇到的高坚果，因此精英越过前方普通植物时也不会再
  穿过后方高坚果。受阻后先落到实际高坚果迎敌面，随后普通啃食仍会按战斗顶层改吃南瓜。
- 精英撑杆自身保持 450 生命；被挡时先在同排同 X 召唤普通撑杆，再对高坚果调用
  `TakeDamage(500, DamageSource::ZOMBIE)`。普通关高坚果从 9000 降至 8500，生存模式仍统一消费
  僵尸增伤与植物韧性词条。
- 海豚在跳跃进度 30% 的既有唯一判定点被挡，弃豚回到 `SWIMMING`；Bonk 从原来的僵尸内部
  分支收敛到植物反馈入口，避免两处重复播放。
- 精英海豚在同一30%节点被挡后也立即弃豚并开始啃食，再由品种钩子对当前格顶层高坚果结算
  `TakeDamage(500, DamageSource::ZOMBIE)`；精英自身700生命不变。水池同格断言通过
  `topPlantsByCell.<row_col>` 读取顶层植物，避免 `plants` 数组受睡莲实体顺序影响。

## 台风锚定

高坚果通过 `Plant::AnchorsPlantCellAgainstTyphoon` 声明双向锚定当前植物格，
`Board::TriggerTyphoonPlantMove` 仍是唯一阵风结算点，不写死植物类型：

- 高坚果自身不换格、不掉出棋盘或落入弹坑；独自抵抗自身位移不扣血。若位于睡莲上，
  上下层作为一个格位保持不拆分。
- 只有相邻植物格直接尝试进入高坚果格时才调用 `OnTyphoonPlantImpact`；每挡下一格立即走
  `TakeDamage(800, DamageSource::OTHER)`。后方植物被普通占格挡住时不把压力传给高坚果。
- 超强台风逐格调用两次，因此紧邻植物造成 1600 基础伤害；相隔一格则先移动再撞击一次，
  只造成 800。来袭睡莲与上层植物按一个格位结算，不按实体层数翻倍。
- 伤害逐格即时生效，但同一阵风用植物 ID 合并 Bonk 与 `TallNutBlock` 星光反馈。若第一步
  撞击杀死高坚果，该步仍被挡，第二步重新读取已经腾出的格位并允许植物进入。
- `weather.lastGustBlockedPlantSteps` 暴露最近一次阵风的直接阻挡格次；它是测试观测值，
  与移动/损失计数一样不进存档。能力本身由植物类型、生命值和格子状态派生，也无需新存档字段。

典型布局在风向右的超强台风中按两步结算：
`向日葵 / 空格 / 豌豆射手 / 高坚果`
变为 `空格 / 向日葵 / 豌豆射手 / 高坚果`。豌豆两次直接撞击使高坚果损失1600，
向日葵第二步只被豌豆占格挡住，不额外传压。

## 验证

2026-07-28 `clang-playtest` 构建成功且无警告。以下脚本均从 `build/clang-playtest/` 在主人
当前桌面的“植物大战僵尸中文版”可见窗口运行，退出码 0，`run.log` 以
`script finished OK` 结束：

- `smoke_tallnut.json`：9000 生命、两档裂纹与大碎屑、裂纹快照恢复不重放、普通撑杆阻拦前
  `JUMPING/anim_jump` 且 Bonk 为 0、跳跃快照继续、阻拦后零跳距、Bonk/星星和啃食小碎屑。
- `smoke_tallnut_elite_pole.json`：阻拦前精英为 `JUMPING/anim_jump` 且双方未受伤；阻拦后
  精英仍为 450 生命并啃食，高坚果为 8500，普通撑杆仍存在。
- `smoke_tallnut_dolphin.json`：海豚由 `JUMPING` 被挡回 `SWIMMING`，弃豚，Bonk 与星星各一次。
- `smoke_tallnut_elite_dolphin.json`：阻拦前顶层高坚果为9000；30%节点后精英海豚弃豚并啃食，
  高坚果精确8500，精英仍为700，Bonk与星星各一次。
- `smoke_wallnut_chew_particles.json`：普通坚果受一次 50 伤，啃食者计数 1，小碎屑有实际
  render quad 且与坚果/僵尸碰撞区相交。
- `smoke_tallnut_typhoon_anchor.json`：普通台风 no-op、强台风双向直接阻挡、超强紧邻两格
  1600 伤害但单次音画、相隔一格先移后挡、向日葵空格链不传压、第一步死亡后第二步放行，
  以及水路上下层作为单个来袭格结算。
- `smoke_typhoon_eating_target.json`：普通碰撞啃食者被阵风吹离仍走碰撞退出；水中海豚被高坚果
  挡下后即使是碰撞箱外 5px 的手动啃食关系，超强台风吹离也会清掉目标 ID 与 `eaterCount`。
  同一快照还覆盖目标植物死亡和海豚自身进入死亡轨道，均不会继续啃食。

水路高坚果与荷叶同格时，粒子探针的 `nearestPlant.type` 可能命中下层荷叶；这类场景应断言
稳定的 `row/col`，不能把最近几何对象类型当成触发者身份。
