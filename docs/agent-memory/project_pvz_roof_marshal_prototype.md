---
name: project_pvz_roof_marshal_prototype
description: 2026-08-09 5-9 屋脊督军第一阶段视觉样机；普通僵尸时间线配独立军官分体素材，尚未接入 BOSS 指挥机制
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-09
---

# 5-9 屋脊督军视觉样机

2026-08-09 主人确认 5-9 BOSS 采用“僵尸博士麾下得力干将”的指挥型首领方向，并指定角色动画
先复用普通僵尸。第一阶段只验证素材与动画适配，不提前实现 15 波出场、指挥技能、指挥车或第六大关内容。

当前新增 `ZOMBIE_ROOF_MARSHAL` 与 `RoofMarshalZombie`，权重为 0、`appearWave=15`，只允许开发者面板或
AutoTest 直造；`AdventureProgression::BossSlot::RESERVED` 尚未绑定该类型，正式 5-9 仍不会自动生成 BOSS。
`RoofMarshal.reanim` 完整复制 `NormalZombie.reanim` 时间线，派生 `SetupZombie()` 只调用
`Zombie::SetupZombie()`，因此复用普通走路、啃食和死亡帧事件，没有新增帧号。

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

验证：`clang-release` 配置、编译和 LTO 链接 exit 0。主人当前桌面可见
`smoke_roof_marshal_visual.json` exit 0、30 条命令全部通过：两只样机、五项资源断言、专属/通用粒子
互斥、单粒子/单 quad 与发射原点相对包围盒均通过；3.5 秒后死亡个体回收、仅保留另一只，并同步
保存走路和军帽随头飞出截图。`smoke_roof_marshal_noinstance.json -NoInstance` exit 0、22 条命令全部
通过，同样覆盖专属掉头与死亡回收。`smoke_level_5_9_boss_slot.json` exit 0、14 条命令全部通过，
继续锁定白天屋顶、15 波、三只普通预览和未绑定的 `RESERVED` BOSS 槽。
