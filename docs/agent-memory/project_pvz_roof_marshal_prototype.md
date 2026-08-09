---
name: project_pvz_roof_marshal_prototype
description: 2026-08-09 5-9 屋脊督军完整首领；12000 生命、生存抗性、指挥召唤、阶段移动与改天技能
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-09
---

# 5-9 屋脊督军首领

2026-08-09 主人确认 5-9 BOSS 采用“僵尸博士麾下得力干将”的指挥型首领方向，并指定角色动画
复用普通僵尸。当前已完成素材、生存层、15 波正式出场、指挥召唤、阶段移动与天气命令；不实现
指挥车或第六大关内容，僵尸博士仍留作未来 6-9。

`ZOMBIE_ROOF_MARSHAL` 与 `RoofMarshalZombie` 的独立随机权重保持为 0、`appearWave=15`；
`AdventureProgression::BossSlot::ROOF_MARSHAL` 只在内部关卡 45 的正式第 15 波由
`Board::TrySummonAdventureBoss()` 额外创建一只，固定中路 `x=1000`。开发者面板和 AutoTest 仍可直造，
但普通出怪池、预览和未来新增类型不会自动混入首领路径。
`RoofMarshal.reanim` 完整复制 `NormalZombie.reanim` 时间线，派生 `SetupZombie()` 先调用
`Zombie::SetupZombie()`，再把本体当前/最大生命设为 12000；因此继续复用普通走路、啃食和死亡帧事件，
没有新增帧号。

美术源分两层：`docs/art/roof-marshal/roof-marshal-concept-v1.png` 是 ImageGen 生成的整张概念设定稿，
只用于确定深海军蓝短军装、旧金领饰和紧凑大檐帽方向，绝不直接切片进游戏；权威运行素材由
`scripts/generate_roof_marshal_assets.ps1` 从普通僵尸小分体可复现生成。脚本只替换外套、四段袖子、
两只鞋、领饰和 `anim_hair` 对应的 64x31 帽子，保留原画布、Alpha、关节轴以及普通脸/手/腿。
主人随后确认军帽随头一起飞；脚本现把普通掉头图与军帽预合成为独立的
`ZombieRoofMarshalHead.png`，共 11 个生成结果均锁定 SHA-256。`resources.xml`、manifest、reanim/粒子键与
`GetTexture(key,false)` 断言已闭环。

视觉结论：白天屋顶默认实例化路径与 `-NoInstance` 路径均能稳定显示深蓝军装、金色领饰和帽子，
帽檐不遮眼，袖子在普通走路帧没有露出棕色接缝。`RoofMarshalZombie::HeadDrop()` 在隐藏
`anim_head1/anim_head2/anim_tongue/anim_hair` 前读取头轨世界锚点，只发射一颗
`RoofMarshalHeadOff` 粒子，因此军帽与头在抛起、旋转和下坠期间不会错位；通用 `ZombieHeadOff`
保持为 0。普通死亡时间线可正常进入 `anim_death` 并在事件终点回收。

生存契约：普通灰烬基础伤害仍为 1800；`TakePlantAshDamage()` 先把土豆雷传入的 `INT32_MAX` 等高值
压到 1800，再进入基类伤害链，因此生存词条仍能正常缩放。大嘴花每次完整咬合改为 1800 基础植物伤害，
返回 false 而不吞下首领。首领不可魅惑；缠绕水草只原地束缚 5 秒后自毁并释放首领，不拖沉、不扣血。
`Zombie::CanBeKilledByMower()` 默认 true，屋脊督军覆写为 false；5-9 屋顶清洁车接触它仍正常启动并
消耗，但从首领身上驶过不造成伤害，普通僵尸对照仍被秒杀。首领拒绝普通烧焦残影，致死灰烬仍走
本体常规死亡时间线与专属军帽掉头粒子。

