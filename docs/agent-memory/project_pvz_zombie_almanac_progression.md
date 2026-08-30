---
name: project_pvz_zombie_almanac_progression
description: 僵尸图鉴由已通关关卡 spawnlist 与永久实际遭遇记录决定可见项；含当前关防泄露、必然召唤单位展开与 UI AutoTest
metadata:
  node_type: memory
  type: project
  originSessionId: 019fa16d-d877-7b71-8530-65f12c3330ca
---

# 僵尸图鉴随冒险进度解锁

## 2026-08-30 固定遭遇与资料完整性

`ZOMBIE_SNOW_BURROW` 补齐标题与说明。静态全集审计还发现 `ZOMBIE_ROOF_MARSHAL`
同样缺少两枚资料键；它由 5-9 的 `BossSlot::ROOF_MARSHAL` 固定创建，不在随机出怪表中，
所以 `ZombieAlmanacScene` 现在按关卡顺序合并随机池与固定 BOSS 遭遇，在 5-9 通关后解锁督军，
不会为图鉴污染 `spawnlists.json` 或提前泄露当前关。

当前可解锁僵尸的标题/说明键均存在、非空且唯一。植物侧同步审计全部已注册类型也没有缺口；
主人怀疑的两株最新植物界碑花与曙光莲，当前 `info.txt` 已各自包含完整标题和说明。

## 2026-08-26 预览与资料断言

图鉴状态投影新增当前页网格实例的轨道/暂停状态，以及右侧详情已解析标题和说明行数。
`smoke_zombie_almanac_winter_preview.json` 因而能直接锁定冰墙工程师的文本和天气干扰僵尸在
网格/详情中的 `anim_idle`，而不只依赖截图人工判断。

2026-07-27：`ZombieAlmanacScene` 不再枚举全部已注册僵尸。`GameAPP::mAdventureLevel` 是当前待玩的关卡，因此只累计 `spawnlists.json` 中 `level < mAdventureLevel` 的合法已注册类型；按关卡号与数组顺序去重，得到稳定的首次遭遇顺序。第 3 关只显示前两关见过的普通僵尸，第 3 关首次出现的路障要到第 4 关才解锁。

`weight: 0` 的召唤型子单位不能为了图鉴解锁写入随机池。当前把舞王与精英舞王显式展开为伴舞这一项必然遭遇；以后新增必然生成的附属僵尸时，同步扩展这一映射。按行地形替换仍不假定玩家必然遇见。

精英舞王属于不能由 spawnlist 推断的概率天气变异。`GameAPP::mEncounteredEliteDancer` 只在 `Board::CreateResolvedWaveZombie` 成功创建实际类型 `ZOMBIE_ELITE_DANCER` 后置位，并立即尝试保存到 `PlayerInfo.json` 的 `encounteredEliteDancer`；旧档缺字段默认 `false`。解析器命中但候选超额、通用 `CreateZombie()` 直造、预览与 `CreateZombieWithID()` 读档都不会误解锁。图鉴在通关并集末尾合并该永久记录，因此即使冒险进度尚未覆盖舞王关卡，也能显示玩家确实刷出的精英舞王。

AutoTest 的 `dump_state` / `assert_state` 现支持 `ZombieAlmanacScene`，字段为 `scene`、`adventureLevel`、`encounteredEliteDancer`、`zombieAlmanacEntries`、`zombieAlmanacEntryCount`、`zombieAlmanacSelected`。`smoke_zombie_almanac_progression.json` 可见验证第 3/4 关边界；`smoke_dancer_almanac.json` 回归舞王与伴舞详情；`smoke_elite_dancer_almanac_unlock.json` 断言直造不解锁、正式天气变异创建成功后解锁，并从游戏暂停菜单进入图鉴目验精英条目。AutoTest 会短路真实 PlayerInfo 磁盘读写，因此永久字段的磁盘契约以保存/加载源码审查覆盖。

2026-07-27 本次改动经 `clang-playtest` 与 `clang-release` 零警告构建；新解锁专项、精英舞王变异回归和第 3/4 关图鉴进度回归均在当前桌面可见运行并 exit 0，新图鉴截图已目验。

2026-08-26：左侧僵尸图鉴网格固定为 8 列×5 行、每页 40 项，防止第六行压到“返回索引”。分页按既有首次遭遇顺序切片，只重建当前页的纯 UI 僵尸与点击体，右侧已选详情不因翻页重置。复用 `Zen_NextGarden` 做两个固定槽位：上一页中心 X=600、下一页中心 X=680；当前两页时只显示可用方向，未来第三页时两键可同时出现而不重合。`smoke_zombie_almanac_pagination.json` 以 7-9 后 41 项锁定 40+1、双向真实点击、末项冰像处刑者详情、资源、按钮位置与默认/`-NoInstance` 截图；`smoke_zombie_almanac_progression.json` 额外锁定单页时两个按钮都隐藏。2026-08-26 `clang-release` 构建与 378 项 Win7 导入审计通过，上述三次可见运行均 exit 0、`status=passed`。
