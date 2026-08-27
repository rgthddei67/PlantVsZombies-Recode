---
name: project_pvz_ice_statue_executioner_zombie
description: 第七大关冰像处刑者的蒙特卡洛选靶、来源绑定冰封、植物专属锤数、中断、命名多 follower 防具与存档契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-27
---

# 第七大关冰像处刑者

## 当前玩法合同

- `ZOMBIE_ICE_STATUE_EXECUTIONER` 为冬日花园强力机制僵尸：300 本体、2700 黑色橄榄球头盔、0.30 移速，总耐久 3000。7-7 首发，7-8/7-9 组合复习；正式波次每波最多五只，计数由 Board 保存并在换波清零。
- 完全进场后，从任一占地格位于实际冻土的活动持久植物与南瓜层中建立合法候选。蒙特卡洛开关开启时，用共享 48-rollout、16 秒短视推演选择对玩家防线损失最大的目标；关闭或推演失败时才回退到战略价值最高、同值稳定植物 ID 最小。处刑者和植物保存双向稳定 ID；目标进度只属于当前来源，不转移给其他处刑者。
- 冰封植物保留实体、格位、生命、Animator 帧和品种内部状态，但停止行为与 Animator，关闭 collider，并拒绝铲除、啃食/索敌、蹦极、台风移动、普通伤害、死亡和压扁。Board 生存再生和台风整组换格等直接修改路径也先检查冰封；植物弹丸不会与植物碰撞，因此自然越过冰像。
- 只有匹配来源 ID 的窄入口能提交处刑锤的 40 点普通伤害或最终处决。格温回暖解除冻土时释放目标并清零进度；来源死亡、断头、断臂或魅惑同样释放，但永久消费本次能力。头盔破裂或被磁力菇吸走不影响处决。
- 建立冰封前先由 Board 按稳定植物 ID 请求目标 3×3 内的炉芯花保护；成功消费一枚炉芯后，目标从未冰封且不受 40 点伤害，处刑者能力原子进入 `SPENT` 并退化为普通僵尸。无保护时才建立冰封，每锤复用扶梯 `anim_placeladder` 完整约 2.5 秒，播放结束先结算 40 点再加一格；普通植物第三格立即处决，玉米加农炮通过植物虚契约改为第七格处决。警铃草只重置当前尚未提交的完整前摇，已提交伤害与进度不回滚；处刑者不识别具体植物类型。
- 处决候选在推演中从 t=0 停止目标行为，但保留生命与阻挡，并按 2.5 秒、40 点和目标专属锤数推进；不能复用蹦极的立即删除近似。玉米炮按集中画像自动选择命中僵尸数最多、有效伤害次高、稳定 ID 最小的爆点，已经离膛的炮弹作为来源无关事件继续结算。

## 静态头盔与命名 follower

- 黑色橄榄球头盔没有独立时间轴，不使用子 Animator。它以稳定槽名 `ice_executioner_helmet` 跟随 `anim_head1`，穿戴缩放为原图 88%；三档损伤只更新该槽贴图，破帽只隐藏该槽。
- `Animator::SparseTrackState` 的 follower 已改为命名 `std::vector`。同一轨道可按插入顺序共存多个静态 follower，调用方只能更新/移除自己的槽。黄油使用 `butter_splat`，因此 `Zombie::Start()` 在派生 `SetupZombie()` 后配置黄油不会再覆盖自然生成头盔；警铃草铃舌、急救员装备和绝缘甲也都迁移到独立稳定槽名。
- 静态 follower 默认继承宿主 Animator 的状态 overlay；黑盔会随减速/冻结等效果一起变色。黄油配置显式传 `inheritOverlayEffect=false`，所以同挂 `anim_head1` 时仍保持黄色。实例与 `-NoInstance` 都按每个 follower 的 base 后紧跟 overlay 提交。
- 只有确实需要独立播放头、clip 或帧事件的附件才继续使用子 Animator。仓库旧 `AttachmentSystem` 没有玩法调用方，不是当前这两套表现路径的权威。
- 破帽使用单粒子 `ZombieIceExecutionerHelmetOff`。发射中心由 `anim_head1` 轨道原点、头盔局部偏移和贴图运行缩放后的中心计算，XML 发射偏移归零；离体粒子比例独立为 70%，避免把较大的穿戴比例直接复制到掉落物。

## 资源、存档与验证

- 身体复用 `Zombie_ladder.reanim` 并隐藏梯子，冰锤与冰像壳由 `scripts/generate_ice_statue_executioner_assets.ps1` 从锁定源图确定性生成。运行资源为 `Zombie_ice_executioner_maul.png` 86x84、`Ice_statue_shell.png` 112x120，以及独立的黑盔三档 `Zombie_ice_executioner_helmet*.png`，不会改色普通橄榄球僵尸。
- 植物灰烬伤害以词条缩放后的伤害和“当前本体 + 当前黑盔”总耐久比较：总耐久更高时走正式 `PLANT_ASH` 扣血链，1800 灰烬从 2700 黑盔扣到 900、本体仍为 300；总耐久不高于伤害时才直接化灰。失去头盔后自然只统计本体。
- 植物保存冰封来源 ID；僵尸保存阶段、目标 ID、已提交进度和能力消费状态；Board 保存每波数量。加载后由 Board 终检双向关系、冻土与终止态，不合法组合原子释放，不复活目标或继承进度。
- `smoke_ice_statue_executioner.json` 当前共 188 条命令，覆盖资源/数值/出怪、自然生成黑盔、同轨黄油共存和 overlay 分流、价值选择、全面隔离、三锤、警铃重置、破帽专属粒子、磁力菇摘盔但不中断处刑、灰烬总耐久阈值、回暖、死亡/断肢/魅惑、快照和每波五只上限。2026-08-26 在当前桌面可见的 `clang-release` 默认实例执行至 command 187、exit 0、`status=passed`；同次炉芯花专项在默认实例与 `-NoInstance` 两条路径均通过，并覆盖处刑者真实封存入口。
- 2026-08-27 可见 `clang-release` 新专项 `smoke_ice_executioner_cob_monte_carlo.json` 47 条命令通过：只有最右一列冻土时，左锚点 7 列、右轮 8 列的玉米炮仍被 MC 选中并持续冰封；第六锤后进度 6、植物仍存活，快照往返保持关系，第七锤消失。原 `smoke_ice_statue_executioner.json` 同次复跑至 command 187、exit 0、`status=passed`；纯数值测试另锁定 16 秒窗口只提交六锤、玉米炮最优爆点和离膛炮弹不可回滚。
- follower 父回归同次在两条路径各通过：`smoke_zombie_butter_layers` 173 条、`smoke_alarm_bell_flower` 120 条、`smoke_healer_area` 35 条、`smoke_insulator_visual` 30 条。全量链接的 Win7 导入审计通过 378 项。
