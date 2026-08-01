---
name: project_pvz_digger_zombie
description: 2026-08-01 普通矿工与爆破工头的共享出土状态机、后排爆破、资源换色、冒险接入、存档与可见验证
metadata:
  node_type: memory
  type: project
---

# 矿工僵尸家族

## 当前实现

- `ZOMBIE_DIGGER` 为 270 本体 + 100 安全帽；地下速度随机 `0.66～0.68 px/tick`，
  持镐完整出土后以 `0.12 px/tick` 向右折返，无镐出土后以普通
  `0.23～0.37 px/tick` 向左推进。出土线按当前棋盘首格起点
  `CELL_INITALIZE_POS_X + 30` 派生，禁止复用 C# 800×600 的绝对 `X=110`。
- `ZOMBIE_ELITE_DIGGER`（爆破工头）复用普通矿工全部地下、出土、丢镐、断肢与
  存档状态机，覆盖为 600 本体 + 250 安全帽。持镐出土后的 3.5 秒 `STUNNED`
  同时作为爆破预警；倒计时结束对房屋侧第 0～2 列、当前行及上下各一行内的每个
  活动植物层造成 150 点 `DamageSource::ZOMBIE` 伤害，然后以 `0.15 px/tick`
  向右折返。边缘行自然裁成 2×3；爆破前死亡或任意阶段丢镐均不爆，预警中丢镐
  立即转为无镐向左推进。
- 七阶段状态机为地下、持镐出土、眩晕、持镐折返、地下丢镐停顿、无镐出土、
  无镐左行。地下/出土期无碰撞、不可被普通弹丸/魅惑/水草选中，也不触发房屋失败；
  只有无镐左行允许进家。`IsMovingRight()` 同时驱动基类位移、台风顺逆风、目标提前量、
  镜像和失败线判定，避免只翻视觉不改规则。
- 出土共 1.3 秒：持镐先 `anim_drill`，最后 0.3 秒 `anim_landing`，随后
  `anim_dizzy` 两轮；无镐地下暂停 2 秒，0.5 秒后显示问号，再走同一高度曲线。
  地下与出土阶段同时隐藏 `ShadowComponent` 和 reanim `_ground` 轨，完整站起后才显示
  组件影子；`ZombieItemUpdate()` 与 Load 均重建该边界。
- 主人给出的全局帧直接注册：啃食 66/81，普通死亡 127；灰烬有镐
  `anim_crumble` 第 36 帧回收，无镐 `anim_crumble_noaxe` 第 73 帧回收。地下、出土与
  无镐停顿阶段遇到灰烬直接回收，不让安全帽截断灰烬伤害。

## 资源、残肢与生命周期

- 权威资源位于 `build/clang-release/resources`，包含矿工本体、出土泥土、问号、
  有镐/无镐灰烬 reanim、三条音效、专属断臂/掉头图和五份粒子配置；其他 preset
  通过 Junction 共享。地下移动以短间隔在当前稳定视觉原点重复发射短寿命尘土，
  避免单个长寿命世界粒子落在移动者身后。
- 安全帽按 2/3、1/3 阈值换 `hardhat2/3`；掉帽、断臂、掉头分别重建专属贴图与粒子，
  读档用 `ZombieItemUpdate()` 复用同一终态。一次性附属 reanim 必须把
  `AnimatedObject` 自身循环类型设为 `PLAY_ONCE`，只调用 `PlayTrackOnce` 不会触发
  基类自动回收；灰烬另由主人给出的帧事件精确销毁。
- `image/reanim/` 中未被 reanim 时间线直接引用的 `hardhat2/3`、`outerarm_upper2`
  运行时换图使用启动扫描生成的 `IMAGE_<UPPERCASE_STEM>`，不能写成
  `IMAGE_REANIM_*`。新 reanim 文件还必须在 `resources.xml` 用 `<Reanimation name>`
  注册；只有文件与 manifest 会让 `AnimatedObject` 得到空 Animator 并在构造期崩溃。
  粒子专用 PNG 同样必须列入 `<ParticleTextures>` 才会产生 `PARTICLE_*` 键。
- 爆破工头的橙色格子衫、安全帽、青色镐头和断臂由
  `scripts/recolor_elite_digger.ps1` 确定性生成并锁 17 份 SHA-256。出土动画使用
  预合成 `rise2～6`，必须逐张换色；只改独立帽子图会在出土时短暂变回白帽。
  独立 `EliteDiggerBlast` 粒子复用小丑爆炸云结构但改为橙红云与青蓝火花，普通小丑
  配置不变。
- 地下挖掘循环声按物种静态引用计数管理；一只矿工离开地下或丢镐不会误停其他实例。
  存档保存阶段、计时、高度、两类随机速度、持镐、问号、落地与尘土计时；读档只重建
  持续状态和循环声，不重播出土/问号等一次性音画。

## 集成与验证

- 已注册普通/精英工厂、动画/资源键、图鉴文本与开发者入口。普通矿工在绝对关卡 32
  （4-5）首次教学；同池在矿工之前加入已学过的加固铁门，作为双向射手解锁后的背击
  绕门复习，不改变矿工末位主题。爆破工头在绝对关卡 33（4-6）加入普通矿工复习池；
  精英 ID 26，`gamedata` 为 weight 2400、appearWave 10、survivalRound 13，每个正式
  波次最多 1 只，波计数进入关卡存档。冒险当前制作到 4-6。
- 可见 `smoke_digger` exit 0，覆盖两种出土、坐标/影子边界、反向移动、两次啃食、
  掉帽/断臂/掉头、两条灰烬回收、正常死亡、共享循环声与多阶段存档往返；`run.log`
  以 `script finished OK` 结束。
- `smoke_digger_visual` 在默认实例化与 `-NoInstance` 两条渲染路径均 exit 0，逐张检查
  地下、问号、出土中段和眩晕截图；出土位置在首格草坪边缘，出土中段无黑色投影，
  完整站起后影子正常出现。
- `smoke_elite_digger` 在 clang-release 默认与 `-NoInstance` 均 exit 0，覆盖出土橙帽、
  3×3 每层 150 伤害、爆破范围几何、爆后向右、预警前后存档防重复、地上/地下丢镐
  取消、受损帽与断臂资源。`smoke_elite_digger_wave_cap`、
  `smoke_elite_digger_almanac`、`smoke_adventure_progression` 均可见 exit 0；普通
  `smoke_digger` 新增三项运行时换图加载断言并回归通过。
- 2026-08-01 可见 `smoke_fog_spawnlists_4_1_to_4_6` exit 0，确认 4-5 有序池为
  普通、路障、精英海豚、精英撑杆、加固铁门、矿工，矿工仍为预览末位主题。
