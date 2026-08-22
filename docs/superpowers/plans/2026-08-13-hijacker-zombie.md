# 劫持者僵尸实施计划

> 对应规格：`docs/superpowers/specs/2026-08-13-hijacker-zombie-design.md`

## Task 1：先建立失败的类型、资源和基础行为测试

**Files**

- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Create: `autotest/scripts/smoke_hijacker_basic.json`
- Create: `autotest/scripts/smoke_hijacker_resources.json`

1. 在 AutoTest 僵尸名称表和状态投影中预留 `ZOMBIE_HIJACKER`、锁定阶段、可计生命、资源加载与粒子计数字段。
2. 脚本覆盖普通生成、啃咬 50、断臂、断头、魅惑、资源键。
3. 运行最小脚本，确认在类型/工厂未注册时按预期失败，保留失败证据。

## Task 2：新增类型、工厂与独立状态机

**Files**

- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`
- Create: `PlantVsZombies/Game/Zombie/HijackerZombie.h`
- Create: `PlantVsZombies/Game/Zombie/HijackerZombie.cpp`
- Modify: `PlantVsZombies/Game/Plant/GameDataManager.cpp`
- Modify: `PlantVsZombies/ResourceKeys.h`
- Modify: `build/clang-release/resources/gamedata.json`
- Modify: `build/clang-release/resources/zombieinfo.json`

1. 枚举尾部追加 `ZOMBIE_HIJACKER`，不改现有数值。
2. 注册 `HijackerZombie : Zombie` 与 `ZombieHijacker` reanim。
3. 实现 1000 基础生命、首次锁定当前/最大生命各 +1000、普通速度、50 啃咬与 `NORMAL/LOCKED/FINALIZING/RESOLVED` 状态；一次性增量标志入实体档，恢复锁定时禁止重复叠加。
4. 注册主人批准的帧事件：啃咬 45、死亡回收 89；不为处决新增帧事件。
5. 实现可计生命、最后 1 秒原子停止、死亡/掉头取消、断臂不取消、魅惑保留技能与存档恢复。

## Task 3：专用弱索引与 Board 锁定状态

**Files**

- Modify: `PlantVsZombies/Game/EntityRegistry.h`
- Modify: `PlantVsZombies/Game/EntityRegistry.cpp`
- Modify: `PlantVsZombies/Game/Board.h`
- Modify: `PlantVsZombies/Game/Board.cpp`

1. 增加按 ID 有序的 `HijackerZombie` 弱索引，接入正常生成、指定 ID 读档、同 ID 覆盖与周期清理。
2. 提供只遍历劫持者候选的 75% 选择入口；比较当前可计生命，收集同最高值候选并按 ID 排序后随机。
3. Board 增加本轮 75% 已尝试、选择 ID、延长预警、最后 1 秒标志。
4. 修改 `AddNightRoofCharge`/`UpdateNightRoofCharge`：场上存在至少一只有效劫持者时，雨中固定额外 +1.5 点/秒且多只不叠加；75% 只锁定一次；100% 选择 7 秒或既有 4 秒；最后 1 秒通知实体；死亡/掉头只取消处决，不改变普通放电计时。
5. 不改变雷荷过载、行选择、基础放电伤害与放电可见阶段。

## Task 4：冻结目标快照与批量处决

**Files**

- Modify: `PlantVsZombies/Game/Board.h`
- Modify: `PlantVsZombies/Game/Board.cpp`
- Modify: `PlantVsZombies/Game/Plant/Plant.h`
- Modify: `PlantVsZombies/Game/Plant/Plant.cpp`（仅当正常死亡入口需要窄扩展）
- Modify: `PlantVsZombies/Game/Zombie/Zombie.h`
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`（仅当专属闪光/正常死亡入口需要窄扩展）

1. 提供统一的僵尸当前可计生命函数：body+helm+shield，明确排除 extra。
2. 在处决放电边沿先冻结植物格组与僵尸目标 ID。
3. 植物按 normal+pumpkin 组值判断，under 免疫，overlay 不独立；目标组把两个战斗层 ID 一并入快照。
4. 施法者先失去碰撞/索敌资格并进入死亡表现，再批量让快照目标走正常死亡入口。
5. 普通雷荷放电随后结算；验证普通伤害不会向处决快照追加目标，其他劫持者不会连锁。
6. 生存模式仅对处决线应用 1200 上限。

## Task 5：动画、位图与粒子资产

**Files**

- Create: `scripts/generate_hijacker_assets.ps1`
- Create: `build/clang-release/resources/reanim/Zombie_Hijacker.reanim`
- Create: `build/clang-release/resources/image/reanim/*Hijacker*.png`
- Create: `build/clang-release/resources/particles/config/HijackerElectricFlash.xml`
- Create: `build/clang-release/resources/particles/config/HijackerHeadOff.xml`
- Create: `build/clang-release/resources/particles/config/HijackerArmOff.xml`
- Modify: `build/clang-release/resources/resources.xml`
- Modify: `build/clang-release/resources/manifest.txt`

1. 用可复现脚本复制 `Zombie_JackBox.reanim`，改名 `anim_pop -> anim_hijack`，替换盒子、手柄、打开核心、伸缩叉、服装、护目镜和断臂资源引用。
2. ImageGen 概念板只用于造型语言；最终 PNG 由可复现流程按现有轨道画布尺寸制作，保留手绘质感、分层明暗、磨损和结构细节，保持透明边缘和低分辨率可读性。禁止把接收器、线圈、护目镜或服装退化成纯色矩形、单线条或儿童画式占位。
3. 紫色电气闪光用白/浅色源图乘法染色；爆发 XML 必写 `SystemDuration`。
4. 头/臂粒子从对应轨道世界坐标发射，XML 局部偏移保持 0。
5. 核对 manifest、`resources.xml`、ResourceKeys 与运行时实际键闭环。

