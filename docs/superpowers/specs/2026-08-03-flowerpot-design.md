# 花盆（Flower Pot）设计

日期：2026-08-03　状态：主人批准完整承载层接入

## 目标

新增经典花盆植物本体，使用主人放入并删除 shadow 轨的 `Pot.reanim`，数值和短暂无啃食行为参照 C# Lawn 实现。

## 当前范围

- 实现 `FlowerPot : Plant`、注册、卡图、数值、图鉴、存档和专项验证。
- 花盆保留原版 300 生命、25 阳光、7.5 秒冷却，以及种下后 1 秒不能被普通僵尸啃食的状态。
- C# `Plant::DrawShadow` 会以 1:1 比例为花盆绘制通用植物阴影，并把通用 X/Y=`-3/51px` 调到 `-4/46px`；本项目让现有 `ShadowComponent` 使用该实际落点与比例，不恢复 reanim shadow 轨。
- 花盆占 Cell 的 `under` 层；屋顶普通植物与南瓜必须有花盆才能分别进入 `normal` / `pumpkin` 层，地刺系仍按原版拒绝屋顶。
- 上层植物视觉锚点相对花盆抬升 5px，逻辑格与碰撞箱不变；落点预览复用同一抬升口径。
- 花盆被上层植物覆盖时暂停 `anim_idle`，上层移除后恢复播放。
- 台风把 `under + normal + pumpkin` 当作同一格组合逐格搬移；花盆与其承载植物同步更新逻辑格，并共享 0.45 秒二维视觉追赶。
- 新开屋顶关在选卡前按 C# `CutScene.AddFlowerPots` 的列优先顺序铺设初始花盆；当前九关制映射为 5-1 五列、5-2 四列、5-3～5-9 三列，读档不重复生成。

## 动画与资源

- `Pot.reanim`：12fps，战斗本体只播放 `anim_idle`。
- `anim_zengarden`、`anim_waterplants` 属于原版其他场景包装轨，本次不接入。
- 实际时间线引用 `Pot_bottom`、`Pot_bottom2`、`Pot_water_base`、`Pot_stem`、`Pot_leaf1`、`Pot_leaf2`、`Pot_top`、`Pot_water_top`；动画内没有 shadow 轨。

## 存档

`SaveExtraData/LoadExtraData` 往返剩余啃食保护时间。旧档缺字段时默认保护结束，避免读档凭空获得保护。

## 验收

- `clang-release` 零警告构建。
- 可见 `smoke_flowerpot.json`：断言资源、数值、`anim_idle`、300 生命、阴影、1 秒保护、屋顶门禁、under/normal 分层、覆盖暂停、上层移除、存读档和双向台风整组搬移。
- `smoke_flowerpot_starting_layout.json` 独立断言 5/4/3 列分支以及列外层、行内层的创建顺序。
- 多植物阶段只使用 `cells`、`topPlantsByCell` 和 `flowerPotsByCell` 做稳定断言，不依赖 unordered plant 数组顺序。
