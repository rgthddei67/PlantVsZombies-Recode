---
name: project_pvz_frost_mine
description: 第七大关伏霜雷的预报校准、真实冻土武装、地面触发、冰制层腐蚀、存档与独立三态美术契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 第七大关伏霜雷

## 定位与奖励

`PLANT_FROSTMINE` 是第七大关第三株新植物，内部 level 58（7-4）通关奖励，供 7-5 起使用；7-3 仍明确无植物奖励。当前调参为 50 阳光、30 秒冷却、300 生命，模拟层 `persistent=false`。它是读取准确寒潮预报后提前校准、待真实冻土兑现才提交的一次性地面伏击，不是随种随炸的土豆雷，也不会自行改变 Board 温度或霜线。

## 状态与 Board 边界

植物私有阶段为 `DORMANT -> CALIBRATED -> ARMED -> SPENT`：

- `DORMANT` 仅在 Board 当前准确预报预测自身行列将进入冻土时转入 `CALIBRATED`；预测范围复用 Board 与真实冻结相同的列数公式，不从 UI 文本或视觉霜线反推。
- 气象干扰只清除 `CALIBRATED`，不改变 Board 已锁定的真实寒潮计划。植物不会仅因当前温度已经很低而越过一次新的有效预报直接武装。
- `CALIBRATED` 在所在格真实冻结时提交为 `ARMED`。一旦提交，回暖、下一轮预报和预报干扰均不能撤销；它在已融化格仍等待首个合法地面目标。
- `SPENT` 与死亡在同一调用栈提交，正常存档不应观察到。损坏或人工构造的 `SPENT` 快照按仍可触发的 `ARMED` 修复，避免加载出不会消失也不会工作的地雷。

阶段保存在植物 `extraData.phase`，读档后立即同步完整状态贴图，不补播音效、粒子或引爆。武装态覆盖 `CanBeEaten()` 为 false；其余阶段沿用植物基类规则。

## 目标与结算顺序

武装后每帧主动扫描一次碰撞范围，以覆盖冻土边沿到达时已有僵尸站在格内的情形；碰撞进入回调仍作为普通移动触发路径。合法目标必须活动、未死亡、未魅惑、与植物同行、碰撞框启用并允许地面机关影响；气球等飞行目标由 `Zombie::CanBeAffectedByGroundHazards()` 排除。同帧多个合法目标按最小 `mZombieID` 唯一选择，保证存档和 AutoTest 可复现。

引爆先锁定音画位置并原子设置 `SPENT`，随后严格按以下顺序调用目标拥有的语义接口：

1. `InterruptUncommittedSpecialAction()`，只撤销尚未提交的蓄力或施工；
2. `ApplyWinterCorrosion(1000)`，只由目标扣除自己的冰制装备层，剩余值不得折算为本体伤害；
3. `TakeDamage(600, DamageSource::PLANT)`，沿标准防具链结算植物来源伤害；
4. 发出土豆雷底音、冻结叠音和 `FrostMineBurst` 后死亡。

对冰裂钻机，该顺序先把 `CHARGING` 退回 `MOVING`，再完整摧毁 900 点独立钻机层，保留其未消费但已失去装备的状态，不产生地裂；600 点常规伤害穿过已移除钻机后把 650 本体压至 50。断头阈值后的通用流血可能在下一更新帧把投影变为 49，测试不得把跨帧结果误锁为 50。

## 美术与资源

三阶段使用同一 reanim 身体轨的三张完整独立贴图：休眠态为低亮蓝紫球茎，校准态有抬起冰晶与青色霜环，武装态明显埋入冰雪且轮廓变低；禁止只用 tint 区分。卡图、碎冰贴图、霜环贴图和 `FrostMineBurst.xml` 均独立注册，默认实例、`-NoInstance` 使用同一资源键。

权威概念源为 `docs/art/frost-mine/frost-mine-concept-v1.png`，SHA-256 为 `2EF0001B381B7B1CDBC2445A0F7B15FC8827D1A33C6955A4D06F18D13BB4ADD5`。`scripts/generate_frost_mine_assets.ps1` 从三格源图确定性裁切、缩放并锁定所有最终输出哈希。状态文件名必须保留 `REANIM_FROSTMINE_*` 词干，才能经标准图片预载得到代码使用的 `IMAGE_REANIM_FROSTMINE_*` 键；新增或重命名后必须刷新 `manifest.txt`，否则启动时仍会扫描旧路径并在资源载入阶段退出。

## 验证

`smoke_frost_mine.json` 共 96 条命令，覆盖资源键、50/30/300 调参、预测冻土边界、干扰退校准、真实冻结武装、武装态不可啃食、存档与回暖持久、普通僵尸引爆、冰裂钻机 1000 腐蚀＋600 伤害＋动作中断且零地裂、飞行目标不触发、7-3 无奖励和 7-4 奖励。`smoke_frost_mine_noinstance.json` 提供同资源和三态的 `-NoInstance` 可见短路径。

2026-08-25 `clang-debug` 全量构建零编译警告，主程序 Win7 导入审计通过 469 项；上述两个脚本在主人当前桌面可见运行均 exit 0、`status=passed`、`script finished OK`。默认路径执行至 command 95，`-NoInstance` 路径执行至 command 16；同步截图已目验三态轮廓、武装埋地、钻机反制、飞行目标保留地雷和 7-4 卡牌。

最终 `clang-release` 全量 O2/LTO 构建零编译器警告，主程序 Win7 导入审计通过 378 项；`save-migration`、`save-schema`、`plant-defense-monte-carlo` 三项 CTest 全部通过。主人当前桌面可见的默认 Vulkan 1.3 实例路径再次执行 `smoke_frost_mine.json` 至 command 95，`-NoInstance` 路径执行短脚本至 command 16，均 exit 0、`status=passed`、`script finished OK`，两路径 Release 截图再次目验通过。同一产物的 `smoke_ice_crack_drill.json` 至 command 132、`smoke_melt_snow_pult.json` 至 command 91、`smoke_winter_garden.json` 至 command 143，均在默认 Vulkan 可见路径通过。
