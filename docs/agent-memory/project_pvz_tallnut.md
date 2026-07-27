---
name: project_pvz_tallnut
description: 2026-07-27 经典高坚果、撑杆与海豚跳跃阻拦、坚果家族啃食碎屑和裂纹存档恢复
metadata:
  node_type: memory
  type: project
---

# 经典高坚果

## 当前实现

`PLANT_TALLNUT` 已注册为 `TallNut : WallNut`，使用主人导入的 `Tallnut.reanim` 与卡图。基础生命
8000，阳光 125，冷却 30 秒；普通坚果保持 4000 生命。两者共用按生命值派生的三阶段材质：
高于 2/3 为完整体，不高于 2/3 与 1/3 时分别切换 `cracked1/2`。首次跨入裂纹阶段喷出
`WallnutEatLarge`；快照读回仅重建材质和阶段，不重放碎屑。

高坚果卡图在 `CardDisplayComponent` 的通用 `0.64` 绘制倍率之上再乘 `0.70`，保持既有卡图
矩形中心后再上移 5px，避开底部阳光文字；该倍率只影响卡槽和选卡界面，不改变草坪本体。

`image/reanim/` 的目录预加载键为 `IMAGE_<文件名大写>`；只有被 reanim XML 直接引用的图片才会
额外以 `IMAGE_REANIM_*` 键加载。`Tallnut_cracked1/2.png` 未被 `Tallnut.reanim` 正文引用，
动态换图必须使用 `IMAGE_TALLNUT_CRACKED1/2`。`WallNut` 的裂纹图同理。

僵尸每次真正执行植物啃食伤害前调用 `Plant::OnZombieBite`。普通植物默认无反馈，`WallNut`
及高坚果喷出 `WallnutEatSmall`，因此不需要在僵尸侧维护植物类型表。碎屑条在
`resources.xml` 以 `Column=5` 拆为五张静态纹理并随机取样，不能作为 `ImageFrames` 循环动画。

## 跳跃阻拦

高坚果通过 `BlocksZombieJump` 阻挡 `POLEVAULT` 与 `DOLPHIN_RIDER`，每次确实阻拦时由植物侧
统一播放 `SOUND_BONK` 并喷出 `TallNutBlock` 星星。

- 普通撑杆在接触高坚果时弃杆、进入 `WALKING` 并开始啃食，实际跳距保持 0。
- 精英撑杆仍先在同排同 X 召唤普通撑杆，再承受绕过僵尸减伤与免伤次数的固定 800 本体伤害。当前基础
  生命 450，因此会进入 `anim_death`；派生阻拦代价必须在 `StartEat` 之后结算，防止死亡动画
  被啃食轨道覆盖。召唤在伤害前完成，保证致死也不吞掉普通撑杆。
- 海豚在跳跃进度 30% 的既有唯一判定点被挡，弃豚回到 `SWIMMING`；Bonk 从原来的僵尸内部
  分支收敛到植物反馈入口，避免两处重复播放。

## 验证

2026-07-27 `clang-playtest` 构建成功且无警告。以下脚本均从 `build/clang-playtest/` 在主人
当前桌面的“植物大战僵尸中文版”可见窗口运行，退出码 0，`run.log` 以
`script finished OK` 结束：

- `smoke_tallnut.json`：8000 生命、两档裂纹与大碎屑、快照恢复不重放、普通撑杆零跳距、
  Bonk/星星、啃食小碎屑。
- `smoke_tallnut_elite_pole.json`：叠满僵尸减伤并赋予 8 次免伤后，精英被挡仍生命归零并进入
  `anim_death`，8 次免伤未被消费，普通撑杆仍存在。
- `smoke_tallnut_dolphin.json`：海豚由 `JUMPING` 被挡回 `SWIMMING`，弃豚，Bonk 与星星各一次。
- `smoke_wallnut_chew_particles.json`：普通坚果受一次 50 伤，啃食者计数 1，小碎屑有实际
  render quad 且与坚果/僵尸碰撞区相交。

水路高坚果与荷叶同格时，粒子探针的 `nearestPlant.type` 可能命中下层荷叶；这类场景应断言
稳定的 `row/col`，不能把最近几何对象类型当成触发者身份。
