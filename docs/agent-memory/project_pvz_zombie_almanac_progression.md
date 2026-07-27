---
name: project_pvz_zombie_almanac_progression
description: 僵尸图鉴由已通关关卡 spawnlist 推导可见项，当前关不提前泄露；含必然召唤单位展开与 UI AutoTest 状态抓手
metadata:
  node_type: memory
  type: project
  originSessionId: 019fa16d-d877-7b71-8530-65f12c3330ca
---

# 僵尸图鉴随冒险进度解锁

2026-07-27：`ZombieAlmanacScene` 不再枚举全部已注册僵尸。`GameAPP::mAdventureLevel` 是当前待玩的关卡，因此只累计 `spawnlists.json` 中 `level < mAdventureLevel` 的合法已注册类型；按关卡号与数组顺序去重，得到稳定的首次遭遇顺序。第 3 关只显示前两关见过的普通僵尸，第 3 关首次出现的路障要到第 4 关才解锁。

`weight: 0` 的召唤型子单位不能为了图鉴解锁写入随机池。当前把舞王与精英舞王显式展开为伴舞这一项必然遭遇；以后新增必然生成的附属僵尸时，同步扩展这一映射。天气概率变异与按行地形替换不假定玩家必然遇见，除非后续新增真实遭遇存档。

AutoTest 的 `dump_state` / `assert_state` 现支持 `ZombieAlmanacScene`，字段为 `scene`、`adventureLevel`、`zombieAlmanacEntries`、`zombieAlmanacEntryCount`、`zombieAlmanacSelected`。`smoke_zombie_almanac_progression.json` 可见验证第 3/4 关边界并保留两份状态 JSON 和截图；`smoke_dancer_almanac.json` 先设进度为 17，回归舞王与伴舞详情。两脚本在 `clang-playtest` 当前桌面可见运行均 exit 0。
