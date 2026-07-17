---
name: project_pvz_improvement_backlog
description: "PvZ 非性能改进待办清单（#2-#8）；#1 注册式工厂已选中单独开工，其余暂存"
metadata:
  node_type: memory
  type: project
  originSessionId: 46e3568e-2999-484f-9d6d-47b9b5702afe
---

2026-05-31 全项目探索后列出的改进 backlog（除新植物/僵尸/子弹外）。性能线已做到很深（见 [project_pvz_perf_optimization](project_pvz_perf_optimization.md) 等），以下聚焦架构/工程/质量。**#1 注册式工厂主人已选中、单独开工，不在此列。** 其余按性价比排序，均非 bug，是"以后改东西更省力/更不易踩坑"类。

**#2 游戏数值 JSON 化（中优先）**
- 现状：植物/僵尸 cost/cooldown/数值全硬编码在 `GameDataManager.cpp:26` 起的 `InitializeHardcodedData`（RegisterPlant/RegisterZombie 几百行）。
- Why：调一次平衡就得重编译。关卡刷怪表已成功外置（`Board.cpp:572` `LoadSpawnListFromJson` → `spawnlists.json`，含去重+范围校验，是好范式）。
- How：把植物/僵尸数值也搬进 JSON，复用 spawnlists.json 的数据驱动风格。

**#3 GameAPP::Run 初始化/清理阶梯 RAII 化（中优先）**
- 现状：`GameApp.cpp:241-309` 每个 init 失败分支手写逆序 reset()/Quit() 链，越往后越长，易漏/错序 → 泄漏或析构崩溃。
- How：RAII scope-guard 栈（成功后压清理 lambda，正常路径 release），或 `int initStage` + 单个 `CleanupTo(stage)` + 统一 goto cleanup。

**#4 统一日志系统（✅ 已完成 2026-06-06）**
- 完整 API/约定见 [project_pvz_logging_system](project_pvz_logging_system.md)。已建 `Logger.h/.cpp`，全量迁移 ~193 处 cout/cerr + 29 处 Renderer fprintf；**GameMonitor.cpp(70) 经主人指示不迁移**（基本不用）。Debug 五级 / Release 仅 WARN/ERROR，主人 VS 构建实测通过。

**#5 无自动化测试（已知约束）**
- CLAUDE.md 明确无测试框架且禁止 assistant 擅自搭建。仅备忘：`SelectSpawnRow`(`Board.cpp:362` 加权选行)、`GetWeightedRandomZombie`、存档 JSON 往返等纯逻辑最适合切入。

**#6 魔法数字（低优先）**
- 已有命名常量很好；残留裸数字：`Board.cpp:72-73` 爆炸半径 130；`Board.cpp:209-210` 阳光落点 50~770 / -110~-20；`Board.cpp:735` `140+row*100`、`Board.cpp:695` `135+row*100`（行高 100 到处复制 → 提 ROW_HEIGHT）；`Board.cpp:497` 刷怪点数 `(wave/3+1)*1000`。

**#7 Definit.h Vector(glm::vec4) 隐式转换 footgun（✅ 已完成 2026-06-25，commit 2cad531 未push）**
- `Definit.h:30` 的 `glm::vec4→Vector` 已加 `explicit`（只取 x,y 的有损降维从自动转换降级为必须显式 `Vector(v4)`）。**全库扫描确认无真实隐式 vec4→Vector 调用点**（vec4 全是颜色参数 / 矩阵变换后立即 .x/.y 取分量，TransformPoint 早已手写 `Vector(result.x,result.y)`），故编译零影响。`line 27` vec2↔Vector 隐式互通（无损，有意）与 SDL_FPoint 隐式转换均保留。与颜色 0..255 / 预乘 alpha 同类静默语义错配隐患（见 [reference_pvz_color_0_255_convention](reference_pvz_color_0_255_convention.md) [project_pvz_premultiplied_alpha](project_pvz_premultiplied_alpha.md)）。
- **附带裁决（主人 2026-06-25 问"Vector 是否该全改 glm::vec2"）：不。** Vector=项目位置词汇类型，602 处/93 文件深嵌；它已是 glm::vec2 之上的人体工学包装（双向隐式互通 + `.magnitude/.normalized/.dot` 成员 API + `Vector::up()` 带屏幕坐标约定的静态工厂 + 隐式 SDL_FPoint）。全量换 vec2 = 高成本（改写几十处成员→glm 自由函数、丢 SDL 互通）零收益。价值正在于这套精心设计的转换边界（对 SDL/vec2 隐式、对 vec4 显式）。`map/set<Vector>` 当前 0 处 → operator</> 基本死代码可单独清。

**#8 .cpp 内成员函数误用 inline（低优先）**
- `Board.cpp` 的 CleanupExpiredObjects/SelectSpawnRow/TrySummonZombie 等标了 inline，但定义在 .cpp 只本 TU 用，对跨 TU 内联无作用。无害，删更诚实。
