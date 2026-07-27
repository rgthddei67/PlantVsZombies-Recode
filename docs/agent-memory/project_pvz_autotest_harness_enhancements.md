---
name: project-pvz-autotest-harness-enhancements
description: AutoTest 同步截图屏障、显式跨场景状态复位与隔离正式存档快照往返
metadata:
  node_type: memory
  type: project
---

# AutoTest 基础设施增强

2026-07-27 为长脚本补齐三项基础契约：

- `screenshot` 使用 VulkanRenderer 单调 ticket；只有 `IMG_SavePNG` 成功后才发布完成，TestDriver 再校验文件存在且非空，随后才记录 `done`。截图后可无等待直接 `goto_level`。
- `reset_test_state` 显式恢复倍速和三项开发者覆盖；`goto_level.resetTestState=true` 复用同一函数，缺省保持旧脚本语义。`timeScaleOn1000` 是稳定断言抓手。
- `save_level_snapshot` / `reload_level_snapshot` 只接受安全短名，路径固定为当前脚本输出目录的 `snapshots/<name>.json`。保存与加载共用正式 `GameInfoSaver` 字段实现；重载销毁旧 GameScene，在新场景正常加载阶段消费一次性路径，成功或失败后都不污染后续 `goto_level`。禁止通过关闭 AutoTest 模式绕过保护。

动画子弹正式存档同时保存不可变对象池槽位 `poolType` 与可变当前类型；读档按槽位类型从池中取得对象，再恢复表现。AutoTest bullet JSON 导出只读 `fromPool` / `poolType`。

验证脚本 `smoke_autotest_harness.json` 覆盖草地→泳池无等待截图、显式状态复位、火炬树桩动画火球快照往返；`smoke_screenshot.json` 与 `smoke_torchwood.json` 同步回归。
