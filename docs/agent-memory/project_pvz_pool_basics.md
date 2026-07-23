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

- 泳池背景使用 6 行、85px 行高，0-based 第 2/3 行是水路；列宽仍为 80px。网格派生位置必须通过 `Board::GetCellCenterPosition/GetCellHeight`。
- `Cell` 有 `under/normal` 两个植物槽。睡莲只能放在空水格的 under 层；普通植物只能放在已有睡莲且 normal 层为空的水格；土豆雷仍禁止下水。铲子、僵尸啃咬与 UI 预览都以 top 层为准。
- 睡莲种下后 1 秒只免疫啃咬，计时器进存档；水面植物只做±2px 绘制浮动，Transform/碰撞与存档仍固定在逻辑格。
- 普通/路障/铁桶僵尸在水路由正式波次入口替换为 `ZOMBIE_POOL_*`，而非为出怪表添加独立权重。特殊陆地僵尸不进水路。水路僵尸使用前/后双探针进水，水中断头/断臂不生成悬浮的陆地碎片，死亡轨道分陆地/水中。
- 泳池僵尸生成基线的集中调参点是 `Board.cpp` 顶部 `kPoolZombieSpawnYOffset`，当前按主人目测定为向下 20px，仅影响日/夜泳池背景。
- 台风对同格睡莲+上层植物必须整叠搬运/丢失，不允许拆叠。
- `PoolCleaner` 使用 `LAND/ENTERING/IN_POOL/EXITING` 高度状态；只改绘制偏移，不改行与碰撞。陆地/水中吞噬分别用 `anim_landsuck/anim_suck`，稳态用 `anim_land/anim_water`，运动速度按 C# 水车:草车 `2.5:3.33` 换算。
- level 19～27 存档写入 `poolGridVersion=1`。缺字段的旧五行/单植物槽存档保留文件但拒绝读取，让关卡重新开始。
- 3-1 为 10 波 `{normal, cone}`；3-2 为 15 波 `{normal, cone, bucket}`。水路替换在选行后发生，成本/权重仍使用出怪表中的基础类型。

## 验证

`smoke_pool_basics.json` 覆盖 3-1/3-2 背景、六行、出怪表、睡莲分层/保护/禁种、天气、台风整叠搬运、地形僵尸替换与水中死亡。`smoke_pool_cleaner.json` 覆盖待机、启动、入水、水中、出水和回到陆地。2026-07-23 两脚本在 `clang-playtest` 当前桌面可见运行均退出 0，`smoke_night_rain` 回归与 `save-migration` CTest 同样通过。
