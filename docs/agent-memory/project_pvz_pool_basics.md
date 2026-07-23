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

- `Background::WATER_POOL` 与其他背景共用 `mBackgroundY`，不再额外上移；日间/夜间泳池的六行网格首行顶部均为 Y=85。泳池使用 6 行、85px 行高，0-based 第 2/3 行是水路，列宽仍为 80px。网格派生位置必须通过 `Board::GetCellCenterPosition/GetCellHeight`。
- `Cell` 有 `under/normal` 两个植物槽。睡莲只能放在空水格的 under 层；普通植物只能放在已有睡莲且 normal 层为空的水格；土豆雷仍禁止下水。铲子、僵尸啃咬与 UI 预览都以 top 层为准。
- 睡莲种下后 1 秒只免疫啃咬，计时器进存档；水面植物只做±2px 绘制浮动，Transform/碰撞与存档仍固定在逻辑格。
- 普通/路障/铁桶僵尸在水路由正式波次入口替换为 `ZOMBIE_POOL_*`，而非为出怪表添加独立权重。特殊陆地僵尸不进水路。水路僵尸使用前/后双探针进水；本项目将下水/出水切换位置相对原探针向右校正 70px。水中断头/断臂不生成悬浮的陆地碎片，死亡轨道分陆地/水中。
- 泳池僵尸仅在 `mInPool=true` 时拒绝化灰：植物爆炸仍走正式伤害与水中死亡轨道，但不创建 `ZombieCharred` 烧焦残影；尚未下水或已经离水时继续沿用陆地化灰表现。这与 C# `ApplyBurn` 的 `mInPool -> DieWithLoot` 分支一致。
- 所有僵尸生成 Y 由网格行中心派生。第一、二大关继续使用已确认正确的 `kZombieSpawnBaseOffsetY=+2px`；主人实测后的泳池背景公共偏移为 `kPoolBackgroundZombieSpawnYOffset=0px`，第三大关所有行再应用 `kThirdAreaZombieAlignmentOffsetY=+10px`。水路必须与同地图陆地行分开，`IsPoolRow(row)` 额外应用 `kPoolRowZombieSpawnYOffset=+25px`，因此只下移第 3/4 行水中僵尸，不影响陆地行。四者均集中在 `Board.cpp` 顶部；旧名 `kPoolZombieSpawnYOffset` 因容易误解为水路专属而弃用。
- `anim_eat` 会重新显露双腿并把 `Zombie_duckytube` 换回无水面接触纹理的普通贴图；泳池僵尸在水中进入啃食态后必须隐藏 inner/outer leg 的 upper/lower/foot 六条轨道，并把泳圈覆盖为 `IMAGE_REANIM_ZOMBIE_DUCKYTUBE_INWATER` 保留涟漪，停止啃食和读档恢复时对称重建/撤销覆盖。
- 僵尸已锁定睡莲后若同格新种上层植物，必须把啃食目标和 `mEaterCount` 从睡莲迁移到新顶层植物，且不重播啃食动画；`StartEat` 的碰撞 stay 主动迁移，`EatTarget` 在每个伤害帧前再兜底检查一次，避免碰撞更新顺序让一口伤害误落到睡莲。该规则按同格顶层通用实现，不把坚果或睡莲类型写死。
- 台风对同格睡莲+上层植物必须整叠搬运/丢失，不允许拆叠。
- `PoolCleaner` 使用 `LAND/ENTERING/IN_POOL/EXITING` 高度状态；只改绘制偏移，不改行与碰撞。水中稳态相对原 28px 下沉位置向上校正 13px，入水/出水阶段按当前浸入比例渐进叠加该校正以避免跳变，原始深度仍照常存档。陆地/水中吞噬分别用 `anim_landsuck/anim_suck`，稳态用 `anim_land/anim_water`，运动速度按 C# 水车:草车 `2.5:3.33` 换算。
- level 19～27 存档写入 `poolGridVersion=2`。缺字段、旧五行/单植物槽或旧版上移 40px 坐标存档均保留文件但拒绝读取，让关卡按当前坐标重新开始。
- 3-1 为 10 波 `{normal, cone}`；3-2 为 15 波 `{normal, cone, bucket}`。水路替换在选行后发生，成本/权重仍使用出怪表中的基础类型。

## 验证

`smoke_pool_basics.json` 覆盖 3-1/3-2 背景、六行、出怪表、睡莲分层/保护/禁种、天气、台风整叠搬运、地形僵尸替换与水中死亡。`smoke_pool_cleaner.json` 覆盖待机、启动、入水、水中、出水和回到陆地，并锁定水中上移 13px 后 `visualY≈309.5`、回到陆地为 `visualY≈294.5`。`smoke_pool_zombie_visuals.json` 在 x=930 断言向右校正后的入水切换，并同时锁定恢复通用背景基线后第三大关水路 Y≈335、陆地行 Y≈140、第一/二大关仍为 Y≈340；水中 `anim_eat` 时六条腿部轨道全隐藏且保留涟漪。`smoke_pool_zombie_interactions.json` 用陆地化灰作对照，断言水中爆炸 `charredZombieCount=0`，并现场验证补种上层植物后睡莲/坚果 `eaterCount` 从 `1/0` 迁移为 `0/1`。2026-07-23 恢复通用背景基线后，前三个泳池坐标相关脚本均在 `clang-playtest` 当前桌面可见运行退出 0，`clang-playtest`、`clang-release` 与 `save-migration` CTest 通过；既有交互脚本、`smoke_night_rain` 等证据继续有效。
