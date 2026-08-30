---
name: project_pvz_roof_marshal_prototype
description: 2026-08-15 5-9 屋脊督军完整首领；15000 生命、拒绝大嘴花吞食且每口20、6/4秒渐强召唤、10秒突击令、抗黄油连控、改天技能与专用血条索引
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-26
---

# 5-9 屋脊督军首领

2026-08-09 主人确认 5-9 BOSS 采用“僵尸博士麾下得力干将”的指挥型首领方向，并指定角色动画
复用普通僵尸。当前已完成素材、生存层、15 波正式出场、指挥召唤、阶段移动与天气命令；不实现
指挥车。早期“僵尸博士留作未来 6-9”只保留为历史设想；当前第六大关以 6-9 无 BOSS 槽完成定案。

`ZOMBIE_ROOF_MARSHAL` 与 `RoofMarshalZombie` 的独立随机权重保持为 0、`appearWave=15`；
`AdventureProgression::BossSlot::ROOF_MARSHAL` 只在内部关卡 45 的正式第 15 波由
`Board::TrySummonAdventureBoss()` 额外创建一只，固定中路 `x=1000`。开发者面板和 AutoTest 仍可直造，
但普通出怪池、预览和未来新增类型不会自动混入首领路径。
图鉴沿相同固定槽位在 5-9 通关后登记屋脊督军，不把首领伪装成随机池成员；标题与机制说明已登记到
权威 `info.txt`。
`RoofMarshal.reanim` 完整复制 `NormalZombie.reanim` 时间线，派生 `SetupZombie()` 先调用
`Zombie::SetupZombie()`，再把本体当前/最大生命设为 15000、每口伤害从 50 提升到 250；因此继续复用
普通走路、啃食和死亡帧事件，没有新增帧号。

美术源分两层：`docs/art/roof-marshal/roof-marshal-concept-v1.png` 是 ImageGen 生成的整张概念设定稿，
只用于确定深海军蓝短军装、旧金领饰和紧凑大檐帽方向，绝不直接切片进游戏；权威运行素材由
`scripts/generate_roof_marshal_assets.ps1` 从普通僵尸小分体可复现生成。脚本只替换外套、四段袖子、
两只鞋、领饰和 `anim_hair` 对应的 64x31 帽子，保留原画布、Alpha、关节轴以及普通脸/手/腿；普通
`Zombie_outerarm_upper2.png` 断袖也只逐像素把棕布换成军服深蓝，皮肤、骨头、透明区与 17x35 锚点不变。
主人随后确认军帽随头一起飞；脚本现把普通掉头图与军帽预合成为独立的
`ZombieRoofMarshalHead.png`，共 12 个生成结果均锁定 SHA-256。`resources.xml`、manifest、reanim/粒子键与
`GetTexture(key,false)` 断言已闭环。

视觉结论：白天屋顶默认实例化路径与 `-NoInstance` 路径均能稳定显示深蓝军装、金色领饰和帽子，
帽檐不遮眼，袖子在普通走路帧没有露出棕色接缝。`RoofMarshalZombie::HeadDrop()` 在隐藏
`anim_head1/anim_head2/anim_tongue/anim_hair` 前读取头轨世界锚点，只发射一颗
`RoofMarshalHeadOff` 粒子，因此军帽与头在抛起、旋转和下坠期间不会错位；通用 `ZombieHeadOff`
保持为 0。`ArmDrop()` 与 `ZombieItemUpdate()` 都把残肢上臂覆盖为
`IMAGE_ZOMBIE_ROOFMARSHAL_OUTERARM_UPPER2`，实时掉臂和读档恢复不会退回普通棕袖。普通死亡时间线可正常
进入 `anim_death` 并在事件终点回收。

