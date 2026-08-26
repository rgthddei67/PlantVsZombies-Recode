---
name: project-pvz-zombie-butter-overlay
description: 全僵尸黄油头贴的语义轨道、最高层与跨层遮挡、存档生命周期和双绘制路径契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-26
---

# 全僵尸黄油头贴跟随与绘制层级

## 当前实现（2026-08-09）

- `Zombie::Start()` 在派生 `SetupZombie()` 完成后统一调用 `ConfigureButterSplatFollower()`。`GetButterSplatTrackName()` 默认返回 `anim_head1`；投篮车覆写为 `Zombie_catapult_driver_head`，冰车覆写为 `Zombie_head`。轨道或黄油贴图确实不存在时不配置 follower，保留旧稳定锚点后绘兜底。
- `ApplyButter()`、自然到期、断头、死亡、魅惑及 `LoadProtectedData()` 都只通过基类切换 follower 可见性；定身时间仍是权威存档状态，不额外保存派生的可见标志。巨人实际倍率为 `0.8 × 1.35 = 1.08`，其他品种默认 0.8。
- Animator 的 follower 保存 `drawAfterAllTracks` 策略。2026-08-26 起每个 `SparseTrackState` 以命名 `std::vector` 保存多个 follower；黄油使用稳定槽名 `butter_splat`，更新和隐藏只影响自己的槽。轨道可见性、自定义纹理和偏移仍留在连续热数组。普通品种默认把已计算好的 follower `InstanceRecord` 延迟到本 Animator 全部轨道与子附件之后提交，使黄油位于头发、眼镜等最高层；巨人覆写为 `false`，紧随 `anim_head1` 提交，让后续木杆、下巴和前臂继续自然遮挡。
- 同轨 follower 按插入顺序稳定提交；实例化路径仍向同一 bindless `InstanceRecord` 队列追加，不插入普通 draw 或 flush，`-NoInstance` 使用相同顺序和父轨矩阵进入既有矩阵 batch。冰像处刑者的 `ice_executioner_helmet` 与黄油同挂 `anim_head1`，证明后配置黄油不会覆盖派生类在 `SetupZombie()` 中建立的静态装备。

## 验证证据

- `smoke_zombie_butter_layers.json` 审计全部 36 个已注册僵尸的语义轨道，实际黄油覆盖普通、撑杆、读报、橄榄球、小鬼、舞王/伴舞、粉红橄榄球、门板、普通/精英投篮车与巨人；默认实例化和 `-NoInstance` 均可见 exit 0、日志 `script finished OK` 且无异常标记。
- 截图确认舞王黄油压在头发上方；巨人 `anim_smash` 中木杆仍遮住黄油，快照读档后层级与可见性保持。最终状态分别报告 `INSTANCE` 与 `NO_INSTANCE`，巨人为 `anim_head1`、`drawAfterAllTracks=false`。
- 共享 `smoke_kernelpult.json` 与 `smoke_catapult_zombie.json` 再次在主人当前桌面可见通过。投篮车脚本同步修正 2026-08-06 已把碰撞宽度从 153 改为 150、但旧断言未更新的基线漂移。
- 2026-08-22 稀疏轨道状态改造后，桌面可见 `smoke_zombie_butter_layers.json` 默认实例与 `-NoInstance` 各退出 0，各 79 条状态断言与 5 张截图通过；人工复核舞王黄油仍压住头发，巨人读档后的木杆仍遮住黄油。督军突击旗帜的真实子 Animator 附件由 `smoke_roof_marshal_assault_visual.json` 覆盖，退出 0、16 条断言与 2 张截图通过。
- 2026-08-26 命名 vector follower 改造后，`smoke_zombie_butter_layers.json` 默认实例与 `-NoInstance` 各 173 条命令通过；冰像处刑者专项在两条路径各 145 条命令通过，并在同步截图中同时显示 `butter_splat` 与 88% 红色头盔。警铃草、急救员和绝缘甲 follower 父回归也在两条路径全部通过。
