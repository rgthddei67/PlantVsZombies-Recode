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

## 2026-07-29 水格外观与同格植物清除

- Crater 现在每帧按**当前格子地形**选择外观：泳池背景的陆地行继续使用普通昼/夜 `crater`，两条水路使用 `crater_water_day/night`；四套资源均按 `PART_0=完整、PART_1=后半程消退` 消费。水格弹坑与水面植物共用 2px 振幅及 row/column 相位的两秒浮动周期。
- 屋顶左右两套资源和 `ResourceKeys` 已入库但暂不接运行逻辑；`Crater.cpp` 留有 `TODO(roof)`。白天水格分支已于 2026-08-04 通过咖啡豆唤醒毁灭菇完成端到端覆盖，详见 [project_pvz_coffeebean](project_pvz_coffeebean.md)。
- 毁灭菇引爆前按 EntityManager 全部植物 ID 快照过滤同一逻辑 `row/column`，排除自身后逐株 `Die()`；因此同格睡莲也会死亡，邻格组合不受影响。不能只读取当前 Cell 的 under/normal 两层，否则未来南瓜等额外层会漏结算。
- 新增 AutoTest `add_crater` 命令和 `craters.N.textureKey/textureLoaded` 抓手。`smoke_crater_terrain_visuals` 覆盖昼/夜泳池的陆地、水格完整、水格消退；`smoke_doomshroom_same_cell_plants` 覆盖同格睡莲死亡、邻格“睡莲+小喷菇”保留和水格弹坑。旧 `smoke_doomshroom` 的坐标 click 对照存在假绿风险，已改为 `assert_can_plant` 对弹坑格 false/旁格 true 的正式入口断言，再直接种旁格对照。
- 当前 `clang-playtest` 配置/全量构建和后续增量构建均退出码 0、无警告；上述三个脚本均在当前桌面可见运行、窗口标题确认且退出码 0，状态 JSON、run.log 与截图一致。

关联 [project_pvz_icefumeshroom](project_pvz_icefumeshroom.md)（前一株植物+adding-particle skill 起源）、[project_pvz_iceshroom_freeze](project_pvz_iceshroom_freeze.md)（全场结算先例）、[feedback_frame_event_numbering](feedback_frame_event_numbering.md)。

## 2026-08-04 咖啡豆唤醒入口

`DoomShroom::StartCharging()` 现在是夜间种下与咖啡豆唤醒共用的单一充能入口；白天仍保持
`anim_sleep`，目标唤醒倒计时归零后才播放 reverse explosion 并进入 `anim_explode`。日间泳池
专项在睡莲上唤醒毁灭菇，随后确认同格睡莲与咖啡豆层均清空并生成水上白天弹坑；原有夜间充能、
白天沉睡和同格睡莲清除两条专项继续可见退出 0。
