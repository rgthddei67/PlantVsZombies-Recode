---
name: project_pvz_plantern_fog_core
description: 2026-07-29 路灯花、雾火经济、逐格照明/索敌、产光倍率、卡槽控制 UI 与存档验证
metadata:
  node_type: memory
  type: project
---

# 路灯花与迷雾核心

## 当前契约（2026-07-29）

- `PLANT_PLANTERN` 在冒险 4-1 作为奖励，25 阳光、30 秒冷却；4-1 只保留雾视觉，4-2 起启用
  燃料、照明、产光增益和雾中远程索敌限制。
- 场上最多一株活动路灯花，唯一性由 `Board::mActivePlanternID` 派生；死亡/压扁释放名额，
  读档由 `CreatePlantWithID` 重建 ID。它不是累计种植次数限制。
- 容量 100、初始 20；关闭/I/II/III 每游戏秒消耗 0/0.5/1/2。范围为无、4×3、
  裁角 8×5、扩大裁角 10×7；新增的一列统一朝僵尸来向，III 最外圈照明 72%。
- `Board::GetPlanternIllumination()` 是逐格唯一形状源，`UpdateFogCellAlpha`、雾中索敌和
  `GetPlanternSunProductionMultiplier()` 都消费它。向日葵/阳光菇生产峰值为
  110%/120%/135%；阳光菇成长和发光尾段不加速。
- 原生雾片约四成采样接近全透明；`GameScene::DrawFog` 先以错位冷灰雾片补洞，再画原层，
  让满雾真正遮住僵尸。补洞层继续消费同一逐格 alpha，不使用固定白色矩形底幕。
- 4-2 起远程索敌以平滑后的 `GetFogCellAlpha()` 为权威，阈值 96；横向 100px 且行差不超过 1
  的近身目标仍可取得。已发射子弹、被动接触和即时结算不撤销。

## 雾火与 UI

- 只有 `TrySummonZombie()` 正式波次创建成功后分配雾火；单只 15，概率使用跨波累计保底，
  普通耐久每只给累计器加 0.30、高耐久最多 0.60，每波预算随压力从 45 平滑到 60。
  僵尸死亡走 `CollectMistFuelFromZombie()` 唯一发起入口。
- 无路灯花、满仓和魅惑目标直接丢弃；部分溢出只预留可容纳量，其余丢弃并触发 1.8 秒卡牌提示。
  `MistFuel` 飞行 0.62 秒，途中只占容量而不增加显示值，抵达灯芯时才正式到账。
- 卡牌在路灯花存活时显示挡位、取整燃料值和比例条；点击卡牌/本体展开
  `CardSlotManager` 持有的 0/I/II/III 瞬态菜单。菜单直接对齐路灯花卡槽下方，允许覆盖天气
  面板，不属于天气栏目且不进入存档。
- 资源键为 `IMAGE_MISTFUEL`，权威文件
  `build/clang-release/resources/image/MistFuel.png`；因 build 被忽略，提交须 `git add -f`。

## 存档与验证

- `Plantern::SaveExtraData` 保存 `fuel/pendingFuel/gear`；在途对象不单独保存，读档把预留量
  结算进燃料。`Zombie::SaveProtectedData` 保存奖励和已领取；Board 保存
  `mistFuelDropAccumulator`。旧档均以中性值兼容，无需 schema 提升。
- `smoke_plantern_fog_core` 当前可见运行 174 条命令 exit 0，覆盖 4-1/4-2 边界、初始 20、
  四档范围/耗油/产光、索敌、唯一性、飞行前后到账、在途读档、满仓与部分溢出、魅惑、
  无路灯花废弃、死亡重种和真实挡位按钮点击；关键截图已目验。
- 原有 `smoke_fog_weather` 148 条命令可见回归 exit 0，确认无路灯花时雾势、台风驱散及
  雾存档不变。
- 设计定稿见 `docs/superpowers/specs/2026-07-29-plantern-fog-core-design.md`。
