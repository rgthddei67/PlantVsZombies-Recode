---
name: project_pvz_marigold_minimal
description: 金盏花最小观赏版本：负100阳光、25秒冷却、无吐钱行为，以及资源键和双渲染路径验收契约
metadata:
  node_type: memory
  type: project
---

# 金盏花最小观赏版本

2026-08-09 完成。主人明确把 Marigold 暂定为纯观赏植物，未来有其他计划再扩展；当前不得从 C# 原版把吐钱状态机、金币、生产音效或帧事件带回来。

**玩法契约：**
- `Marigold : Plant` 不覆写行为，只播放基础 `anim_idle`，生命值沿用 300。
- `gamedata.json` 的费用为 `-100`、冷却为 `25.0s`；正式卡槽种下会因 `SubSun(-100)` 增加 100 阳光。
- 轻量推演只登记 `baseHealth:300`，`sunPerSecond` 保持 0，`persistent` 保持 true。
- 既有 `PLANT_MARIGOLD` 枚举、冒险解锁位和 AutoTest 名称表保持原位置；新 `ANIM_MARIGOLD` 追加在动画枚举末尾，避免移动旧整数值。

**资源契约：**
- reanim 注册键/值为 `REANIM_MARIGOLD = "Marigold"`，卡图键为 `IMAGE_MARIGOLD`。
- `Marigold_blink1/2.png` 没有被当前 reanim 的 `<i>` 直接引用，只能用全目录预加载产生的 `IMAGE_MARIGOLD_BLINK1/2`；头、嘴、眉毛和花瓣为时间线直引，可用 `IMAGE_REANIM_MARIGOLD_*`。不要把两类键混用。

**验证：**
- `clang-release` Release/LTO 构建退出 0，无编译器警告；vcpkg applocal 收尾仍提示 PATH 中缺 `dumpbin/objdump`，但静态链接 EXE 正常运行。
- 可见 `smoke_marigold.json -Seed 42` 默认与 `-NoInstance` 均退出 0；资源、`-100`、`25000ms`、真实卡槽阳光 `0→100`、25 秒冷却、快照往返及无持续产出断言通过。
- 单株默认 `INSTANCE` 与 `NO_INSTANCE` 的整数 `worldBounds` 同为 `left=418, top=100, right=470, bottom=172`；两条路径的单株和向日葵/小喷菇同排截图均已肉眼确认站位与影子正常。
