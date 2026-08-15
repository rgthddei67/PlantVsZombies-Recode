---
name: project_pvz_gold_magnet
description: 第六大关磁暴菇的装备触发EMP、通用条件蒙特卡洛画像、原版资源与实盆站位合同
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-15
---

# 第六大关磁暴菇与条件磁吸推演

`PLANT_GOLD_MAGNET` 是 6-7（内部 52）通关奖励，6-8 起可选；由磁力菇 normal 层原位升级，
追加 75 阳光，卡冷却 50 秒，300 生命。它保留夜间蘑菇语义，白天睡眠并可由咖啡豆完整唤醒。

## 正式战斗合同

- `GoldMagnet : MagnetShroom` 复用 READY/SUCKING/CHARGING、离体物、磁吸音效和存档，只把成功
  吸取后的总充能改为 12 游戏秒。`MagnetShroom` 通过窄虚入口提供轨名、充能和成功剥离钩子，
  基础磁力菇仍使用 `anim_shooting → anim_nonactive_idle2`，磁暴菇使用原版资源的
  `anim_attract → anim_idle`；没有新增动画帧事件。
- 只有 `Zombie::ExtractMagneticItem` 原子成功并由植物接管离体物后，才以目标碰撞矩形中心释放
  100px 圆形 EMP；只对活动、非垂死、非魅惑敌方僵尸调用正式 `ApplyParalysis(2.5f)`，不造成伤害。
  场景扶梯仍能被吸取但不触发 EMP。
- 绝缘僵尸仍返回 150 点提取者本体反噬，顺序固定为“装备剥离/接管 → EMP → 反噬”；反噬致死
  不回滚装备或脉冲。接地僵尸的 30 秒范围免控只含减速、冻结和黄油，所以麻痹继续生效；天线路障
  本身不具磁吸资格。

## 全局轻量推演合同

- `PlantSimulationProfile` 保存通用磁吸范围、冷却和麻痹参数；场上 `PlantSnapshot` 复制真实剩余
  充能，新种卡牌从就绪态开始。白天用通用 `daytimeDormant` 让候选卡只保留阻挡生命，不虚构主动能力。
- `ZombieSnapshot` 复用正式 `CanBeTargetedByMagnetShroom()`，再由
  `GetMagneticSimulationLayer()` 声明 `HELM/SHIELD/TOOL`；矿工必须覆写为工具，避免把非磁性硬帽
  误当本次消费层。推演成功后原子消费资格并清对应生命层，同件装备不能重复贡献。
- `UpdateMagneticPulses` 是无 GameObject 的公共 step：普通爆区/蹦极选点、急救员治疗与黑夜屋顶
  雷荷路线全部调用。无任何磁性目标时以计数 O(1) 跳过；目标未进入 1100px 场景右边界时等待入场；
  有目标才按正式范围与距离选中、结算 100px/2.5 秒麻痹并进入 12 秒冷却。单 rollout 仍最多 16 只僵尸。

## 资源、站位与验证

- `GoldMagnet.reanim` 原样取自本地经典资源副本，SHA-256 为
  `6CE8B9F127113EC4F2C26BAF464847E41E4356B56BD87AFD734EB7E498021CF9`；时间线只有
  `anim_idle/anim_attract` 两个包装轨。EMP 使用 `GoldMagnetEMP.xml` 的约 0.36 秒紫环与六颗小火花，
  复用 `PARTICLE_RAIN_CIRCLE/PARTICLE_STAR40`，不新增位图。
- 主人实机发现初版 `scale=0.72, offset=[-55,-74]` 明显偏小且相对花盆左上漂。最终使用原生
  `scale=1.0, offset=[-34,-44]`；专项改在 6-7 初始真实花盆上升级，锁定水平中心与约 89px idle
  宽度，并以同步截图确认下部叶片坐入盆口。禁止退回无花盆捷径验证站位。
- 2026-08-15 `clang-release` 构建和两目标 Win7 导入审计通过，纯数值
  `PlantDefenseMonteCarloTests` 通过；当前桌面可见 `smoke_gold_magnet` 默认路径与最终资源
  `-NoInstance` 慢路径均为 96 条命令、exit 0，`smoke_gold_magnet_reward`、
  `smoke_magnetshroom`、`smoke_insulator_magnet` 与 `smoke_grounding_zombie` 也均 exit 0。
  两条渲染路径日志、状态 JSON 与最终实盆截图均已复核。
