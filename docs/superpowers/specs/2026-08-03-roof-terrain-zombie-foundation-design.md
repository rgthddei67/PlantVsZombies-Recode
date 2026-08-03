# 屋顶地形与僵尸路径基础设计

日期：2026-08-03 ｜ 状态：主人已确认第一阶段范围

## 1. 本阶段目标

- 接通 `ROOF` / `NIGHT_ROOF` 背景显示与五行屋顶棋盘。
- `Board` 成为屋顶几何的唯一权威：离散格位沿斜坡逐列错高，运动对象按世界 X 连续贴合斜坡。
- 普通僵尸以及已实现的矿工、气球等特殊移动僵尸均复用同一地形高度入口。
- 暂不实现花盆。为便于先验证战斗，本阶段明确允许普通植物直接种在屋顶 normal 层；花盆完成后再收紧为 under 承载层。

## 2. C# 语义与当前场景换算

C# `Board.GridToPixelY` 的屋顶格高为 85px，前五列每向左一列下沉 20px；
`Board.GetPosYBasedOnRow` 则以 1:4 的连续坡度让移动对象随 X 下坡。

本项目不得复制 C# 的 800×600 绝对坐标。当前棋盘左缘为
`CELL_INITALIZE_POS_X=242`，因此坡顶由网格派生为：

`roofSlopeEndX = CELL_INITALIZE_POS_X + 5 * CELL_COLLIDER_SIZE_X`

连续坡高为：

`max(0, roofSlopeEndX - worldX) * 0.25`

离散格位仍取各列左缘的连续坡高，使前五列依次相差 20px。屋顶基础 Y 使用现有
`CELL_INITALIZE_POS_Y` 再减去原版格位公式中的 10px 校正；所有数值集中在
`Board.cpp` 匿名命名空间并写明单位。

## 3. API 与所有权

`Board` 新增或扩充以下窄入口：

- `IsRoofBackground()`：统一判断白天/黑夜屋顶。
- `GetRowCenterYAtX(row, worldX)`：运动对象所用的连续逻辑行中心。
- `GetZombieCollisionY(row, worldX)` / `GetZombieSpawnY(row, worldX)`：生成、读档和逐帧贴地共用；保留单参数重载供没有 X 的旧调用方使用。
- `GetCellCenterPosition(row, col)`：返回离散斜坡格中心；Cell 的 Transform/Collider 使用同一位置。

禁止在普通僵尸、矿工、气球各自复制斜率或坡顶常量。

## 4. 僵尸更新

- `Board::CreateZombie` 与 `CreateZombieWithID` 按保存的 X 计算初始 Y，存档仍只保存 `row + x`。
- `Zombie` 基类在台风位移后、普通/派生 `ZombieMove` 后把 Transform Y 对齐当前 X 的屋顶地形。
- 该同步只改变 Transform 的地形基线；矿工出土深度继续由 `mVisualOffset`、气球飞行高度继续由空中碰撞 offset 表达，因此不会覆盖各自状态机。
- 矿工和伴舞的地面裁剪底边改为按当前 X 查询，避免在斜坡上仍裁到平地高度。
- 开发者鼠标选行同样用鼠标 X 查询各行屋顶高度。

## 5. 临时植物规则

本阶段 `CanPlantAt` 对屋顶走明确的“临时直种”分支：普通植物占 normal 层，仍遵守占格、
弹坑、冰道、唯一性等现有规则。睡莲、水草等水生规则不因屋顶放宽。

花盆实现时删除该过渡分支，新增 `PLANT_FLOWERPOT` under 层资格，并同步
`CreatePlant/CreatePlantWithID`、顶层选择、铲子、啃食、绘制层、存档和 AutoTest。
第一阶段屋顶存档属于开发中过渡数据；花盆接入时用屋顶网格版本拒绝或迁移无花盆直种旧档，
不静默制造非法组合。

## 6. 本阶段不做

- 花盆植物及其承载层规则。
- 投掷植物、屋顶清洁车、屋顶专属僵尸与 5-1～5-9 出怪编排。
- 平射子弹撞坡与投掷弹贴坡弹道；它们在地形权威稳定后单独接入，避免与僵尸路径同时扩大回归面。
- 5-9 Boss 玩法；`NIGHT_ROOF` 仅复用同一几何并保持既有夜晚判断。

## 7. 验收

- level 37 显示屋顶背景、5 行、9 列、85px 行高，前五列逐列错高且后四列同高。
- 屋顶可直接种普通植物，植物逻辑位置等于对应 Cell 中心。
- 普通、气球和矿工在坡面任意 X 的 Transform Y 与 `GetZombieSpawnY(row, x)` 一致。
- 台风/普通移动跨过坡顶时没有 Y 跳变；快照重载后仍按保存 X 恢复正确高度。
- 白天屋顶与黑夜屋顶共用几何；非屋顶、泳池现有坐标回归不变。
