---
name: project_pvz_alarm_bell_flower
description: 第七大关警铃草、同行未提交动作中断、三叶草骨架适配、存档与验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-27
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

场上动画完整复用原版 `Blover.reanim` 的 12 FPS 时间轴和全部头、茎、叶片、地面分件：
待机 0～32、弯折 33～51、余韵循环 52～61，`anim_blow` 与返回段均以 2.0 倍播放。
七类原版分件只做确定性的冬季青蓝换色，不再用大型 AI 铃头替换核心头身；现有 AI 铃图只裁成
24×24 小挂件，以命名 follower 稳定挂在 `Blover_stem1`，随父轨完整仿射运动并继承整株淡出。
最后 0.22 秒整株淡出。

## 资源闭环

`scripts/generate_alarm_bell_flower_assets.ps1` 同时锁定原版 Blover 时间轴、卡图、七类分件、
既有警铃母图和全部输出 SHA-256。脚本只对绿色像素做青蓝 HSV 派生，保留眼睛、描边和暖色细节；
卡图由原版三叶草同步换色并叠加同一小铃，场上与卡图共享身份。旧的大铃头、铃舌和叶座运行图
已删除，不再参与资源注册或运行时换图。

资源必须同时闭合 `resources.xml` 的 GameImages/ParticleTextures/Reanimation、`ResourceKeys`、
manifest，以及 AutoTest 的 `HasReanimation` 和每张运行时 `GetTexture(key,false)` 断言。
警铃动画没有新增帧事件；玩法仍在首个正式逻辑步立即提交。

## 验证入口

核心专项是 `smoke_alarm_bell_flower.json`，覆盖数值、资源、同行选择、稳定 ID、异排、
无伤害、气象干扰重启、钻机重置、施工墙撤销、演出中快照与 7-6 奖励。
`visual_alarm_bell_flower_transition.json` 以 0.35 倍速分别截取原版弯折、表情换图、两个连续
旋转放大帧、余韵和淡出，专门检查完整三叶草动作、小铃跟随和循环连续性。最终证据须使用
`clang-release` 在默认实例路径与 `-NoInstance` 当前桌面可见运行。

2026-08-27 玩家反馈视觉重制证据：资源生成器复跑且新旧输入与全部输出哈希闭合；`clang-release`
全量 LTO 构建通过，主程序 Win7 导入审计通过 378 项，CTest 三项为 3/3。`smoke_alarm_bell_flower.json` 扩至
121 条命令，默认 Vulkan 与 `-NoInstance` 均执行至 command 120、exit 0、`status=passed`、
`script finished OK`，新增断言确认七类换色分件、小铃资源及运行期 follower 可见。
`visual_alarm_bell_flower_transition.json` 两路径均执行 30 条命令至 command 29 并通过；八帧慢放
逐张目验原版三叶草弯折、表情切换、旋转放大、青蓝轮廓、小铃跟随与淡出连续，两条绘制路径一致。

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