生存契约：普通灰烬基础伤害仍为 1800；`TakePlantAshDamage()` 先把土豆雷传入的 `INT32_MAX` 等高值
压到 1800，再进入基类伤害链，因此生存词条仍能正常缩放。大嘴花不会吞下首领，拒吞后使用统一默认值，
每次完整咬合经正式 `PLANT` 伤害链造成 20。首领不可魅惑；缠绕水草只原地束缚 5 秒后自毁并释放首领，不拖沉、不扣血。
`Zombie::CanBeKilledByMower()` 默认 true，屋脊督军覆写为 false；5-9 屋顶清洁车接触它仍正常启动并
消耗，但从首领身上驶过不造成伤害，普通僵尸对照仍被秒杀。首领拒绝普通烧焦残影，致死灰烬仍走
本体常规死亡时间线与专属军帽掉头粒子。

黄油抗连控：督军覆写正式 `ApplyButter()` 虚入口，首次黄油只定身 1.25 游戏秒，定身期间重复黄油不刷新；
自然解除后开启 5 游戏秒免疫，玉米粒/黄油弹的弹丸伤害仍先照常结算。免疫倒计时属于督军派生存档，
旧档默认 0，损坏档钳到 0～5 秒；定身与免疫同时存在时以定身为准，死亡/掉头清除不产生遗留免疫。

指挥契约：登场 1 秒生成首批，此后生命不低于 5400 时每 6 秒生成 3 只，低于 5400 时每 4 秒生成
4 只；同一批使用不重复随机行并避开首领当前行，全部在 `x=910` 创建。固定白名单显式列出当前枚举
0..35 的全部前五大关已实现类型，共 36 种：普通/快速变体、全部现有精英、泳池形态、伴舞和小鬼均在
其中；红眼巨人、督军自身及未来第六大关以后新增类型不因扩张枚举而自动进入。实际抽取还必须通过
`Board::CanSpawnZombieInRow`，所以泳池专属形态不会被硬塞到 5-9 屋顶。生命不低于 11000 时只抽标准池；
从 10999 降到 5400 的过程中，每个位置抽取高威胁池的概率由 30% 线性升至 100%，低于 5400 后固定
100% 从高威胁池抽取，配合 4 秒 4 只的狂暴节奏不再混入普通杂兵。概率完全由当前生命派生，不新增
存档字段。每次创建后用现有 `anim_idle` 播放 1.2 秒指挥姿势，不新增动画帧事件；若首领
正在 `anim_eat`，派生 `Update()` 只补推进一次首领逻辑，照常召唤/施法但不抢占啃食轨道。

突击令：每完成第 2、4、6……批召唤，选择当前非魅惑、非垂死、非督军兵力最多的一行（并列随机）。
督军先原子切到该逻辑行并用既有跨行走路演出收敛视觉 Y，再吹响 `SOUND_HUGEWAVE`，显示 2.8 游戏秒
“第 N 行全军突击”中央警报，并令该行单位在 10 游戏秒内自主水平推进与每口伤害各乘 1.5。每个目标
同时显示原版 `Zombie_flag1.png`：黄油已占语义头轨 follower，因此红旗使用独立一帧
`RoofMarshalAssaultFlag.reanim` 子 Animator 懒挂到同一已审计语义轨道，位置在头部右后侧，默认实例化与
`-NoInstance` 都留在 Animator 树内。强化计时和两项倍率属于每只 `Zombie` 的受保护存档数据；红旗
由计时派生，快照重载在 `FinalizeProtectedLoad()` 重建，到期/濒死/死亡隐藏。首领保存突击次数、最近
目标行、受影响数与上批召唤时自身行用于诊断，不强化自己。残血阶段不再做 6 秒自主换行，但突击令
仍可按新规则跑向目标行。

移动与耐久阶段：本体不低于 5400 时不水平推进，稳态播放 `anim_idle2`，每 6 秒只换到相邻合法行；
逻辑行和碰撞箱先原子提交，独立视觉 Y 补偿在 0.65 秒内归零。低于 5400 后停止自主换行并按普通
僵尸走路推进。督军体积固定 1.2 倍，`gamedata.json` 偏移为 `[-60,-102]`，影子为 `(1.2,0.9)`；
视觉缩放不改变普通碰撞箱。普通 1/3 掉头临界值对督军完全取消，生命大于 0 始终保留头部，只有
本体生命归零才发射专属军帽连头粒子并进入死亡动画。

