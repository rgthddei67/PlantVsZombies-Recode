# 精英舞王僵尸实现计划

**目标：** 实现主人批准的紫衣精英舞王、台风出生变异、八伴舞补召及“本行触发、其余行吞噬”的小推车机制，并完成可见 AutoTest 闭环。

**技术路线：** `EliteDancerZombie` 继承 `DancerZombie` 以复用同 reanim 帧事件和残肢契约；Board 在正式波次选型后一次解析实际类型；小推车通过虚拟能力查询走无触发清除；紫色资源由 PowerShell 对红色布料做 HSV 换色并生成独立 reanim。

**设计依据：** `docs/superpowers/specs/2026-07-21-elite-dancer-zombie-design.md`

## 任务 1：注册链与紫色资源

- [x] 增加 `ZOMBIE_ELITE_DANCER`、独立 AnimationType、ResourceKey、工厂注册和开发/测试名表。
- [x] 两份 gamedata 增加完整五字段且 `weight=0`；两份 info 增加图鉴文案。
- [x] 新增并运行 `scripts/recolor_elite_dancer.ps1`，生成 11 张紫色服装部件和两份 `Zombie_EliteJackson.reanim`。
- [x] 两份 resources.xml 注册 `ZombieEliteJackson`。

## 任务 2：精英本体与八伴舞召唤

- [x] 新增 `EliteDancerZombie.{h,cpp}`，调用舞王 Setup 后覆盖 800 血。
- [x] 无视植物但保留敌对僵尸互啃；绕过普通舞王定身状态机，始终推进。
- [x] 每 0.4 秒补一名、最多八名；实现阵营有效性、八点生成循环、魅惑继承与存档。
- [x] 将超强台风 1.70 倍接入统一 Animator extra 速度收敛点。

## 任务 3：天气出生变异与存档

- [x] Board 增加 25% 正式解析器，在 `TrySummonZombie()` 选型后调用且仍扣普通舞王成本。
- [x] 同一波最多两只；正式进入新波时重置，天气切换不重置。
- [x] 保存/恢复本波成功生成数量，旧版单台风布尔字段兼容迁移且读档不重 roll。
- [x] 台风开始、衰减、停止与恢复时刷新精英的派生速度。

## 任务 4：小推车吞噬

- [x] Zombie 增加默认关闭的“吞噬其他行 mower”能力查询，精英覆写为 true。
- [x] Mower 碰撞先正常触发当前行，再处理其他行吞噬分支。
- [x] Board 保留当前 mower、无触发销毁其他行 mower 并更新失车状态；当前 mower 正常消灭精英。

## 任务 5：AutoTest

- [x] 增加 `spawn_wave_zombie` 正式解析入口。
- [x] dump 增加本波生成数量、小推车数量/行号/状态及精英能力字段。
- [x] 新建专项脚本覆盖天气变异、召唤封顶、忽略植物、速度、冻结/减速、魅惑、小推车和死亡。
- [x] 截图检查紫衣、八伴舞站位与当前行小推车启动、其他行清空后的精英死亡。

## 任务 6：验证与交付

- [x] `cmake --preset clang-release` 与 `cmake --build --preset clang-release` 零警告。
- [x] 当前桌面可见运行精英舞王 smoke，保留窗口确认、退出码、run.log、状态 JSON、断言和截图。
- [x] 运行最小相关既有回归（普通舞王召唤及精英台风速度/变异）。
- [x] 更新天气、舞王主题及 MEMORY 路由；核对忽略资源已强制加入暂存区。
- [x] 提交已验证改动，并在工作树和上游状态安全时推送。

## 最终调参表

| 位置 | 参数 | 值 | 含义 |
|---|---|---:|---|
| `EliteDancerZombie.cpp` | `kEliteDancerHealth` | 800 | 本体基础血量 |
| `EliteDancerZombie.cpp` | `kMaxActiveBackupDancers` | 8 | 直属普通伴舞存活上限 |
| `EliteDancerZombie.cpp` | `kBackupSummonInterval` | 0.4 s | 缺员时每只补充间隔 |
| `EliteDancerZombie.cpp` | `kSuperTyphoonSpeedMultiplier` | 1.70 | 仅 `SUPER` 的额外动作/移动倍率 |
| `EliteDancerZombie.cpp` | `kSummonSideDistance` | 100 px | 八点编队的前后横向距离 |
| `EliteDancerZombie.cpp` | `kSummonFrontMinX` | 130 px | 过近房屋时跳过前方生成点 |
| `Board.cpp` | `kEliteDancerMutationChancePercent` | 25% | 合资格普通舞王的变异概率 |
| `Board.cpp` | `kEliteDancerMaxPerWave` | 2 | 每波成功生成上限 |
| `gamedata.json` | `weight / appearWave / survivalRound` | `0 / 8 / 4` | 不独立抽取，继承普通舞王出现阶段 |
| `gamedata.json` | `offset / scale` | `[-50,-85] / 1.0` | Jackson 视觉站位 |
