# 屋顶地形与僵尸连续坡面

## 当前状态（2026-08-03）

- 内部关卡 37～44 使用 `ROOF`，45（5-9）使用 `NIGHT_ROOF`；`GameScene` 已分别绘制 `IMAGE_BACKGROUND_ROOF` 与 `IMAGE_BACKGROUND_NIGHTROOF`，两张贴图均通过资源键加载断言。
- 屋顶沿用 5 行、9 列：首行逻辑顶部为 `CELL_INITALIZE_POS_Y - 10 = 78`，行高 85。房屋侧前 5 列按每列 20px 离散抬升，平台从 `CELL_INITALIZE_POS_X + 5 * CELL_COLLIDER_SIZE_X = 642` 开始；植物与格对象统一取 `Board::GetCellCenterPosition`。
- 僵尸地面是同一几何的连续版本：平台左侧 `Y += (642 - worldX) * 0.25`。权威接口为 `Board::GetRowCenterYAtX`、`GetZombieCollisionY(row, worldX)` 和 `GetZombieSpawnY(row, worldX)`，不得在僵尸品种中复制坡度。
- `Zombie` 基类在阵风横移后、以及每个品种完成 `ZombieMove` 后统一执行屋顶 Y 收敛。因此普通行走、气球飞行和矿工地下移动都只负责 X；出生与读档也用保存的 `row + x` 重建 Y，无需提升存档版本。屋顶美术脚底最终校准量为行中心下移 `17.5px`。
- 伴舞出土与矿工地下裁剪使用当前 X 的地面线。选卡预览在网格初始化后创建，使用主人从原版量得的世界 X `1056..1356` 和第 2～5 行，再经同一出生 Y 接口贴坡，避免顶行头部进入天空。

## 坡面消费边界

- 不给通用 `Transform` 自动套坡面。`Board` 只提供几何权威，地面实体、网格实体、跨行效果与自由飞行对象按语义选择是否消费，避免污染其他地图、UI、投掷物和独立动画。
- `LawnMower` 在屋顶统一切为 `MowerType::ROOF`、加载 `RoofCleaner.reanim`，逻辑路径每帧经 `GetMowerTerrainY(row,x+40)` 贴坡，地形原点最终下移 `9px`。本体 idle/moving 视觉偏移分别为 `(6,-40)` / `(-4,-4)`；影子分别为 `(41,22)` / `(31,58)`，已覆盖触发前、运动中和读档恢复。
- 普通与鎏金冰车仍禁止在正式屋顶波次生成，但测试直造时冰道按原版语义只保留平台：左缘钳到坡顶 `X=642`，整段以平台行 Y 水平绘制，不沿坡弯曲。
- 辣椒的 12 段火焰逐段按自身 X 查询连续地面线；屋顶弹坑使用左右屋顶专图与对应偏移；雨滴地面水花先随机 X，再在该 X 的连续行地面窄带内落点。
- 台风换格仍在结算帧切逻辑格，但 0.45 秒视觉追赶从源/目标 `GetCellCenterPosition` 插值完整二维锚点，本体、影子与测试导出共用该锚点，因此植物经过屋顶坡面不会横移浮空。
- 平射子弹不贴坡，只把当前弹体离地高度与 `GetRowCenterYAtX(row, bulletX)` 比较：豌豆/寒冰/火球/尖刺/毒豆阈值 28px，孢子 0px，星弹 23px；投掷物排除。撞坡先播放对应命中反馈再回收，阴影独立消费当前 X 的地面线。

## 临时种植边界

- 花盆尚未实现。本阶段在 `Board::CanPlantAt` 显式允许非水生普通植物直接占屋顶 normal 层，under 层保持为空；睡莲、水草、海蘑菇仍保留各自水生地形限制。
- 后续花盆应占 under 层，并在同一个屋顶门禁把“允许直种”替换为“已有花盆才允许 normal 植物”，同时覆盖预览、铲除、啃食、存档重建和组合层测试。

## 明确延后

- 花盆、投掷植物主动调整抛物线、屋顶专属僵尸与 Boss 仍未实现。投掷物当前保持自由抛物线，并明确排除于平射坡面遮挡。

## 验证证据（2026-08-03）

- `clang-release` 配置、编译、链接成功。
- 主人完成连续实玩校准并确认最终画面“现在没问题了”。可见 `smoke_roof_zombie_foundation.json` 与 `smoke_roof_terrain_consumers.json` 均 exit 0；后者覆盖预览、屋顶车静止/运动/读档、平台冰道、逐段辣椒火焰、左右弹坑、台风植物二维滑动、白天/黑夜屋顶、雨滴落点及平射坡面遮挡，并逐张检查 6 张同步截图。
- 可见回归 `smoke_zamboni.json`、`smoke_jalapeno.json`、`smoke_typhoon.json`、`smoke_typhoon_bullets.json`、`smoke_crater_terrain_visuals.json`、`smoke_bullet_shadow.json` 全部 exit 0。`smoke_typhoon_bullets` 同步了当前源码的风力数值与孢子已响应台风的类型表，不再保留过期期望。
