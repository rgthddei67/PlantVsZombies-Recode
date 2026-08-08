---
name: project-pvz-umbrella-leaf
description: 经典叶子保护伞的九宫格空中防御、篮球反弹、蹦极弹回、存档与可见 AutoTest 契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-08
---

# 经典叶子保护伞

2026-08-08 完成 `PLANT_UMBRELLA`：100 阳光、7.5 秒冷却、300 生命，注册独立卡图与 `Umbrellaleaf.reanim`。待机为 `anim_idle`；首次空中威胁以原版 22 FPS 播放 `anim_block`，5 厘秒后由 `ACTIVATING` 进入 `REFLECTING`，包装轨结束自动回待机。全过程使用状态计时和一次性轨道，没有新增动画帧事件。半尺寸阴影偏移为 `(0,+27)`；草地、夜间和屋顶花盆截图已检查。

植物通过 `ProtectsCellFromAirborneThreat`/`ActivateAirborneDefense` 声明能力和阶段，`Board::FindAirborneThreatProtector` 按逻辑格查询并以稳定实体 ID 选择最早种下的一株，威胁侧不维护植物类型表。有效伞叶保护自身和周围八格；死亡、预览、压扁、已被蹦极抓取的伞叶不提供保护。

篮球先按原层级解析实际目标，再查询该格保护者。首次接触启动伞叶，`onTriggerEnter + onTriggerStay` 在保持重叠宽限期间等待 0.05 秒；正式展开后篮球消失、目标免伤，并播放 splat 与从当前弹心发射的 `UmbrellaReflect`。反弹粒子复用篮球贴图，用 Position X/Y 关键帧表达稳定向右上抛弧线，避免依赖本引擎不支持的原版 `LaunchAngle`。保护范围外仍造成 75 僵尸来源伤害。

蹦极在 `LandAtTarget` 原节点检查保护；命中保护格时同时播放伞声和 `boing`，清空目标植物并直接空手 `RISING`，不进入抓取。防御阶段与展开剩余时间进入植物 `extraData`；加载以通用 Animator 已恢复的轨道规范化状态，不重播声音或重新触发威胁。

验证：`clang-release` 构建 exit 0；主人当前桌面可见运行 `smoke_umbrella_leaf.json` 默认与 `-NoInstance` 两条路径均 101 条命令、exit 0、`script finished OK`，覆盖资源键、数值、范围外伤害、对角保护、展开等待、反弹粒子最终矩形、蹦极空手上升、快照无反馈重放、待机恢复与屋顶花盆。四张截图逐张检查。既有 `smoke_bungee_zombie.json` 104 条与 `smoke_catapult_zombie.json` 143 条同样可见 exit 0；链接保留既有 vcpkg applocal 找不到 objdump 的非阻断提示。

可复用契约：短展开保护能力由植物声明范围/阶段，Board 只稳定选择保护者；持续碰撞威胁必须同时接 Enter/Stay，让 ACTIVATING 保持威胁与目标、REFLECTING 再原子消费。加载只恢复终态，禁止重放实战反馈。
