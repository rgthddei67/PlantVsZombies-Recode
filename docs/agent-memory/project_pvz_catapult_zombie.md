---
name: project-pvz-catapult-zombie
description: 经典投篮车的十二发篮球、车辆碾压/爆胎/死亡、专属化灰、存档及屋顶 5-5/5-6 出怪契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-08
---

# 经典投篮车僵尸

2026-08-08 完成 `ZOMBIE_CATAPULT`：850 点整车本体生命、出生随机 23～37px/s 基础车速、无断臂断头、不可魅惑/水草抓取，水池正式刷新只允许陆路。车辆没有 `_ground` 根运动，手动 X 位移但继续消费能力、寒冰、雨势、台风与场地倍率；每帧以独立攻击框压扁同排合格植物。

投篮状态机为 `WALKING/SHOOTING/RELOADING/CALTROP_DYING`。初始 12 球，`anim_shoot` 第 46 帧发射 75 伤篮球，固定飞行 1.2 秒、拱高 210px，片段结束扣库存；篮筐继续只显示既有四球，库存大于等于 5 时全显，进入 4/3/2/1 后逐个减少。有球时装填 3 秒，无球后永久前进。篮球走已有 Bullet 对象池但只碰植物，按同格 overlay/normal/pumpkin/under 重新解析目标层。`Bullet::HandlePlantContact` 在实际扣血前按目标逻辑格查询空中防御保护者；叶子保护伞展开中的篮球由 `onTriggerStay` 等待，展开后正式反弹并生成 `UmbrellaReflect`，保护范围外仍走原 75 伤入口。

两段损坏切换侧板/投臂贴图并补烟，低于 200 HP 概率自损。地刺通过 `Zombie::HandleCaltropHit` 通用虚入口派发：投篮车消耗地刺、播放爆胎和 `anim_bounce`，2.8 秒后 `CatapultExplosion`；普通/鎏金冰车保持原语义。普通死亡立即爆炸，灰烬死亡只创建 `Catapult_Charred` 并在主人指定的第 29 帧移除，不叠普通爆炸。phase、计时、库存、随机车速、目标、烟雾与爆胎终态全部进入 `extraData`，装填中快照往返不重放声音或多发球。

主人可见目验后的最终坐标：gamedata offset 为 `[-7,-108]`（较初版整体下移 18px）；篮球起点相对稳定视觉原点为 `[+100,-3]`（较初版向车尾回收 30px）。黄油不再退回车头逻辑原点，改为跟随专属 `Zombie_catapult_driver_head` 司机头轨；冻结冰晶底边中心锚到稳定视觉原点 `[+95,+143]`，其中 Y 保持普通脚底高度，X 位于两轮之间的整车视觉中央。`Zombie` 基类为两种状态贴图提供独立虚锚点，普通僵尸仍保持 `anim_head1`/逻辑脚底原公式。碰撞框、碾压框、烟雾、爆炸和化灰都从 `Transform + mVisualOffset` 派生，整车上下调只改 gamedata，不分别补偿。资源为 `CatapultZombie`、`Catapult_Charred`、`Zombie_catapult_basketball.png`、`basketball.ogg` 与六发射器 `CatapultExplosion.xml`；运行时断言覆盖 reanim、篮球和三张损坏材质实际键。

冒险保留既有 5-3（level 39）普通、路障、撑杆、加强铁门、扶梯和 5-4（level 40）普通、路障、小丑、蹦极、气球、普通/精英扶梯阵容。新增 5-5（level 41，25 波/300 阳光）为普通、路障、铁桶、投篮车的精简教学；新增 5-6（level 42，25 波/700 阳光）为普通、路障、铁桶、蹦极、扶梯、投篮车的复习综合，承接 5-5 通关后获得的叶子保护伞。最终 `smoke_catapult_spawnlists.json` 在主人当前桌面可见运行 69 条命令、exit 0，覆盖 5-1～5-6 完整数组、屋顶行兼容，`run.log` 以 `script finished OK` 结束；5-5/5-6 选卡预览截图均已检查且无越界。

最终验证证据：`smoke_catapult_zombie.json` 在主人当前桌面可见运行 143 条命令、exit 0，覆盖第 46 帧前后、初始 12/首发后 11、十二发共 900 伤与耗尽、装填快照、碾压、两段损坏、不可魅惑、普通爆炸、化灰第 29 帧、爆胎延迟爆炸、爆胎阶段停止碾压，以及司机头黄油与中央冰晶的相对锚点；11 张同步截图均已检查。2026-08-08 接入叶子保护伞后再次可见运行同一 143 条命令，exit 0、`script finished OK`，篮球既有弹道截图保持正常；新交互的范围内外、展开等待和反弹粒子另由 `smoke_umbrella_leaf.json` 覆盖。共享父回归 `smoke_kernelpult.json` 184 条命令、`smoke_iceshroom.json` 47 条命令同样可见 exit 0，普通僵尸的头部黄油与脚底冰晶截图保持原样；三份 `run.log` 都以 `script finished OK` 结束且无 FAIL/Fatal/WATCHDOG/资源缺失标记。最终 `clang-release` 配置、编译与增量链接均 exit 0；链接仍输出既有 vcpkg applocal 找不到 objdump 的非阻断提示。
