---
name: project-pvz-fixed-timestep
description: 2026-07-06 固定步长主循环(逻辑60Hz)落地——AutoTest 同 Seed 逐字节复现；核心 foot-gun=追帧步间必须补事件 poll 否则合成输入按下沿湮灭
metadata:
  node_type: memory
  type: project
  originSessionId: 704ffb39-f43c-4277-8498-172fb33302e7
---

# 固定步长主循环 ✅合master未push (729356e)

2026-07-06 起源于主人与台湾大佬的游戏循环之争（QUESITON.md）。裁决：不分线程（本项目实测 Update 侧 CPU-bound、Present 0.14ms，分线程无收益）；变步长 dt×speed 本身不是"错误算法"；真正值钱的是**确定性**→ 落地 loop 层固定步长。

## 终态设计
- `DeltaTime.h`：`BeginFrame()` 只累积墙钟并返回本渲染帧逻辑步数（0..3，`kMaxCatchUpSteps=3` 封顶，超出丢债=过载慢动作退化，替代旧 `maxDeltaTime=0.1` clamp）；`BeginStep()` 每步装填 `dt = 1/60 × timeScale`（暂停=0 但**步进不冻结**，菜单仍需 Update 消费点击）。全仓库几百个 `GetDeltaTime()` 调用点零改动。
- `GameApp.cpp` 主循环：Update 段变步循环；`InputHandler::Update()`（边沿衰减 PRESSED→DOWN）从帧末移到**每逻辑步末尾**——一次点击恰好被一个逻辑步消费，追帧不会种两棵；0 步帧边沿保留不丢。
- `SetMax/MinDeltaTime` 已退役删除（无外部调用者）。

## 核心 foot-gun（调了一轮才发现）
**追帧的第2/3步之前必须补一次 SDL poll**（恢复不变量"每逻辑步前必有 poll"）。否则：TestDriver `key`/`click` 跨步状态机在同一渲染帧内连推 KEYDOWN+KEYUP → 下帧同批 poll 中 PRESSED 被 KEYUP 直接改写成 RELEASED，**按下沿未被任何逻辑步观察即湮灭** → `IsKeyPressed` 热键永不触发。症状=smoke_develop/smoke_dev_spawn_pause 的 rshift 面板打不开（截图里只有徽章没面板）。触发条件=截图等重命令耗时>33ms 造成下帧追 2-3 步。

## 验证方法（可复用）
同 `-Seed 42` 双跑 demo_peashooter，diff 全部产物：run.log(帧号)/state.json/3张PNG **逐字节一致**。根治 [project_pvz_trophy_decouple_coin](project_pvz_trophy_decouple_coin.md) 里"-Seed 不固定帧时序、静态 click 坐标脱靶"的 foot-gun。
教训：验证脚本要确认真的比了东西——smoke_gameplay 只产 run.log，state.json 对比是空对空假阳性；选产物最全的脚本做确定性对照。

## 语义变化备忘
- `wait_frames`/`hold_frames` 的"帧"=逻辑步（60Hz），不再随渲染帧率漂移。
- AutoTest 看门狗 `GetUnscaledDeltaTime` 从墙钟变逻辑步计时（过载丢债时看门狗同步变慢）——对确定性更好，但"墙钟15s"注释已不精确。
- 渲染不插值：逻辑60Hz+高刷屏会有轻微顿感；建议搭配 vsync 60。若做插值需双缓冲 Transform，改动面大，暂缓。
