---
name: project-pvz-garlic
description: 经典大蒜的受伤外观、僵尸独立嫌恶换行状态、同介质行选择、存档与 grossout 头部对齐契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-08
---

# 经典大蒜与僵尸跨行反应

2026-08-08 完成 `PLANT_GARLIC`：50 阳光、7.5 秒冷却、400 生命；生命严格低于 2/3、1/3 时依次切换 `Garlic_body2/body3`，最低档隐藏三根茎。阶段只由已保存生命派生，读档走无反馈终态重建。

首口啃食照常造成 50 伤并播放普通 Chomp，随后在 `Zombie` 建立独立嫌恶状态，不能依赖仍存在的植物目标。普通脸品种在 0.7 秒停吃并显示 grossout，1.7 秒从同介质有效相邻行中选行、恢复走路，2.7 秒结束；缺专属脸的品种在 0.2 秒短停后直达换行节点。冻结和黄油暂停计时，寒冰减速不改时间轴。

换行直接修改权威 `mRow`；`EntityRegistry` 行桶和碰撞桶下一帧按当前行重建，无需增量通知。Transform Y 以 100px/游戏秒追赶 `Board::GetZombieSpawnY`，泳池不跨介质，屋顶按当前 X 的坡面目标收敛。phase、elapsed、是否已换行和中途 Y 全部入档，读档后重建停格速度层与恶心脸。

交互约束：停止啃食必须走统一目标清理以归零 `eaterCount`；破报纸先原子取消嫌恶状态再进入 `anim_gasp`；已经触发后被魅惑仍完成既定换行；死亡清理状态。通用 `Zombie_head_grossout` 比普通头图多 15px 顶部透明留白，只对 `anim_head1` 设 -15px 局部 Y offset，并在所有退出路径恢复原图、offset 和附属轨道。

验证证据：`clang-release` 配置/编译成功；当前桌面分别以默认实例化和 `-NoInstance` 可见运行 `autotest/scripts/smoke_garlic.json`，两次均为 196 条命令 exit 0、`run.log` 结束为 `script finished OK`。两路径的 Garlic `worldBounds` 与视觉中心整数投影完全相同，11 张同步截图逐张复核，覆盖三档受伤、普通/读报/无脸品种、停顿与换行中快照、破报纸、魅惑、泳池和屋顶；既有 `smoke_charm_paper_eating.json` 父类回归也通过并完成截图复核。
