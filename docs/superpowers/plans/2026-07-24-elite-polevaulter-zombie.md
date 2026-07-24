# 精英撑杆僵尸实现计划

**目标：** 实现绿色精英撑杆、双倍跳距、落地生成普通撑杆，并加入冒险 3-3/3-4。

**技术路线：** 派生类复用 `Polevaulter` 全套帧事件；父类落地流程只增加虚拟跳距和落地钩子；PowerShell 精确换色红蓝材质；用专项行为脚本和整段泳池出怪表脚本完成可见验收。

**设计依据：** `docs/superpowers/specs/2026-07-24-elite-polevaulter-zombie-design.md`

## 任务 1：注册链与绿色资源

- [x] 增加僵尸枚举、动画枚举、ResourceKey、工厂注册以及开发/AutoTest 名表。
- [x] 增加完整 gamedata 五字段与图鉴文案。
- [x] 新增并运行换色脚本，生成绿色部件、绿色掉头贴图与独立 reanim，并锁定逐文件 SHA-256。
- [x] 注册独立 reanim、粒子贴图与绿色掉头粒子配置。

## 任务 2：精英行为

- [x] `Polevaulter` 落地流程增加虚拟跳距与落地钩子，普通行为保持 150px。
- [x] 新增 `ElitePolevaulterZombie`，覆盖 600 HP、1.2 动画倍率与 300px 跳距。
- [x] 落地位移完成后在同排同 X 生成普通撑杆。
- [x] 复核跳跃中读档、落地后独立实体存档及魅惑边界。

## 任务 3：冒险出怪

- [x] `spawnlists.json` 新增 3-3：15 波，普通/路障/精英撑杆。
- [x] 新增 3-4：20 波，普通/路障/普通撑杆/铁桶/粉色橄榄球/精英撑杆。
- [x] 静态校验 JSON、枚举范围、工厂、正权重与 Junction 权威资源。

## 任务 4：AutoTest

- [x] 状态 JSON 增加撑杆阶段、已跳标志与实际跳距整数投影。
- [x] 新建精英专项脚本，覆盖血量、速度、双倍跳距、落地生成、绿色截图和死亡消失。
- [x] 新建 3-1 至 3-4 出怪表脚本，覆盖完整池与 3-3/3-4 预览截图。

## 任务 5：验证与交付

- [x] `clang-playtest` 配置、构建零警告。
- [x] 当前桌面可见运行两份专项脚本与普通撑杆回归，检查窗口、退出码、日志、状态与截图。
- [x] `clang-release` LTO 发布构建零警告。
- [x] 更新泳池/精英撑杆项目记忆并检查忽略资源强制暂存；清单完成后提交并按仓库状态决定 push。

## 最终调参表

| 位置 | 参数 | 值 | 含义 |
|---|---|---:|---|
| `Polevaulter.cpp` | `kNormalVaultDistance` | 150 px | 普通撑杆跳跃逻辑位移 |
| `ElitePolevaulterZombie.cpp` | `kElitePolevaulterHealth` | 600 | 精英本体血量 |
| `ElitePolevaulterZombie.cpp` | `kEliteVaultDistance` | 300 px | 精英跳跃逻辑位移 |
| `ElitePolevaulterZombie.cpp` | `kEliteAnimationSpeedMultiplier` | 1.2 | 精英相对普通的动画倍率 |
| `gamedata.json` | `weight / appearWave / survivalRound` | `2300 / 5 / 5` | 抽取权重兼成本、最早波次、生存轮次 |
| `gamedata.json` | `offset / scale` | `[-50,-85] / 1.0` | 视觉站位 |
| `spawnlists.json` | 3-3 | `15 / [普通,路障,精英]` | 精英独立教学 |
| `spawnlists.json` | 3-4 | `20 / [普通,路障,撑杆,铁桶,粉色橄榄球,精英]` | 撑杆家族综合与高速压力 |
