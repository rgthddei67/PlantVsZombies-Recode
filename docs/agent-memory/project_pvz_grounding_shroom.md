---
name: project_pvz_grounding_shroom
description: 第六大关接地菇的三格雷荷保护、反噬、绝缘僵尸接地和独立低精度动画合同
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-13
---

# 第六大关接地菇

`PLANT_GROUNDINGSHROOM` 是 5-9 通关奖励，6-1 起可选；100 阳光、20 秒卡冷却、500 生命，
继承 `Shroom`，黑夜清醒、白天睡眠，屋顶仍必须由花盆承载。

## 放电合同

- 接地菇声明式保护自身与同排左右各一列的所有非花盆植物层，只免本次离散雷荷停机，
  不改变 `Board::IsPlantPausedByRoofRunoff` 的连续径流暂停。
- `Board::ResolveNightRoofChargeDischarge` 先按稳定植物 ID 冻结所有目标分配：最近列距离优先，
  同距取更小植物 ID；僵尸消费同一批仍有效接地范围后，再按提供者 ID 每株只反噬一次。普通瓦面直接扣本体 100，接地菇自身列
  位于活动湿坡时扣 150，绕过南瓜和防御词条。接地菇因此死亡也不回滚本次植物保护或僵尸压制。
  满血时可在干燥瓦面完成 5 次保护并于第 5 次反噬后死亡；湿坡可完成 4 次并于第 4 次死亡。
- `Plant::CanGroundNightRoofChargeFor`、`SuppressesNightRoofChargeProtectionFor` 与
  `AbsorbGroundedNightRoofCharge` 是品种侧窄接口，Board 不按植物类型分支。
- 同排且位于接地菇连续三格条带内的绝缘僵尸不能承接本次雷荷；直接命中仍按原规则扣胸甲，
  但不会因该次命中进入过载。轻型弹丸抗性、湿润增伤和其他胸甲规则不变。

## 动画与资源

- 独立 `GroundingShroom.reanim`，基础 12fps；包装轨为 `anim_idle`、`anim_shooting`、
  `anim_sleep`，整株 `GroundingShroom_body` 轨负责低幅呼吸和图片切换。
- 放电直接 `PlayTrackOnce("anim_shooting", "anim_idle", ..., returnBlend=0)`，不使用帧事件；
  通用 Animator 存档可完整往返中途受电轨，因此无额外品种字段。
- 四张战场姿势为 112×120 低分辨率透明图，整株按初版生成尺寸等比缩至约 0.8；卡图保持
  120×120 画布但整株独立等比缩至约 0.7 并居中。`scripts/generate_grounding_shroom_assets.ps1`
  从去底源图确定性重建资源，二者都不压缩宽高比。

## 数值背景与验证

同批将基础雷荷停机从 2.5/5 秒提升为普通瓦面 8 秒、径流湿坡 20 秒；平台仍按普通档。
专项应同时覆盖资源与数值、三格边界、湿/干反噬、死亡不回滚、径流暂停保留、中途动画存档、
绝缘拒绝承接/过载、5-9 奖励，以及默认实例化与 `-NoInstance` 两条绘制路径。

## 当前验证证据

- 2026-08-13 `clang-release` 增量构建完成，378 项 Win7 x64 导入审计通过；applocal 仍输出仓库既有的
  dumpbin/objdump 探测警告，但链接和产物生成均成功。
- 当前桌面可见 `smoke_grounding_shroom` 默认与 `-NoInstance` 最终 87 条命令均 exit 0；覆盖
  500→400 干燥反噬、500→350 湿坡反噬、三格边界、径流暂停、致死快照、
  接地菇致死当次仍压制绝缘僵尸、中途动画存档和普通接地范围内的绝缘能力压制。
- `smoke_grounding_shroom_card`、`smoke_grounding_shroom_reward`、
  `smoke_night_roof_charge_effects` 与 `smoke_insulator_charge` 均可见 exit 0；同步截图已目验战场
  可见框约 80×86、卡图约 72×78，均保持原宽高比。
