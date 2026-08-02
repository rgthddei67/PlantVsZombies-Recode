---
name: project_pvz_elite_pogo_zombie
description: 2026-08-02 精英跳跳僵尸的碳纤维外观、冲击缓冲、非磁性装备、每波上限、4-9 编排与预览动画
metadata:
  node_type: memory
  type: project
---

# 精英跳跳僵尸

## 当前契约

- `ZOMBIE_ELITE_POGO` 由 `ElitePogoZombie` 继承普通跳跳：850 本体生命、能力/动画/持杆前进倍率 1.15，生存权重 2700、出现波次 15、出现轮次 14，仍只允许四条陆路。
- 外观为深紫运动外套、青绿色裤装、金色鞋部高光和黑金碳纤维跳杆。`scripts/recolor_elite_pogo.ps1` 生成并锁定 19 个 reanim、运行时换图和粒子资源；权威资产只在 `build/clang-release/resources`。
- 碳纤维杆不实现磁性装备：`HasMagneticItem/ExtractMagneticItem` 均拒绝，磁力菇不会进入吸取充能。
- 第一次在前跳阻拦节点命中高坚果时仍触发 Bonk/星星，消耗一次性冲击缓冲器，对植物造成 600 点僵尸来源伤害，并保杆完成前跳；缓冲状态入档。第二次阻拦完全复用普通跳跳的折杆和啃食。
- `Board` 对正式波次成功创建的精英跳跳每波最多计 1 只；计数进入关卡存档，并在新波及生存下一波正确重置。4-9（内部 36）为 30 波六类型池：普通、路障、铁桶、加固铁门、普通跳跳、精英跳跳。
- 普通/精英共享 `PogoZombie::Update()` 的纯展示弹跳；选卡和图鉴详情无水平位移、碰撞或落地声，图鉴网格缩略图继续暂停。

## 资源、图鉴与验证

- `ElitePogo.reanim` 独立替换衣裤、鞋、手臂和跳杆；断臂运行时贴图与 `ZombieElitePogo` 断杆粒子通过父类窄虚入口选择，普通跳跳资源保持原样。`resources.xml` 同时注册 reanim 与三列粒子贴图，构建生成的 `manifest.txt` 已确认包含四类入口资源。
- 图鉴在游玩 4-9 时不提前显示，通关后作为普通跳跳之后的第 25 项解锁；标题和说明位于 `info.txt`。
- 2026-08-02 `clang-release` 构建通过。`smoke_elite_pogo.json`、`smoke_elite_pogo_wave_cap.json`、`smoke_fog_spawnlists_4_7_to_4_9.json`、`smoke_elite_pogo_almanac.json` 和父类 `smoke_pogo_zombie.json` 均在主人当前桌面可见运行，窗口标题正确、exit 0、`run.log` 以 `script finished OK` 结束；截图覆盖外观、两次高坚果、磁力菇免疫、选卡与图鉴详情。
