---
name: project-pvz-catapult-zombie
description: 经典投篮车的六发篮球、车辆碾压/爆胎/死亡、专属化灰、存档及屋顶 5-5/5-6 出怪契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-08
---

# 经典投篮车僵尸

2026-08-08 完成 `ZOMBIE_CATAPULT`：850 点整车本体生命、出生随机 23～37px/s 基础车速、无断臂断头、不可魅惑/水草抓取，水池正式刷新只允许陆路。车辆没有 `_ground` 根运动，手动 X 位移但继续消费能力、寒冰、雨势、台风与场地倍率；每帧以独立攻击框压扁同排合格植物。

投篮状态机为 `WALKING/SHOOTING/RELOADING/CALTROP_DYING`。初始 6 球，`anim_shoot` 第 46 帧发射 75 伤篮球，固定飞行 1.2 秒、拱高 210px，片段结束扣库存并按剩余数量隐藏篮筐球；有球时装填 3 秒，无球后永久前进。篮球走已有 Bullet 对象池但只碰植物，按同格 overlay/normal/pumpkin/under 重新解析目标层。`Bullet::HandlePlantContact` 的实际扣血前保留 `TODO(叶子保护伞)`，未来拦截只接管这一入口。

两段损坏切换侧板/投臂贴图并补烟，低于 200 HP 概率自损。地刺通过 `Zombie::HandleCaltropHit` 通用虚入口派发：投篮车消耗地刺、播放爆胎和 `anim_bounce`，2.8 秒后 `CatapultExplosion`；普通/鎏金冰车保持原语义。普通死亡立即爆炸，灰烬死亡只创建 `Catapult_Charred` 并在主人指定的第 29 帧移除，不叠普通爆炸。phase、计时、库存、随机车速、目标、烟雾与爆胎终态全部进入 `extraData`，装填中快照往返不重放声音或多发球。

主人可见目验后的最终坐标：gamedata offset 为 `[-7,-108]`（较初版整体下移 18px）；篮球起点相对稳定视觉原点为 `[+100,-3]`（较初版向车尾回收 30px）。碰撞框、碾压框、烟雾、爆炸和化灰都从 `Transform + mVisualOffset` 派生，整车上下调只改 gamedata，不分别补偿。资源为 `CatapultZombie`、`Catapult_Charred`、`Zombie_catapult_basketball.png`、`basketball.ogg` 与六发射器 `CatapultExplosion.xml`；运行时断言覆盖 reanim、篮球和三张损坏材质实际键。

冒险保留既有 5-3（level 39）普通、路障、撑杆、加强铁门、扶梯和 5-4（level 40）普通、路障、小丑、蹦极、气球、普通/精英扶梯阵容。新增 5-5（level 41，25 波/300 阳光）为普通、路障、铁桶、投篮车的精简教学；新增 5-6（level 42，25 波/700 阳光）为普通、路障、铁桶、蹦极、扶梯、投篮车的复习综合，承接 5-5 通关后获得的叶子保护伞。最终 `smoke_catapult_spawnlists.json` 在主人当前桌面可见运行 69 条命令、exit 0，覆盖 5-1～5-6 完整数组、屋顶行兼容，`run.log` 以 `script finished OK` 结束；5-5/5-6 选卡预览截图均已检查且无越界。

验证证据：`clang-release` 编译并链接成功；可见 `smoke_catapult_zombie.json` 118 条命令 exit 0，覆盖第 46 帧前后、75 伤、六发耗尽、装填快照、碾压、两段损坏、不可魅惑、普通爆炸、化灰第 29 帧、爆胎延迟爆炸及爆胎阶段停止碾压，9 张同步截图已检查。共享回归 `smoke_caltrop.json`、`smoke_cabbagepult.json`、`smoke_kernelpult.json` 均可见 exit 0；地刺脚本同时修正了与当前屋顶正式规则冲突的陈旧断言，屋顶地刺应不可直接种植。
