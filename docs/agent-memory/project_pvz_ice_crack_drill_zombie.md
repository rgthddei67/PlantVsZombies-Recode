---
name: project_pvz_ice_crack_drill_zombie
description: 第七大关冰裂钻机僵尸、冻土地裂、右手钻机视觉、存档和验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 冰裂钻机僵尸与独立地裂

## 身份与投放

`ZOMBIE_ICE_CRACK_DRILL` 为 650 本体＋900 冰制钻机层，权重 2400、第五波起、生存第 12 轮起，每个正式波次最多生成一只。冒险在 7-4 首次登场，7-5 立即复习，7-6/7-7 暂退给后续新机制独立教学，7-8/7-9 进入组合关。图鉴只在玩家通关 7-4 后解锁。

僵尸复用 `ConeZombie.reanim` 的既有死亡与啃食事件，不新增动画帧号。安全帽与钻机均不是全身换色：安全帽三档破损，独立钻机三档破损×四相钻齿；资源源图保存在 `scripts/assets/ice_crack_drill_source.png`，`scripts/generate_ice_crack_drill_assets.ps1` 可确定性重建实机贴图、地裂和碎冰粒子。

## 蓄力与装备生命周期

钻机完全进入草坪且当前格为冻土时停步蓄力 3.5 游戏秒。冻结、黄油和麻痹通过基类更新门禁自然暂停；冻土融化或通用未提交动作中断只取消本次尝试，之后可重试。死亡、断头、断臂、魅惑或钻机层碎裂会原子取消并消费能力；钻机由画面前侧右手握持，断臂同步脱落钻机，读档会把非法终止态修复为 `SPENT`。

钻机 reanim 挂在稳定 `Zombie_body` 轨，握把对齐右手；身体轨先于手掌轨绘制，所以手掌自然盖住握把。实机按源图 50% 绘制，不继承外臂的大幅摆动；巡航约 0.5px 垂直微摆，蓄力仅约 ±1px 水平、±0.5px 垂直高频颤抖。矿工安全帽原图属于向右行走品种，生成器先镜像到脸侧再前移 3px，避免帽檐落在后脑。

钻机层永久掉落时不能只把子 Animator 的 Alpha 设为 0：破甲同帧的父级受击 additive glow 会绕过该透明度，留下半透明且仍在转动的钻机轮廓。终态现在从 `Zombie_body` 调用 `DetachAnimator` 后释放强引用，读档恢复的无钻机状态也执行同一销毁路径；专项同时断言 `drillRigAnimatorReady=false` 并目验破甲瞬间截图。

## 地裂与雪锚

蓄力提交后由 Board 创建独立 `GroundRift`，来源僵尸死亡、魅惑或回暖都不回滚。地裂以 180px/游戏秒向房屋传播，跨大步长仍按列从右到左各结算一次；Board 的 `ForEachActivePlantInCell` 统一按 `overlay/pumpkin/normal/under` 快照并重新解析活动实体，`ApplyWinterGroundImpactToCell` 在同一入口之上先选定拦截响应、再逐层伤害。咖啡豆、南瓜、普通植物和花盆/睡莲均各自承受当前倍率下的 500 点僵尸来源伤害，并发射裂缝粒子；咖啡豆只对 `GROUND_CRACK` 走正式承伤，其他普通地面伤害免疫不变。

雪锚果以 `WinterGroundImpactKind::GROUND_CRACK` 原子响应：当前格全部植物层仍承受当前完整伤害，结算完该格后才把后续倍率乘 0.5，左侧各层植物因此承受 250。`GameObjectManager` 强持有地裂，Board 只持弱引用；关卡快照保存行、连续前沿 X、下一待结算列和后续倍率，读档不重播提交音效或首段粒子。Save Schema v4 为旧关卡档补每波预算 0 与空地裂数组。

2026-08-25 整格伤害修正按主人要求不运行 AutoTest，只完成源码审计与 `clang-debug`/`clang-release` 编译；四层实际受击表现由主人实机验收。

## 2026-08-25 验证

`clang-release` 完整配置与全量 LTO 构建通过，主程序 Win7 导入审计通过 378 项；`SaveSchemaTests` 通过。`smoke_ice_crack_drill.json` 在当前桌面可见的默认 Vulkan 和 `-NoInstance` 路径均执行至 `commandIndex=131`、exit 0、`status=passed`，覆盖资源注册、数值、每波上限、冻土门禁、控制暂停、可重试中断、蓄力中快照、融化取消、地裂提交与快照、雪锚减半、盐蚀碎机、魅惑消费和图鉴预览。同步截图目验两路径的 50% 钻机、右手遮挡、微颤、三档破损与前移安全帽一致。

同次 `smoke_ice_wall_engineer.json` 默认 Vulkan 与 `-NoInstance` 均执行至 `commandIndex=128` 并通过，确认共用矿工帽来源的工程师三档安全帽也已镜像到脸侧、前移 3px，施工合同无回归。静态出怪审计确认 ID 44 只在绝对关卡 58/59/62/63，即 7-4/7-5/7-8/7-9。
