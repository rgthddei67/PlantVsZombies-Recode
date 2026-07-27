# 海豚僵尸设计规格

日期：2026-07-27

## 目标

实现经典海豚僵尸并在冒险 3-7 首次出场。数值与行为以 C# 原版为准，同时提供统一的“植物阻拦僵尸跳跃”接口，为后续高坚果接入撑杆、海豚等跳跃类型打基础。

## 已确认资源与主人约定

- reanim：`Zombie_dolphinrider.reanim`，12 FPS。
- 轨道区间：`anim_walkdolphin` 15–53、`anim_walk` 54–92、`anim_jumpinpool` 93–137、`anim_ride` 138–142、`anim_dolphinjump` 143–165、`anim_eat` 166–207、`anim_swim` 208–249、`anim_death` 250–288。
- 主人给定帧事件直接使用：啃食 182、201；死亡 288。
- 断肢按 C#：隐藏外臂手、下臂，并把海豚僵尸上臂换成断臂贴图。
- 断头隐藏 `anim_head1` 与 `anim_head2`。
- 水中断头、断臂只更新模型，不抛任何粒子；陆地断头使用现有海豚头粒子，断臂复用通用僵尸手臂粒子。
- 音效从 `D:\PVZ\中文年度加强版完整版\Test\sounds` 按 C# 名称导入。

## 数值

| 项目 | 原版值 | 本项目表示 |
|---|---:|---|
| 本体生命 | 500 | `mBodyMaxHealth = mBodyHealth = 500` |
| 出怪权重 | 1500 | `gamedata.json weight` |
| 最早波次 | 10 | `appearWave = survivalRound = 10` |
| 骑豚陆地速度 | 0.89–0.91 px/tick | 按原版 47 tick/s 对齐为约 42 px/s；根运动 27 FPS |
| 弃豚速度 | 0.23–0.37 px/tick | 按平均值对齐为约 14 px/s；根运动 9 FPS |
| 入水动画 | 16 FPS | clip 倍率 `16/12` |
| 骑乘动画 | 12 FPS | clip 倍率 `1` |
| 跳跃动画 | 10 FPS | clip 倍率 `10/12` |

## 状态机

| 状态 | 稳态轨道/一次性轨道 | 碰撞 | 行为 |
|---|---|---|---|
| `APPROACHING` | `anim_walkdolphin` | 开 | 仅允许生成在泳池水路；快速向左移动 |
| `ENTERING_POOL` | `anim_jumpinpool` | 关 | 越过池沿；结束后进入骑乘 |
| `RIDING` | `anim_ride` | 开 | 水中快速骑行；首次碰到可吃植物时起跳，不啃食 |
| `JUMPING` | `anim_dolphinjump` | 关 | 跳过目标；30% 时检查植物阻拦能力；跳跃结束弃豚 |
| `SWIMMING` | `anim_swim` | 开 | 普通速度游泳，可啃食、魅惑、被水草抓取 |
| `WALKING_WITHOUT_DOLPHIN` | `anim_walk` | 开 | 离开泳池后的普通走路状态 |

入水与跳跃期间关闭碰撞，避免同一帧碰撞批次抢占动画。状态、目标植物 ID、阻拦检查标志和已提交位移必须入存档；读档恢复 Animator 当前轨道与进度，不把一次性动画粗暴跳到终点。

## 视觉与逻辑坐标

C# 基于 800×600 的绝对坐标不照搬。池沿判断统一使用 `Board::IsPoolWorldPosition`，生成限制统一使用 Board 行语义。

reanim 在入水、骑乘、跳跃和游泳轨道之间含局部根位移。实现以逻辑 Transform 承担碰撞和出界判断，以 `GetVisualPosition` 的瞬态补偿消除换轨闪跳；跳跃完成时把动画已经表现的水平位移一次提交到 Transform，随后清零视觉补偿。

## 跳跃阻拦契约

新增 `ZombieJumpType`，至少包含 `POLEVAULT`、`DOLPHIN_RIDER`、`POGO`。`Plant::BlocksZombieJump(type)` 默认返回 `false`。

- 撑杆僵尸起跳前查询 `POLEVAULT`；被阻拦时弃杆、转慢速走路并开始啃食。
- 海豚僵尸跳跃达到 30% 时查询 `DOLPHIN_RIDER`；被阻拦时播放 `Bonk`，在植物前方落水弃豚，转普通游泳。
- 本任务不实现高坚果；后续高坚果只需覆写该接口即可同时接入两类跳跃。

## 断肢、死亡与音效

- 海豚在手臂阈值前若仍骑豚，不立即断臂；弃豚后补做阈值检查，与 C# 一致。
- 进入池后所有断头、断臂粒子均被抑制，模型和生命状态仍正常更新。
- 骑豚、入水或跳跃状态死亡时直接移除；弃豚后按 `anim_death` 播放并在主人给定第 288 帧移除。
- `DolphinAppears` 只在正式刷新时播放；发现目标并开始跳跃时同时播放
  `DolphinBeforeJumping` 与 `PlantWater`。其余使用 `ZombieEnteringWater`、`Bonk`、`LimbsPop`。

## 冒险与图鉴

- `spawnlists.json` 新增 3-7（内部关卡 25），20 波，种类为普通、路障、铁桶、海豚。
- 海豚是该关唯一新威胁；第 10 波以后才出现，通关后获得高坚果，符合“先展示跳跃威胁，再给反制”的节奏。
- 增加中文图鉴名称、能力说明与原版风味文案。

## 验证

- 静态检查 3-1 至 3-7 刷怪表，确认 3-7 出现海豚且前序关卡不出现。
- 可见 AutoTest 验证：仅水路生成、入水、骑乘、跳过第一株、弃豚后啃食、断肢无水中粒子、死亡消失、无 watchdog。
- 保存/读取覆盖一次性状态，确认轨道、状态、碰撞开关与部件可见性一致。
- 图鉴解锁检查 3-7 前后差异。
- 完成 `clang-playtest` 与正式 `clang-release` 构建。