天气命令：每完成第 3、6、9……次实际指挥召唤，若 Board 当前为晴天或小雨，调用
`Board::TriggerRoofMarshalWeather(MEDIUM, 20, false)` 强制 20 游戏秒中雨；已有中雨不续期，已有大雨
不降级。生命首次从不低于 5400 跌至 `1..5399` 时，调用同一入口强制至少 30 游戏秒大雨；已有大雨
只在余时不足时补足一次。入口不额外抽取台风，并复用 `BeginRain` 的两秒过渡、粒子、声音、天气板
与 Board 现有存档字段。僵尸只保存其 `commandCount` 等施法节奏，绝不复制雨势或天气倒计时。

首领血条：`GameScene` 在 Board 的 `GAME` 状态下从当前存活、非预览、非垂死的
`ZOMBIE_ROOF_MARSHAL` 实体即时派生表现状态，不在场景或存档复制生命。`EntityRegistry` 为此维护
按实体 ID 有序的专用弱索引，并以最小实体 ID 保证多实例开发测试时结果稳定。最终版血槽居中为 560×18，左上角
`(270,556)`；黑金装甲底板从 Y=529 延伸至 Y=588，主动覆盖可牺牲的关卡文字区域，避免遮住第五路主要作战区。
槽内绘制实际生命 `current / max`，并直接读取督军 getter 在 11000、5400 处画“精锐/狂暴”黑金铭牌和
贯穿标线；低于 5400 时红色填充脉冲。首领致死即隐藏，不等待死亡动画回收；快照恢复后由实体状态
自然重建。全部图形由现有 `Graphics` 基元与字体绘制，不新增位图资源。

验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_visual.json` exit 0、32 条命令全部通过：两只样机、12000 当前/最大生命、五项资源断言、专属/通用粒子
互斥、单粒子/单 quad 与发射原点相对包围盒均通过；3.5 秒后死亡个体回收、仅保留另一只，并同步
保存走路和军帽随头飞出截图。`smoke_roof_marshal_noinstance.json -NoInstance` exit 0、24 条命令全部
通过，同样覆盖专属掉头与死亡回收。`smoke_level_5_9_boss_slot.json` exit 0，
锁定白天屋顶、15 波、三只普通预览、`ROOF_MARSHAL` 槽位及最终波唯一正式首领。

2026-08-15 生存层复核：`clang-release` 重建 exit 0，378 项 Win7 导入审计通过；主人当前桌面可见
`smoke_roof_marshal_survivability.json` 105 条命令通过，覆盖樱桃炸弹一次 1800、土豆雷限制为 1800、
大嘴花两口后首领 `15000→14960`、魅惑免疫、水草束缚/释放、5-9 `ROOF` 清洁车启动且不伤首领，
以及普通僵尸仍被草地推车秒杀。同步截图确认大嘴花咬合后首领仍在场且血条为 `14960/15000`。

完整指挥与天气验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_command`、`smoke_roof_marshal_weather`、`smoke_roof_marshal_visual`、
`smoke_roof_marshal_survivability`、`smoke_level_5_9_boss_slot` 与 `smoke_roof_marshal_noinstance`
均 exit 0、`script finished OK`。天气专项覆盖第一大关 no-op、第三次指挥中雨、既有大雨不降级/不刷新、
残血从小雨切到无台风大雨、后续受击不重置余时，以及中雨/大雨和指挥累计次数的快照往返；同步截图
确认天气板、屋顶雨景、1.2 倍本体与影子同时正确显示。既有 `smoke_night_rain` 和
`smoke_roof_rain_sky` 亦可见通过，证明自然雨势与屋顶雨云路径未被首领入口改写。

