---
name: project_pvz_doomshroom_crater
description: 毁灭菇+弹坑+粒子ImageFrames实装(3a79e9e..db73b07未push)；主人已验收；两个skill已同步
metadata:
  node_type: memory
  type: project
  originSessionId: 86284160-c1ea-44cc-91e7-b2c880d86a4d
---

2026-07-16 (3a79e9e+3918192+db73b07, 均未push) 毁灭菇 DoomShroom 全套完成并经主人验收（存档/影子/预览/无敌四轮反馈均已修）：植物类/弹坑 Crater/粒子引擎 ImageFrames 消费端/Doom.xml 移植。**skill 已同步 (db73b07)**：adding-particle（ImageFrames 移入标签表 + "原版 XML 移植口径"配方）、adding-plant（格子占用系统/引爆无敌/全局帧号核对/resources.xml 陈旧检查，含主人手补 anim_xxx 注释）。

- **数值（主人未裁定=默认忠实原版）**：125 阳光/50s 冷却/1800 伤/半径 250 圆 vs 僵尸判定矩形/弹坑 180s；充能=anim_explode(全局 19..51) 按 23fps（PlayTrack clip=23/12，reanim 基础 12fps），帧 51 引爆（主人指定，末帧安全：普通前进与回绕分支都覆盖 51）。
- **弹坑架构**：Crater 轻量 GameObject（Trophy 先例），LAYER_GAME_OBJECT=0（背景-10000 之上、植物 10000 之下）；Board::AddCrater/HasCraterAt 持 weak_ptr 簿记；阻种闸门在 CardSlotManager::CanPlaceInCell（AutoTest `plant` op 直连 CreatePlant **绕过**此闸门，测阻种须走 click 真实路径+旁格对照防假阴）；存档 craters[{row,col,timeLeft}] 旧档兼容。
- **分份贴图机制**：resources.xml `<Image Column="2" Row="1">` 自动切成 `IMAGE_XXX_PART_0/1` 独立纹理（crater.png 列0白天/列1黑夜）——弹坑不需要引擎改动。
- **Doom.xml 移植口径**：原版时间单位=厘秒→秒（150→1.5）；EmitterOffset 减半（双倍生效 foot-gun）；SystemPosition 常量偏移折算进 EmitterOffset（同样减半）；FullScreen 紫闪=ParticleScale 4000 的 WhitePixel 等效（RGB 轨道 白→紫）；负数区间 [-300 -200] 须升序。特效名=首发射器 Name（"Doom"）。
- **foot-gun 实录**：①msvc-debug 的 resources.xml 整体陈旧（缺寒冰菇/舞王/铁门共 12 个文件条目）——同步资源时先 Compare-Object 两 preset 的 xml，别只 append；已整文件覆盖+补拷。②粒子图键前缀按 resources.xml 段落定：ParticleTextures→`PARTICLE_*`，GameImages→`IMAGE_*`，Doom.xml 里引用错前缀=粒子静默不生成。③帧事件是全局帧号跨轨道通用（mFrameEvents 只按 int 帧号），新植物选引爆帧须核对其他轨道窗口扫不到它。
- **已知小瑕疵（已报主人）**：夜间充能中存档→读档会重播一次 reverse_explosion 音效（SetupPlant 无条件播；视觉由 RestoreAnimState 正确接管）。
- 影子=scale 1.0/offset (2,30)（主人两轮校对：比小蘑菇系 0.6 大一档+右移）；**存读档主人已验证没问题**。
- 验收修正 (3918192)：①弹坑格悬停曾照常显示落点预览——落点预览的隐藏闸门在 `UpdatePlantPreviewPosition` 的 isOverCellWithPlant，与 CanPlaceInCell 是**两处独立口径**，加占格类系统两处都要改；②充能期间无敌=TakeDamage 覆写（参考樱桃炸弹只闪光），但睡觉分支放行 `Plant::TakeDamage`（白天=普通蘑菇）。

关联 [project_pvz_icefumeshroom](project_pvz_icefumeshroom.md)（前一株植物+adding-particle skill 起源）、[project_pvz_iceshroom_freeze](project_pvz_iceshroom_freeze.md)（全场结算先例）、[feedback_frame_event_numbering](feedback_frame_event_numbering.md)。
