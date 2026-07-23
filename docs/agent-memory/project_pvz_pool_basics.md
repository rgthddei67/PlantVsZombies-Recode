---
name: project_pvz_pool_basics
description: 2026-07-23 第三大关泳池基础；当前只实现 3-1/3-2，含六行、睡莲双层占格、地形僵尸替换、PoolCleaner 和旧档边界
metadata:
  node_type: memory
  type: project
---

# 第三大关泳池基础系统

## 当前范围（2026-07-23）

只开放冒险 3-1（level 19）和 3-2（level 20）的基础系统与出怪表。3-3 才引入的新僵尸、3-3～3-9 出怪表、其他泳池植物均未实现，后续必须逐关推进，不得将本基础当作整个第三大关已完成。

资源由主人提供：泳池地图、睡莲/三种水路僵尸动画、`PoolCleaner` 已在 `resources.xml` 注册。实现不新增原版入水水花或水面闪光，PoolCleaner reanim 自带轨道不在此限制内。

## 核心契约

- 仅第三大关 `Background::WATER_POOL` 相对通用背景基线额外上移 40px，六行网格首行顶部同步设为 Y=45；后续 `NIGHT_WATER_POOL` 不继承该实测偏移。泳池使用 6 行、85px 行高，0-based 第 2/3 行是水路，列宽仍为 80px。网格派生位置必须通过 `Board::GetCellCenterPosition/GetCellHeight`。
- `Cell` 有 `under/normal` 两个植物槽。睡莲只能放在空水格的 under 层；普通植物只能放在已有睡莲且 normal 层为空的水格；土豆雷仍禁止下水。铲子、僵尸啃咬与 UI 预览都以 top 层为准。
- 睡莲种下后 1 秒只免疫啃咬，计时器进存档；水面植物只做±2px 绘制浮动，Transform/碰撞与存档仍固定在逻辑格。
- 普通/路障/铁桶僵尸在水路由正式波次入口替换为 `ZOMBIE_POOL_*`，而非为出怪表添加独立权重。特殊陆地僵尸不进水路。水路僵尸使用前/后双探针进水；本项目将下水/出水切换位置相对原探针向右校正 70px。水中断头/断臂不生成悬浮的陆地碎片，死亡轨道分陆地/水中。
- 所有僵尸生成 Y 由网格行中心派生。第一、二大关继续使用已确认正确的 `kZombieSpawnBaseOffsetY=+2px`；泳池动画叠加主人目测确认的 `kPoolZombieSpawnYOffset=50px`，只有第三大关再应用 `kThirdAreaZombieAlignmentOffsetY=-10px`，不影响旧地图或后续泳池地图。三者均集中在 `Board.cpp` 顶部，第三大关网格整体移动时生成 Y 会自动随行中心移动。
- `anim_eat` 会重新显露双腿并把 `Zombie_duckytube` 换回无水面接触纹理的普通贴图；泳池僵尸在水中进入啃食态后必须隐藏 inner/outer leg 的 upper/lower/foot 六条轨道，并把泳圈覆盖为 `IMAGE_REANIM_ZOMBIE_DUCKYTUBE_INWATER` 保留涟漪，停止啃食和读档恢复时对称重建/撤销覆盖。
- 台风对同格睡莲+上层植物必须整叠搬运/丢失，不允许拆叠。
- `PoolCleaner` 使用 `LAND/ENTERING/IN_POOL/EXITING` 高度状态；只改绘制偏移，不改行与碰撞。陆地/水中吞噬分别用 `anim_landsuck/anim_suck`，稳态用 `anim_land/anim_water`，运动速度按 C# 水车:草车 `2.5:3.33` 换算。
- level 19～27 存档写入 `poolGridVersion=1`。缺字段的旧五行/单植物槽存档保留文件但拒绝读取，让关卡重新开始。
- 3-1 为 10 波 `{normal, cone}`；3-2 为 15 波 `{normal, cone, bucket}`。水路替换在选行后发生，成本/权重仍使用出怪表中的基础类型。

## 验证

`smoke_pool_basics.json` 覆盖 3-1/3-2 背景、六行、出怪表、睡莲分层/保护/禁种、天气、台风整叠搬运、地形僵尸替换与水中死亡。`smoke_pool_cleaner.json` 覆盖待机、启动、入水、水中、出水和回到陆地。`smoke_pool_zombie_visuals.json` 在 x=930 断言向右校正后的入水切换、泳池第 3 行生成 Y≈300，并断言水中 `anim_eat` 时六条腿部轨道全隐藏，同时输出背景/网格/僵尸 Y 对齐截图。2026-07-23 三脚本在 `clang-playtest` 当前桌面可见运行均退出 0，`smoke_night_rain` 回归与 `save-migration` CTest 同样通过。