指挥契约：登场 1 秒生成首批，此后生命不低于 4000 时每 9 秒生成 3 只，低于 4000 时每 7 秒生成
4 只；同一批使用不重复随机行并避开首领当前行，全部在 `x=910` 创建。常规池固定为普通、路障、
撑杆、铁桶、读报和铁门；低于 8000 后每个召唤位有 30% 从橄榄球、舞王、冰车、玩偶匣、气球、
矿工、跳跳、蹦极、扶梯、投篮车和巨人池抽取。两池是显式原版白名单，原创/精英、泳池专属、
召唤子单位、红眼巨人、督军自身和未来第六大关类型不得自动进入。每次创建后用现有 `anim_idle`
播放 1.2 秒指挥姿势，不新增动画帧事件。

移动与耐久阶段：本体不低于 4000 时不水平推进，稳态播放 `anim_idle2`，每 6 秒只换到相邻合法行；
逻辑行和碰撞箱先原子提交，独立视觉 Y 补偿在 0.65 秒内归零。低于 4000 后停止自主换行并按普通
僵尸走路推进。督军体积固定 1.2 倍，`gamedata.json` 偏移为 `[-60,-102]`，影子为 `(1.2,0.9)`；
视觉缩放不改变普通碰撞箱。普通 1/3 掉头临界值对督军完全取消，生命大于 0 始终保留头部，只有
本体生命归零才发射专属军帽连头粒子并进入死亡动画。

天气命令：每完成第 3、6、9……次实际指挥召唤，若 Board 当前为晴天或小雨，调用
`Board::TriggerRoofMarshalWeather(MEDIUM, 20, false)` 强制 20 游戏秒中雨；已有中雨不续期，已有大雨
不降级。生命首次从不低于 4000 跌至 `1..3999` 时，调用同一入口强制至少 30 游戏秒大雨；已有大雨
只在余时不足时补足一次。入口不额外抽取台风，并复用 `BeginRain` 的两秒过渡、粒子、声音、天气板
与 Board 现有存档字段。僵尸只保存其 `commandCount` 等施法节奏，绝不复制雨势或天气倒计时。

验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_visual.json` exit 0、32 条命令全部通过：两只样机、12000 当前/最大生命、五项资源断言、专属/通用粒子
互斥、单粒子/单 quad 与发射原点相对包围盒均通过；3.5 秒后死亡个体回收、仅保留另一只，并同步
保存走路和军帽随头飞出截图。`smoke_roof_marshal_noinstance.json -NoInstance` exit 0、24 条命令全部
通过，同样覆盖专属掉头与死亡回收。`smoke_level_5_9_boss_slot.json` exit 0，
锁定白天屋顶、15 波、三只普通预览、`ROOF_MARSHAL` 槽位及最终波唯一正式首领。

生存层验证：`clang-release` 重建 exit 0；主人当前桌面可见
`smoke_roof_marshal_survivability.json` 覆盖樱桃炸弹一次 1800、土豆雷限制为 1800、大嘴花两口各 1800、
魅惑免疫、水草束缚/释放、5-9 `ROOF` 清洁车启动后首领保持 12000，以及普通僵尸仍被草地推车秒杀。
默认与 `-NoInstance` 视觉脚本已按 12000 致死伤害同步回归；`smoke_reinforced_door.json` 综合回归
继续通过，证明普通小推车和既有植物直杀抗性未被首领接口改坏。

完整指挥与天气验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_command`、`smoke_roof_marshal_weather`、`smoke_roof_marshal_visual`、
`smoke_roof_marshal_survivability`、`smoke_level_5_9_boss_slot` 与 `smoke_roof_marshal_noinstance`
均 exit 0、`script finished OK`。天气专项覆盖第一大关 no-op、第三次指挥中雨、既有大雨不降级/不刷新、
残血从小雨切到无台风大雨、后续受击不重置余时，以及中雨/大雨和指挥累计次数的快照往返；同步截图
确认天气板、屋顶雨景、1.2 倍本体与影子同时正确显示。既有 `smoke_night_rain` 和
`smoke_roof_rain_sky` 亦可见通过，证明自然雨势与屋顶雨云路径未被首领入口改写。