## Task 6：声音、天气面板与无全场扫描描边

**Files**

- Modify: `PlantVsZombies/Game/Zombie/HijackerZombie.cpp`
- Modify: `PlantVsZombies/Game/Board.h`
- Modify: `PlantVsZombies/Game/GameScene.h`
- Modify: `PlantVsZombies/Game/GameScene.cpp`
- Modify: `PlantVsZombies/Game/Plant/Plant.cpp`
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp`

1. 75% 声明低电流循环声，100% 增加警报脉冲，最后 1 秒切急促反馈；取消/死亡/结算与析构都释放声明。成功处决另在批处理提交边沿播放一次独立碎裂声，不按目标数量叠加。
2. 天气面板通过 Board 选择 ID O(1) 显示实时 `处决线：N`。
3. 植物与僵尸各自在现有 Draw 内做一次 O(1) 阈值查询并叠紫色脉冲；植物用格坐标直查 normal+pumpkin。
4. 不新增每帧 `GetAllPlantIDs/GetAllZombieIDs` 或类型全扫。

## Task 7：冒险、生存出怪编排

**Files**

- Modify: `build/clang-release/resources/gamedata.json`
- Modify: `build/clang-release/resources/spawnlists.json`
- Modify: `PlantVsZombies/Game/Board.h`
- Modify: `PlantVsZombies/Game/Board.cpp`
- Modify: `PlantVsZombies/Game/GameApp.cpp`（如预览/解锁表集中在此）

1. 权重设 2000、`appearWave=9`，每波上限 2。
2. 6-4 池精简并在第 7 波走正式保证单只路径；保证只计一次并进入存档/状态投影。
3. 6-5 加入劫持者并保留现有组合池。
4. 生存第 15 轮起加入出怪池；第 14/15 轮做边界断言。

## Task 8：完整存档与确定性 AutoTest

**Files**

- Modify: `PlantVsZombies/Game/GameInfoSaver.cpp`
- Modify: `PlantVsZombies/Game/AutoTest/TestDriver.cpp`
- Create: `autotest/scripts/smoke_hijacker_lock.json`
- Create: `autotest/scripts/smoke_hijacker_execution.json`
- Create: `autotest/scripts/smoke_hijacker_layers.json`
- Create: `autotest/scripts/smoke_hijacker_save.json`
- Create: `autotest/scripts/smoke_hijacker_spawn.json`
- Create: `autotest/scripts/smoke_hijacker_visual.json`

1. Board 存读 75% 尝试、选择 ID、延长预警与最后阶段；Hijacker 存读自身展示状态。
2. 扩充 `set_night_roof_charge` 或增加窄夹具，使 74.9→75、99.9→100、1 秒边沿可确定性触发。
3. 状态投影导出候选数、选择 ID、有效性、处决线、是否延长、是否最后 1 秒、每个实体可计生命和 doomed 标志。
4. 覆盖规格验收矩阵及三处快照往返；断言读档后不重选。
5. 同步截图验证紫色锁定标记、面板、最后 1 秒、植物组描边、处决闪光和正常死亡。

## Task 9：构建、可见验证与回归

1. 导入 VS 开发者环境后运行：

   ```powershell
   cmake --preset clang-release
   cmake --build --preset clang-release
   ```

2. 从 `build/clang-release` 用提升权限的 `Start-Process -WindowStyle Normal -PassThru` 在主人当前桌面依次运行最小专项脚本。
3. 每个脚本检查进程退出码、`run.log`、`status.json`/dump、全部断言和 PNG；用 `view_image` 逐张核对最终低分辨率部件与完整合成，重点检查材质层次、轮廓、磨损和缩放后的可读性，不接受几何占位感。
4. 至少回归普通雷荷、JackInTheBox、魅惑、断肢、波次上限、冒险 6-4/6-5 与生存出怪脚本。

## Task 10：契约审计、记忆与交付

1. 审计 `adding-zombie`、`adding-particle` 及其 references；把本次可复用的 75% 稀有索引、冻结批量处决、Board-实体交叉引用读档、粒子锚点契约同步进技能。
2. 如技能有修改，先完整读取 `skill-creator/SKILL.md`，再运行其 `quick_validate.py` 校验全部改动技能。
3. 更新 `docs/agent-memory/project_pvz_sixth_area_night_roof_backlog.md` 与 `docs/agent-memory/MEMORY.md`，把“记录待实现”改为当前实现/验证证据。
4. 审查 `git diff` 和资源哈希，确认没有用户无关改动；提交完成且验证通过的工作。
5. 当前分支与上游可常规 fast-forward、工作区范围纯净时 push；否则保留本地提交并说明。

## 完成定义

- 规格中的机制、数值、动画、视觉、出怪、存档和性能契约全部落地。
- `clang-release` 零错误零警告。
- 专项与相关回归全部在当前桌面可见运行且退出 0。
- 最终截图证明默认实例化路径；涉及 Animator 资源的关键静态视觉另跑 `-NoInstance` 对照。
- UI/Draw 常态路径不存在新增全场遍历，只有实际处决事件执行一次冻结快照扫描。
