# 植物 / 僵尸 血量显示叠加层 — 设计文档

日期：2026-05-31

## 目标

为植物和僵尸增加可开关的血量文字叠加显示，便于调试与观察。开关由 `GameAPP`
中的两个 bool 控制，植物与僵尸独立。

## 开关

复用 `GameAPP` 已有字段（无需新增）：

- `bool mShowPlantHP = false;`  — 植物显示血量
- `bool mShowZombieHP = false;` — 僵尸显示血量

## 数据来源

- 植物：`mPlantHealth` / `mPlantMaxHealth`
- 僵尸本体：`mBodyHealth` / `mBodyMaxHealth`
- 僵尸一类防具：`mHelmType` / `mHelmHealth` / `mHelmMaxHealth`
- 僵尸二类防具：`mShieldType` / `mShieldHealth` / `mShieldMaxHealth`

## 显示规则

### 植物
- 单行，绿色 `(0, 1, 0, 1)`
- 文本：`"<mPlantHealth>/<mPlantMaxHealth>"`，例如 `300/300`
- 无标签前缀
- 锚点：`GetPosition()`

### 僵尸
- 浅蓝色 `(0.6, 0.8, 1.0, 1.0)`
- 逐行向下堆叠，行距 ≈ 字号，行间无空行
- 仅显示存在的部件：
  1. 第 1 行（锚点 `GetPosition()`）：`本体: <mBodyHealth>/<mBodyMaxHealth>`
  2. 若 `mHelmType != HelmType::HELMTYPE_NONE`：`一类: <mHelmHealth>/<mHelmMaxHealth>`
  3. 若 `mShieldType != ShieldType::SHIELDTYPE_NONE`：`二类: <mShieldHealth>/<mShieldMaxHealth>`
- 不存在的部件整行不画（不留空行）

### 通用
- 预览态（`mIsPreview`，如选卡幽灵/图鉴预览）不显示
- 字号默认 17，字体 `ResourceKeys::Fonts::FONT_FZCQ`

## 实现方式

在 `Plant` 与 `Zombie` 分别重写 `void Draw(Graphics* g) override;`：

1. 先调用基类 `AnimatedObject::Draw(g)` 绘制本体动画。
2. 判断对应开关与 `mIsPreview`，满足条件时叠加血量文字。
3. 坐标：`GetPosition()` 为逻辑坐标，先经 `g->LogicalToWorld(x, y)` 转世界坐标，
   再直接调用 `g->DrawText(...)`（不走 `GameAPP::DrawText`，因其不做 letterbox 变换）。

不新建独立 `GameObject`：血量文字生命周期完全从属宿主，重写 `Draw` 最简单且零同步成本。

## 涉及文件

- `Game/Plant/Plant.h` / `Plant.cpp`：加 `Draw` 重写；`Plant.cpp` 补 `#include "../../GameApp.h"`
- `Game/Zombie/Zombie.h` / `Zombie.cpp`：加 `Draw` 重写（Zombie.cpp 已含 GameApp.h）

## 非目标 (YAGNI)

- 不画血条图形（仅文字）
- 不做血量颜色随比例渐变
- 位置/字号微调由主人后续自行调整，本次只给 `GetPosition()` 默认锚点
