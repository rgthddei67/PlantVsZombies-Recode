---
name: project-pvz-tanglekelp
description: "2026-07-24 缠绕水草：空水格直种、C# 99/51/21/0cs 抓取、前后层包裹、拖沉与存档"
metadata:
  node_type: memory
  type: project
---

# 缠绕水草（Tangle Kelp）

2026-07-24 新增 `PLANT_TANGLEKELP`：25 阳光、30 秒冷却，只能直接种在空水格，
占普通植物层；不需要睡莲，也不能叠在已有睡莲或其他植物上。植物不可被僵尸选为啃食目标，
没有独立陆地阴影。

## C# 行为与动画契约

抓取行为直接参考 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn`：

- 待机时检查同一水路与植物 80x80 近身矩形相交的目标，排除魅惑、无头、垂死、
  不在水中、已被另一株水草锁定及特殊不可命中阶段，并优先最靠近房屋者。
- 锁定后按剩余 `99 → 51 → 21 → 0cs` 的独立倒计时推进；51cs 停止目标啃食并开始
  以 100px/s 视觉下沉，21cs 播放末段水花和入水声，0cs 水草与目标一起死亡。
- 水草若在流程中被其他来源摧毁，已锁定目标也立即死亡；这要求 `Plant::Die()` 为虚函数，
  使基类承伤路径能分派到水草的死亡收尾。
- `anim_grab` 是纯视觉，不需要主人提供或批准新帧事件。资源为 12fps，按原版 24fps 以
  2 倍 clip 速度播放；`Layer 29` 和 `Layer 32` 分别构成后/前层，两份 Animator 在
  `Zombie::Draw` 中夹住僵尸本体，通用附着偏移为 `(-13, 15)`。

水草锁定关系由 Zombie 保存 `tangleKelpPlantID`、拖沉标志、下沉偏移和抓取动画帧；
植物保存状态、剩余时间和目标 ID。旧档缺字段时保持中立，抓取中的读档会重建两份
`anim_grab`，失去植物的孤儿锁定会在下一帧清理。

撑杆僵尸仅 `JUMPING` 阶段不可抓，伴舞僵尸仅 `RISING` 阶段不可抓。大嘴花不会争抢
已经被水草锁定的目标；土豆雷不会由该目标主动触发，但由其他目标引爆的范围伤害仍可波及。

## 验证证据

`clang-playtest` 与启用 LTO 的 `clang-release` 均零警告编译通过，
`ctest --test-dir build/clang-playtest` 的
`save-migration` 通过。`smoke_tanglekelp.json` 在主人当前桌面可见运行，窗口标题为
“植物大战僵尸中文版”、退出码 0，`run.log` 记录 74 条命令全部完成，覆盖：

- 陆地拒种、空水格直种、睡莲格拒绝叠种及普通层占格。
- 锁定、前后层 `anim_grab`、51cs 拖沉、停止啃食和最终双方清理。
- 大雨 1.2 倍植物行动倍率只推进抓取倒计时；目标即使冻结仍按节点拖沉。
- 魅惑目标豁免，以及抓取途中水草被摧毁时目标同步死亡。

截图 `tanglekelp_water_placement.png`、`tanglekelp_grab_wrap.png` 和
`tanglekelp_drag_under.png` 和 `tanglekelp_heavy_rain_frozen_drag.png` 已人工检查：
待机水位、包裹层次、同步下沉及大雨冻结叠加表现均正常。
设计文档为 `docs/superpowers/specs/2026-07-24-tanglekelp-design.md`。
