---
name: pvz-flag-meter-non-multiple-waves
description: 非整十总波数下旗帜进度条的位置、升旗索引和验证证据
metadata:
  node_type: memory
  type: project
  updated: 2026-07-18
---

# 非整十波旗帜进度条

## C# 原版口径

参考 `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Board\Board.cs`：

- `GetNumWavesPerFlag()` 的常规值为 10。
- `DrawProgressMeter()` 令 `i` 从 1 到 `mNumWaves / numWavesPerFlag`，每面旗对应真实波号 `i * numWavesPerFlag`，再以总波数为完整时间轴从进度条右端线性映射到左端。
- C++ `FlagMeter` 的位置参数是从左到右的 `0..1`，所以第 `k` 面旗应使用 `1 - k * 10 / maxWave`。例如：15波=`1/3`；25波=`3/5, 1/5`；35波=`5/7, 3/7, 1/7`。

旧实现从 `i=0` 开始使用 `i*10/maxWave`，等于错误生成了一面“第0波旗”，并把后续旗帜的方向和升旗索引一起反转；总波数不是10的整数倍时最明显。

## 当前实现约束

- `GameProgress::SetupFlags()` 按第10、20、30……波的顺序保存旗子，位置使用上述原版映射。
- `GameProgress::Update()` 在第10的倍数波直接用 `currentWave / 10 - 1` 升起对应旗帜。
- `InitializeRaisedFlags()` 必须沿同一存储顺序从索引0开始恢复；不要再从 `m_flagCount - 1` 反向恢复，否则读档与实时表现会分叉。

## 2026-07-18 验证

- `cmake --preset clang-release` + `cmake --build --preset clang-release` 通过，无新增警告。
- 当前桌面可见 AutoTest 分别临时设置15/25/35总波数：退出码均为0，`run.log` 均 `script finished OK`，状态均为关卡2-6、`boardState=GAME`、`wave=1`，截图分别显示1/2/3面旗且位置符合公式。
- 25波开发者面板连续推进到第10波：状态断言 `wave == 10` 通过，截图确认右侧第10波旗面升起。临时脚本已删除；证据保留在 `build/clang-release/autotest/out/tmp_flag_meter_*`。
