# 警铃草实现计划

1. 在稳定植物与动画枚举末尾追加 `PLANT_ALARMBELLFLOWER` / `ANIM_ALARMBELLFLOWER`，注册独立 `AlarmBellFlower`、gamedata、图鉴、AutoTest 名称与 7-6 奖励。
2. 实现首个正式逻辑步的同行候选收集：按最短正余时、再按稳定僵尸 ID 选唯一目标，只调用一次通用中断接口且不造成伤害。
3. 让冰墙工程师接入现有 `GetInterruptibleSpecialActionRemaining` / `InterruptUncommittedSpecialAction` 契约；外部中断拆除半墙但不消费能力，死亡、断头、魅惑等终止路径仍永久消费。
4. 保存警铃是否触发、成功结果与 1 秒演出余时；读档只恢复三态表现，不重播声音、粒子或中断动作。
5. 使用非写实手绘卡通分件母图，确定性生成 120px 透明轴心分件、0.8 内容比例卡图、复用 Blover 的 reanim 与金青整行脉冲贴图；固定叶座保留原版双段茎，铃舌跟随铃头，并注册粒子 XML 和全部资源键。
6. 扩展状态投影和专项脚本，覆盖资源、数值、单行最短余时与 ID 并列、气象干扰/钻机/工程师、无候选、零伤害、快照和 7-6 奖励。
7. 使用 `clang-release` 构建，在当前桌面可见运行专项、气象干扰、钻机、工程师、伏霜雷和冒险奖励回归；检查退出码、日志、状态 JSON，以及默认实例和 `-NoInstance` 截图。
8. 更新第七大关内容记忆与相关植物、僵尸、粒子技能，运行 skill-creator 校验，提交并在安全 fast-forward 条件下推送。
