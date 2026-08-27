---
name: project_pvz_bobsled_team_zombie
description: 第七大关雪橇车队僵尸的四人编队、雪锚果散射收束、原版动画资源、存档与验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 雪橇车队僵尸

## 已实现行为

`ZOMBIE_BOBSLED_TEAM` 是一个正式出怪身份，但一次接纳会生成队长和三名跟随者共四个真实实体。每人 270 本体生命；队长另持有 300 雪橇耐久。车队在 Board 当前真实冻土上以 60 px/游戏秒滑行，离开冻土、雪橇破坏或撞植物后进入 1.5 秒 `LANDING`，随后四人独立步行、啃食、受控、魅惑、断肢和死亡。正式成本为 2000、开放波次 5、生存开放轮次 10，每波最多接纳三队。

雪橇撞植物时由目标的 `ResolveWinterGroundImpact(COLLISION)` 返回通用响应；僵尸不识别 `PlantType`。普通响应让成员向本行更深处和相邻行散开；存活且锚定的雪锚果可反复响应，把四人留在植物右侧同一行。两条路径都由雪橇以 `DamageSource::ZOMBIE` 造成 1200 碰撞伤害；响应先于伤害决定，所以致死当前撞击仍受约束，之后植物死亡失效。碰撞提交时先快照植物 collider 中心，再在伤害前发射 `ZombieBobsledPlantImpact` 两层冰雪飞溅，因此目标即使被同次伤害击杀也不会丢失反馈锚点。步行后的每次啃食为 50，切入 `anim_eat` 使用 0.1 秒混合；主人提供的动画事件为死亡 133、啃食 151/169，不再增加其他帧事件。

## 编队与存档契约

队长取得稳定实体 ID 后才在首次实战更新创建正式跟随者；`CreateZombieWithID` 不补队。选卡和图鉴为了完整宣传画面会显式创建四个无稳定 ID、无碰撞、无玩法注册的展示实体，它们不走正式跟随者生成入口，候选语义仍为一队。每名正式成员保存 `role/phase/slot/leaderID/memberIDs`、落地起止位置和计时，Board 另保存本波已接纳队数。读档逐只恢复实体，不重生缺员、不重抽落点；缺领队或缺成员的车上孤儿安全进入普通下车，已经步行的成员死亡互不连带。

Board 的每个选卡展示实体仍参与 `mZombieNumber` 创建/销毁平衡。整队拆除总是先把队长本人放入待死亡数组，再按槽位 1～3 解引用跟随者；不能依赖尚未建立的 `memberIDs[0]`，否则单体预览或正式创建首帧的队长会消失但计数永久多 1。图鉴使用独立弱引用组管理四名展示成员，切换条目时逐一销毁，且不接触 Board 计数。

乘车与落地阶段的世界出界回收统一使用队长 X，独立步行后才恢复逐成员位置判断。自然波从 `SCENE_WIDTH + 40` 创建队长，而后排三名成员分别位于 `+50/+100/+150`；若沿用通用逐实体 `SCENE_WIDTH + 65` 右边界，第一次一秒轮询会先 `Die()` 后排并触发整队回收。专项必须从自然出生 X 等待超过一秒，再断言四名成员仍处于 `RIDING`。

乘车时只有队长启用 275×115 车辆碰撞体，跟随者不接受单独弹丸索敌；全队免疫魅惑、减速、冻结、黄油、麻痹、水草和地面危害。落地中可受伤但仍保持控制免疫。车上任一成员本体死亡会终止仍附着的整队，步行后恢复普通死亡语义。

## 原版资源与表现

`scripts/import_bobsled_team_assets.ps1` 用锁定的 SHA-256 从 `D:\PVZ\中文年度加强版完整版\Test` 导入 `Zombie_bobsled.reanim`、19 张部件图、5 张雪橇图和 `ZombieBobsledHead.png`。雪橇前后层与坠毁阶段按原版槽位绘制，默认实例和 `-NoInstance` 共用同一顺序。

断头隐藏 `anim_head1/anim_head2` 并发射专属头部粒子；断臂统一隐藏走路的 `Zombie_outerarm_lower/hand` 与啃食专用的 `Zombie_outerarm_lowereating/handeating`，把 `Zombie_dolphinrider_outerarm_upper` 换为 `Zombie_bobsled_outerarm_upper2`，并在走路/啃食切轨后按断肢状态重放这一 helper，再发射原版手臂粒子。步行影子 Y 偏移为 38，乘车/落地为 40；两者是用户目验后定下的视觉值。

## 关卡与验证

第七大关在 7-2、7-5、7-8、7-9 投放雪橇车队；7-2 以撑杆、快铁桶、橄榄球和加固铁门形成冲刺前线，后续与冰墙、钻机和红眼进入高压组合。选卡预览和图鉴详情均显示四名队员共乘一辆完整雪橇；图鉴网格缩略图仍是一格正式候选。

2026-08-24 的 `clang-release` 全量 LTO 构建及 378 项 Win7 导入审计通过；`save-migration`、`save-schema` 此前均通过。断臂/撞击反馈修复后的 `smoke_bobsled_team.json` 共 158 条命令，在当前桌面可见的默认实例与 `-NoInstance` 路径都执行至 command 157、exit 0、`status=passed`；覆盖资源键、正式生成/同波上限、两种散开、雪锚果实际碰撞与粒子 collider 相交、RIDING/LANDING 快照、死亡与啃食帧、啃食中断臂四轨显隐、断肢粒子和图鉴。

2026-08-25 修复自然出生区后排成员误触发世界右边界的问题。`clang-debug` 构建及可见专项通过；最终 `clang-release` 全量 LTO 构建与 378 项 Win7 导入审计通过，默认 Vulkan 实例和 `-NoInstance` 两次可见 `smoke_bobsled_team.json` 均执行 170 条命令至 command 169、exit 0、`status=passed`。新增用例从 X=1140 直造正式四人队，跨过 1.2 游戏秒边界轮询后仍断言 4 名 `RIDING`、队长存活成员数为 4，并保存同步截图。

2026-08-25 修复选卡预览队长拆队时未执行基础 `Die()` 导致的幽灵计数，并按主人要求把选卡与图鉴详情扩成四人共乘完整雪橇。主人原始 `level56_data.json` 的 SHA-256 一致只读副本实测为最终波 30/30、实体 0、内部计数 0，修复版第 2 帧生成奖杯；原文件未改写。最终 `clang-release` 全量 LTO 构建与 378 项 Win7 导入审计通过；`smoke_bobsled_team_preview_count`、`smoke_bobsled_team_zero_count_save`、`smoke_bobsled_team_almanac_preview` 以及 177 命令的 `smoke_bobsled_team` 均可见通过，图鉴与选卡四槽掩码为 15、X 跨度 150px，两个展示专项另通过 `-NoInstance`。

2026-08-27 雪锚果改为存活冻土上反复响应后，`smoke_bobsled_team.json` 对齐当前每波三队上限，并把落地后相对编队跨度窗口放宽为仍能判定同行编队、但不依赖单帧取证落点的 120～130px。默认 Vulkan 与 `-NoInstance` 两次可见专项均执行 192 条命令至 command 191、exit 0、`status=passed`；实际碰撞确认 1200 伤害后雪锚果仍为 ready，四名成员继续同排落地，全部 8×2 张截图目验正常。
