# 屋顶地形与僵尸连续坡面

## 当前状态（2026-08-03）

- 内部关卡 37～44 使用 `ROOF`，45（5-9）使用 `NIGHT_ROOF`；`GameScene` 已分别绘制 `IMAGE_BACKGROUND_ROOF` 与 `IMAGE_BACKGROUND_NIGHTROOF`，两张贴图均通过资源键加载断言。
- 屋顶沿用 5 行、9 列：首行逻辑顶部为 `CELL_INITALIZE_POS_Y - 10 = 78`，行高 85。房屋侧前 5 列按每列 20px 离散抬升，平台从 `CELL_INITALIZE_POS_X + 5 * CELL_COLLIDER_SIZE_X = 642` 开始；植物与格对象统一取 `Board::GetCellCenterPosition`。
- 僵尸地面是同一几何的连续版本：平台左侧 `Y += (642 - worldX) * 0.25`。权威接口为 `Board::GetRowCenterYAtX`、`GetZombieCollisionY(row, worldX)` 和 `GetZombieSpawnY(row, worldX)`，不得在僵尸品种中复制坡度。
- `Zombie` 基类在阵风横移后、以及每个品种完成 `ZombieMove` 后统一执行屋顶 Y 收敛。因此普通行走、气球飞行和矿工地下移动都只负责 X；出生与读档也用保存的 `row + x` 重建 Y，无需提升存档版本。
- 伴舞出土与矿工地下裁剪使用当前 X 的地面线。冰车及鎏金冰车仍按既有规则禁止在屋顶生成，等其专属坡面/冰道语义单独实现。

## 临时种植边界

- 花盆尚未实现。本阶段在 `Board::CanPlantAt` 显式允许非水生普通植物直接占屋顶 normal 层，under 层保持为空；睡莲、水草、海蘑菇仍保留各自水生地形限制。
- 后续花盆应占 under 层，并在同一个屋顶门禁把“允许直种”替换为“已有花盆才允许 normal 植物”，同时覆盖预览、铲除、啃食、存档重建和组合层测试。

## 明确延后

- 平射子弹被坡面遮挡、投掷物抛物线、屋顶清洁车、屋顶专属僵尸与 Boss 不在本阶段。平射遮挡应让子弹高度与 `GetRowCenterYAtX(row, bulletX)` 比较，不能通过让平射子弹本体贴坡伪造。

## 验证证据（2026-08-03）

- `clang-release` 配置、编译、链接成功。
- 可见 `smoke_roof_zombie_foundation.json` 65 条命令全绿：锁定白天/黑夜背景、5×9 网格、85px 行高、642px 坡顶、直接种植、普通/气球/矿工相对坡面偏移为 0，以及存档重载后仍为 0；三张同步截图已逐张核对。
- 可见 `smoke_digger_spawn_rows.json` 通过。`smoke_pool_basics.json` 的本次相关部分（六行网格、睡莲叠层、阵风、陆/水僵尸）全部通过，随后停在既有 3-2 `maxWave` 脚本期望 15、当前实值 20 的过期断言，与屋顶改动无关。
