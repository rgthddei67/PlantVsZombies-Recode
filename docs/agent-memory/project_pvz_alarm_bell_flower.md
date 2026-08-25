---
name: project_pvz_alarm_bell_flower
description: 第七大关警铃草、同行未提交动作中断、三叶草骨架适配、存档与验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 警铃草与未提交动作中断

## 身份与选择规则

`PLANT_ALARMBELLFLOWER` 是内部 level 60（7-6）通关奖励，供 7-7 起使用。当前调参为
25 阳光、50 秒冷却、300 生命，模拟层 `persistent=false`。种下后首个正式逻辑步响铃，
不造成伤害；无合法目标仍消耗卡牌并完成演出。

植物只遍历本行活动、未死亡、未魅惑的僵尸，读取
`Zombie::GetInterruptibleSpecialActionRemaining()`。负数表示当前没有公开的未提交动作；
候选按剩余时间最短、稳定僵尸 ID 最小选择唯一目标，只调用一次
`InterruptUncommittedSpecialAction()`，边沿失败不改投第二名。植物不识别具体僵尸类型。

当前动作拥有者合同如下：气象干扰僵尸 `CHANNELING` 进入 5 秒重启；冰裂钻机
`CHARGING` 取消蓄力并可从完整 3.5 秒重试；冰墙工程师 `BUILDING` 原子拆除施工墙，
保留能力并可从完整 4 秒重新施工。已经提交的整栏预报干扰、地裂和完工冰墙均不回滚。
未来冰像处刑者只需在当前锤击尚未提交时覆写同一接口，不得让警铃草维护品种表。

## 生命周期、动画与存档

脉冲提交后植物保留 1 游戏秒演出，期间不可啃食且普通伤害不会取消余韵；保存
`pulseTriggered/pulseSucceeded/afterglowRemaining`。读档恢复表现但不得再次中断、播声或发粒子，
余时为零的损坏快照只等待下一逻辑步回收。

场上动画直接复用原版 `Blover.reanim` 的 12 FPS 时间轴：待机 0～32、弯折 33～51、
余韵循环 52～61，`anim_blow` 与返回段均以 2.0 倍播放。`Blover_dirt_back` 只换成固定叶座，
`Blover_stem2/stem1` 保留为连续双段茎，铃头替换 `Blover_head`，铃舌作为头轨 follower 共用
完整仿射变换。三种铃头按底部茎秆插口而非 Alpha 包围盒校准；右倾响铃/疲惫头在 120px
画布内比正面头左移 10px，避免换图放大阶段接到铃身左后侧。最后 0.22 秒整株淡出。

## 资源闭环

非写实 PvZ 手绘母图为 `docs/art/alarm-bell-flower/alarm-bell-flower-rig-parts-v1.png`；
`scripts/generate_alarm_bell_flower_assets.ps1` 锁定母图、Blover 时间轴和全部输出 SHA-256，
确定性去除连通棋盘背景并生成三张铃头、铃舌、叶座、卡图和整行脉冲。卡图的完整角色内容
按主人确认的 0.8 比例（120px 画布内 86px）居中，场上骨架尺寸保持不变。

资源必须同时闭合 `resources.xml` 的 GameImages/ParticleTextures/Reanimation、`ResourceKeys`、
manifest，以及 AutoTest 的 `HasReanimation` 和每张运行时 `GetTexture(key,false)` 断言。
警铃动画没有新增帧事件；玩法仍在首个正式逻辑步立即提交。

## 验证入口

核心专项是 `smoke_alarm_bell_flower.json`，覆盖数值、资源、同行选择、稳定 ID、异排、
无伤害、气象干扰重启、钻机重置、施工墙撤销、演出中快照与 7-6 奖励。
`visual_alarm_bell_flower_transition.json` 以 0.35 倍速分别截取换头前、换头帧、两个连续
放大帧、余韵和淡出，专门防止静态图硬切、茎头脱节与语义轴心偏移。最终证据须使用
`clang-release` 在默认实例路径与 `-NoInstance` 当前桌面可见运行。

2026-08-25 最终证据：资源生成器复跑并锁定全部哈希；`clang-release` 完整重编 156 个目标后
链接成功，主程序 Win7 导入审计通过 378 项，CTest 的 save-migration、save-schema 与
plant-defense-monte-carlo 为 3/3。`smoke_alarm_bell_flower.json` 在默认 Vulkan 实例路径和
`-NoInstance` 当前桌面可见运行均执行 120 条命令至 command 119，exit 0、`status=passed`、
`script finished OK`；新增断言确认 SPENT 气象干扰与既有 30 秒黑障不被回滚。
`visual_alarm_bell_flower_transition.json` 两路径均执行 30 条命令至 command 29 并通过；主人
逐轮确认卡图 0.8、原版骨架动态、双段茎连接与最终铃头插口偏移。父回归
`smoke_weather_jammer_interrupt_save.json` 24 条、`smoke_ice_wall_engineer.json` 129 条均通过。
新增 `adapting-classic-reanimation` 技能，`adding-plant`/`adding-zombie` 同步中断合同，三者均通过
skill-creator `quick_validate.py`。
