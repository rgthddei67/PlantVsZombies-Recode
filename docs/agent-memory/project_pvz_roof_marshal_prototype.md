---
name: project_pvz_roof_marshal_prototype
description: 2026-08-09 5-9 屋脊督军视觉样机与生存层；12000 生命、首领直杀抗性，尚未接入 BOSS 指挥机制
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-09
---

# 5-9 屋脊督军视觉样机与生存层

2026-08-09 主人确认 5-9 BOSS 采用“僵尸博士麾下得力干将”的指挥型首领方向，并指定角色动画
先复用普通僵尸。第一阶段先验证素材与动画适配，随后按主人确认加入首领生存层；仍不提前实现 15 波出场、
指挥技能、指挥车或第六大关内容。

当前新增 `ZOMBIE_ROOF_MARSHAL` 与 `RoofMarshalZombie`，权重为 0、`appearWave=15`，只允许开发者面板或
AutoTest 直造；`AdventureProgression::BossSlot::RESERVED` 尚未绑定该类型，正式 5-9 仍不会自动生成 BOSS。
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

验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_visual.json` exit 0、32 条命令全部通过：两只样机、12000 当前/最大生命、五项资源断言、专属/通用粒子
互斥、单粒子/单 quad 与发射原点相对包围盒均通过；3.5 秒后死亡个体回收、仅保留另一只，并同步
保存走路和军帽随头飞出截图。`smoke_roof_marshal_noinstance.json -NoInstance` exit 0、24 条命令全部
通过，同样覆盖专属掉头与死亡回收。`smoke_level_5_9_boss_slot.json` exit 0、14 条命令全部通过，
继续锁定白天屋顶、15 波、三只普通预览和未绑定的 `RESERVED` BOSS 槽。

生存层验证：`clang-release` 重建 exit 0；主人当前桌面可见
`smoke_roof_marshal_survivability.json` 覆盖樱桃炸弹一次 1800、土豆雷限制为 1800、大嘴花两口各 1800、
魅惑免疫、水草束缚/释放、5-9 `ROOF` 清洁车启动后首领保持 12000，以及普通僵尸仍被草地推车秒杀。
默认与 `-NoInstance` 视觉脚本已按 12000 致死伤害同步回归；`smoke_reinforced_door.json` 综合回归
继续通过，证明普通小推车和既有植物直杀抗性未被首领接口改坏。
