---
name: project_pvz_bobsled_team_zombie
description: 第七大关雪橇车队僵尸的四人编队、雪锚果散射收束、原版动画资源、存档与验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-24
---

# 雪橇车队僵尸

## 已实现行为

`ZOMBIE_BOBSLED_TEAM` 是一个正式出怪身份，但一次接纳会生成队长和三名跟随者共四个真实实体。每人 270 本体生命；队长另持有 300 雪橇耐久。车队在 Board 当前真实冻土上以 60 px/游戏秒滑行，离开冻土、雪橇破坏或撞植物后进入 1.5 秒 `LANDING`，随后四人独立步行、啃食、受控、魅惑、断肢和死亡。正式成本为 2000、开放波次 5、生存开放轮次 10，每波最多接纳一队。

雪橇撞植物时由目标的 `ResolveWinterGroundImpact(COLLISION)` 返回通用响应；僵尸不识别 `PlantType`。普通响应让成员向本行更深处和相邻行散开；可用的雪锚果原子消费一次锚定，把四人留在植物右侧同一行。两条路径都由雪橇以 `DamageSource::ZOMBIE` 造成 1200 碰撞伤害。步行后的每次啃食为 50，切入 `anim_eat` 使用 0.1 秒混合；主人提供的动画事件为死亡 133、啃食 151/169，不再增加其他帧事件。

## 编队与存档契约

队长取得稳定实体 ID 后才在首次实战更新创建跟随者；预览和 `CreateZombieWithID` 均不补队。每名成员保存 `role/phase/slot/leaderID/memberIDs`、落地起止位置和计时，Board 另保存本波已接纳队数。读档逐只恢复实体，不重生缺员、不重抽落点；缺领队或缺成员的车上孤儿安全进入普通下车，已经步行的成员死亡互不连带。

乘车时只有队长启用 275×115 车辆碰撞体，跟随者不接受单独弹丸索敌；全队免疫魅惑、减速、冻结、黄油、麻痹、水草和地面危害。落地中可受伤但仍保持控制免疫。车上任一成员本体死亡会终止仍附着的整队，步行后恢复普通死亡语义。

## 原版资源与表现

`scripts/import_bobsled_team_assets.ps1` 用锁定的 SHA-256 从 `D:\PVZ\中文年度加强版完整版\Test` 导入 `Zombie_bobsled.reanim`、19 张部件图、5 张雪橇图和 `ZombieBobsledHead.png`。雪橇前后层与坠毁阶段按原版槽位绘制，默认实例和 `-NoInstance` 共用同一顺序。

断头隐藏 `anim_head1/anim_head2` 并发射专属头部粒子；断臂隐藏 `Zombie_outerarm_lower/Zombie_outerarm_hand`，把 `Zombie_dolphinrider_outerarm_upper` 换为 `Zombie_bobsled_outerarm_upper2`，再发射原版手臂粒子。步行影子 Y 偏移为 38，乘车/落地为 40；两者是用户目验后定下的视觉值。

## 关卡与验证

第七大关在 7-2、7-3、7-5、7-7、7-8、7-9 投放雪橇车队，7-4 与 7-6 留出其他机制教学空间。图鉴只显示一名代表乘员和完整雪橇。

2026-08-24 的 `clang-release` 全量 LTO 构建及 378 项 Win7 导入审计通过；`save-migration`、`save-schema` 均通过。`smoke_bobsled_team.json` 共 145 条命令，在当前桌面可见的默认实例与 `-NoInstance` 路径都执行至 command 144、exit 0、`status=passed`、`script finished OK`；覆盖资源键、正式生成/同波上限、两种散开、雪锚果实际碰撞、RIDING/LANDING 快照、死亡与啃食帧、断肢粒子和图鉴。既有 `smoke_snow_anchor_nut.json` 同次 Release 默认实例回归执行至 command 81 并通过。
