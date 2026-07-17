---
name: project-pvz-trophy-decouple-coin
description: "2026-07-06 Trophy两步下沉Coin→AnimatedObject(0496138)→GameObject(e644cd8,均合master未push)；胜利路径冒烟smoke_trophy_win.json用click target=trophy"
metadata:
  node_type: memory
  type: project
  originSessionId: b2c74a12-8f67-4620-9c95-7fc3f5d95918
---

2026-07-06（0496138 + e644cd8，合 master 未 push）：Trophy 两步下沉 Coin→AnimatedObject→**GameObject**（主人追问「用不到 anim 为何不直接 GameObject」——正确，ANIM_NONE 等于对基类说不要它唯一提供的东西）。自建 Transform/Collider/mBoard 脚手架；Board 持 `weak_ptr<Trophy> mTrophy` 供存档定位（不进金币表），CreateTrophyWithID/COIN_TROPHY 已删；dump_state 新增 `trophy` 字段（null 或 {x,y}）；click 命令新增 `"target":"trophy"` 执行时实时解析坐标。

**Why:** Trophy 原本绕过 Coin 全部核心行为（跳过 Coin::Start、不飞向计数器、999s 消失永不触发），只借入场缩放动画；对 AnimatedObject 也只借脚手架。

**How to apply / foot-gun:**
- 启动时的 `cannot load animation for type 0` 噪音已随下沉到 GameObject **根除**；若再出现说明有新的 ANIM_NONE AnimatedObject。
- **AutoTest 静态 click 坐标会脱靶**：僵尸死亡位置受帧时序影响（-Seed 只固定随机数，不固定墙钟帧步长），同 seed 两次跑奖杯 x 可漂移——已实测翻车，动态目标一律用 `"target":"trophy"` 模式（要点新目标就仿此扩展）。
- `TransformComponent::SetScale(float&)` 收非常量引用，constexpr 常量传不进去（clang 报 drops const），要经局部变量/包装函数。
- 存档路径 AutoTest 测不到（短路存档），读档走 CreateTrophy，旧存档 "id" 字段忽略（兼容）。
- PS 5.1 here-string 提交信息内含英文双引号会被 native 参数传递拆碎（pathspec 报错）——改中文引号。

关联 [project_pvz_autotest_assert_state_todo](project_pvz_autotest_assert_state_todo.md)
