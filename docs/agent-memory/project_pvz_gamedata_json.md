---
name: project_pvz_gamedata_json
description: GameDataManager 数值外置 resources/gamedata.json：JSON 唯一数值来源+严格校验快速失败；含 GameApp.cpp 大小写、陈旧 msvc-debug 构建树两大坑
metadata:
  node_type: memory
  type: project
  originSessionId: 28339bd3-15f5-4711-b296-a6f7b0480af9
---

2026-07-07 完成（9f3e3ca + 0b37e9f，master 未 push）。backlog #2 落地：`GameDataManager` 注册表数值全部外置到运行时 `./resources/gamedata.json`。

**架构（主人四项裁决）：**
- 范围=仅注册表数值：植物 cost/cooldown/offset/scale、僵尸 weight/appearWave/survivalRound/offset/scale。**血量/速度/子弹伤害不做**（散在各类构造，主人嫌麻烦，二期候选）。
- **B 方案：JSON 是数值唯一来源**，代码只留身份注册（枚举名/纹理键/动画/工厂）——RegisterPlant 6 参、RegisterZombie 5 参。
- 快速失败：无 JSON/解析失败/缺条目/**缺任一字段**（主人原话"但凡只要少一个"）→ 收集全部错误一次性 LOG_ERROR + SDL_ShowSimpleMessageBox → Initialize() 返 false → InitializeResourceManager 失败路径 exit -6。多余键仅 WARN（防删类型后旧 JSON 锁死）。
- 弹窗有 `TestDriver::GetInstance().IsActive()` 守卫：AutoTest 模式不弹模态框，负向测试才能拿退出码。

**新增植物/僵尸现在是 5 步**：gamedata.json 加条目是新增的一步；2026-07-22 起只改 `build/clang-release/resources/gamedata.json`，其他 preset 通过目录联接共享。漏加会启动时指名道姓报错。

**验证**：demo_peashooter -Seed 42 基线含 run.log **全字节一致**；负向①缺文件②删 PLANT_CHOMPER.cooldown 均 -6 且错误带枚举名+字段名。

**Foot-guns：**
1. **仓库文件名是 `GameApp.cpp` 不是 `GameAPP.cpp`**（类名才是 GameAPP）。`git add` 大小写写错会静默漏 stage（Windows 文件系统不区分但 git 索引区分），提交后 `git status --short` 核对。
2. **旧历史：msvc-debug 资源副本长期不更新会让旧 exe 跑新资源并产生假崩溃。** 2026-07-22 已用指向 clang-release 权威资源的目录联接消除资源漂移；若 Debug 仍出现 ABI/LNK 问题，重 configure 后全量重建，不要删除保存原版资产的 `build/clang-release`。
3. **后台 run_in_background 的 PowerShell 不继承前台导入的 VS 环境**（cmake not found）——每条命令都要重新导 VsDevCmd。