2026-08-10 修正验收：`clang-release` 配置、编译、LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_eating_command` exit 0、26 条命令通过，实测 `attackDamage=250`、首口坚果
`4000→3750`、全程保持 `anim_eat`，约 1 秒仍召唤 3 只，快照重载后继续啃食且命令计数为 1；
`smoke_roof_marshal_command` exit 0、119 条命令通过，锁定 36 种白名单、全部现有精英允许、红眼/自身
排除，以及第二批突击令只强化一行、移动/啃食均为 150% 并完整入档；`smoke_roof_marshal_visual`
exit 0、46 条命令通过，断袖资源已加载，7999 血实时断臂和快照重载截图均显示深蓝断袖，致死后专属
军帽掉头仍正常。`smoke_roof_marshal_noinstance -NoInstance` exit 0、25 条命令通过，普通小鬼啃食父类
回归 `smoke_imp_eat` exit 0、18 条命令通过。

2026-08-10 强化回归：`clang-release` 重建和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_command` exit 0、133 条命令通过，覆盖正式第 15 波血条出场/读档、1/6/4 秒召唤节奏、10 秒突击强化、
首领随突击换行及残血阶段仅允许突击换行；`smoke_roof_marshal_control_resistance` exit 0、25 条命令
通过，覆盖黄油定身上限 1.25 秒、定身期间不刷新、自然解除后 5 秒免疫及快照往返；
`smoke_roof_marshal_assault_visual` 默认与 `-NoInstance` 均 exit 0、32 条命令通过，截图确认目标行
所有僵尸显示 `Zombie_flag1.png` 红旗、首领跑向该行、中央 2.8 秒突击令警报，以及强化满 10 秒后
红旗同步消失。`smoke_roof_marshal_eating_command`、`smoke_roof_marshal_weather` 和
`smoke_roof_marshal_visual` 亦均 `script finished OK`；天气测试第三次指挥取证点已按 6 秒常态节奏
同步到约 13 游戏秒。

2026-08-10 首领血条验收：`clang-release` 配置、编译和 LTO 链接 exit 0；主人当前桌面可见
`smoke_roof_marshal_boss_bar` exit 0、39 条命令全部通过。专项锁定出场前隐藏、12000 满血、
8000/4000 标线位置、7000 精锐阶段、快照往返、3000 狂暴阶段和致死隐藏；五张同步截图确认最终
560×18 版本的标题、实际生命与黑金阶段铭牌清晰，并显著减少对第五路的遮挡。

2026-08-10 召唤质量强化：`clang-release` 配置、编译和 LTO 链接 exit 0；主人当前桌面可见
`smoke_roof_marshal_command` exit 0、142 条命令全部通过。专项锁定 12000 血高威胁概率 0%、6000 血
65%、3999 血 100%，并在狂暴阶段连续两批断言 4/4 高威胁单位；最后一批固定 Seed 42 实际为普通扶梯、
精英扶梯、普通扶梯和气球，未混入标准池杂兵，快照重载后仍保持 4/4。

2026-08-12 首领血条性能修复：原 `GetFirstActiveZombieOfType()` 虽无临时数组，仍在每个渲染帧扫描
全体僵尸；20,000 僵尸压测中 `SceneCmd.RoofMarshalBossHealthBar` 实测约 1.49ms。`EntityRegistry` 现维护
按实体 ID 有序的屋脊督军专用弱索引，由正常生成与 `AddZombieWithID` 读档恢复共同登记，并沿既有周期
清理过期项；血条只遍历督军候选、即时复核活动/垂死/生命状态，并用 `shared_ptr` 保证读取期间生命周期。
正常 0～1 个首领时查询不再随普通僵尸数量增长，开发模式多实例仍稳定选择最小实体 ID。
`clang-release` 配置、编译、LTO 链接及 378 项 Win7 导入审计通过；主人当前桌面可见 Vulkan
`smoke_roof_marshal_boss_bar` exit 0，39 条命令全部通过，覆盖出场前隐藏、正常生成、阶段扣血、
`AddZombieWithID` 快照恢复和致死隐藏，五张截图已检查。首次回归同时暴露该脚本仍停在调参前的
12000/8000/4000 旧断言，现已同步当前 15000/11000/5400 权威常量。尚未由 Codex 自动复现主人完整
20,000 僵尸压力场景；后续以 `SceneCmd.RoofMarshalBossHealthBar` 的实测桶确认最终回收量。
