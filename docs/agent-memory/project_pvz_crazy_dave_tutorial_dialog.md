# 疯狂戴夫关卡闲聊与隐性机制提示

## 2026-08-24 首次接入

冒险模式在全新进入指定关卡的选卡阶段前，可由 `CrazyDaveDialog` 播放一次原版风格的
模态闲聊。当前覆盖 2-1、4-1、4-2、4-9、5-1、6-1、6-9 和 7-1；按主人要求不在
1-1、3-1出现，也不逐个解释僵尸。文字只借天气、迷雾、路灯花燃料、屋顶坡面与径流、
黑夜屋顶雷荷、动态雾和寒潮剧情给出含蓄提示。

对话使用 `CrazyDave.reanim` 的进入、待机、说话、疯狂与离开轨道，不新增动画帧事件。
点击、Enter 或空格继续，Esc 跳过；活动期间 `GameScene` 暂停场景和 Board 输入，最后一次
点击不会穿透到选卡界面。只在完整看完或主动跳过后立即记录为已读，场景中断不记录；读档
恢复、AutoTest 普通进关、生存模式和已经看过的关卡不会自动弹出。

玩家 schema v5 新增排序去重的 `crazyDaveTutorialsSeen` 冒险关卡号数组，v4 及更早迁移为空。
AutoTest 提供 `show_crazy_dave_dialog`、`advance_crazy_dave_dialog`、
`skip_crazy_dave_dialog`，专项锁定全部台词、支持关卡、完成/跳过记录、资源数量、绘制几何及
截图。

## 资源与渲染契约

`CrazyDave` 资源从 `resources.xml` 注册；运行前必须同时取得 reanimation 和它引用的 32 个
图片键。原版 `CrazyDave_body1.jpg` 以同尺寸 `CrazyDave_body1_.png` 灰度图提供透明遮罩。
`ResourceManager` 现把通用 `Name.jpg + Name_.png` 识别为颜色图与 alpha 遮罩组合：遮罩亮度
写入颜色图 alpha；遮罩缺失时仍是普通 JPG，遮罩存在但损坏或尺寸不符则加载失败。不能用
空 Animator、空纹理或把黑底直接当身体像素来兜底。reanim 的 PNG/JPG 回退只在候选全部
失败时记录 ERROR，JPG 正常命中不能先留下一个误导性的 PNG 缺失错误。

## 验证

- `clang-debug` 编译及 `save-schema` 1/1 通过；可见专项全部通过。
- `clang-release` 完整配置、编译及 Win7 导入审计通过，`save-schema` 1/1 通过。
- 可见 `clang-release` 专项在默认 Vulkan、Vulkan `-NoInstance` 和 OpenGL 3.3 三条路径均
  117 条命令通过；活动画面分别确认实例路径为 true/false/false，截图确认戴夫透明身体、
  对话框、中文换行和全部关卡布局正常。
- Debug 与 Release 的启动输出均确认 `[ERROR]` 为 0，且不再出现误探测的
  `CRAZYDAVE_BODY1.png` 路径。
