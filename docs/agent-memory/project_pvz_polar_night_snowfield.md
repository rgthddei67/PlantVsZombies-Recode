# 第八大关极夜雪原核心环境

## 当前边界

2026-08-28 已建立第八大关公共底座；完整规格见
`docs/superpowers/specs/2026-08-28-polar-night-snowfield-design.md`。同日后续已把 8-1/8-2、潜雪僵尸、
8-1 听雪草奖励、8-3/8-4 适应头盔僵尸和初版 `spawnlists.json` 接入；8-5～8-9 最终僵尸池仍待后续。新增内容必须继续复用这里的
背景、环境资格和正式波次创建边界，不能复制第二套雪原状态。

## 地图与资源

`Background::POLAR_NIGHT_SNOWFIELD` 是五行九列平地夜间背景：夜空不掉阳光、蘑菇不睡眠，
并集中拒绝旧雨势、台风、雾势、冬日花园寒潮/冻融线、径流和雷荷。权威背景
`build/clang-release/resources/image/background_polar_night.png` 由第一大关正视图构图改绘，随后按
Board 实际世界 Cell 边界 x=242..962、y=88..588 做分段校准；运行时贴图偏移换算后的九列五行
接缝与 80×100 Cell 对齐。左侧帐篷只作构图，不提供安全区或机制。

雪穴权威贴图为 `build/clang-release/resources/image/snow_hole.png`；两张纹理均走
`resources.xml`、`ResourceKeys.h` 和 AutoTest `GetTexture(key,false)` 闭环。左下常驻“极地观测站”
最终为 x=7、y=405、宽 170、高 147 的半透明中号面板，三行各 44px；底板完整包住末行风速条，
不覆盖任何 Cell。常态贴地流雪与湿度派生竖直落雪位于世界覆盖层；危险强风另由
`PolarWindUp/PolarWindDown.xml` 的近景长雪带、中景薄风带和卷起碎雪三层粒子表现，只保留最多三条
程序斜线提示上下风切，仪表与顶栏保持可读。风雪带从 12m/s 开始平滑淡入，18m/s 仍是唯一玩法
危险线；XML 以少量 `SpawnMinActive` 加 `SpawnRate` 逐层建立，避免跨线时整批粒子同帧弹出。

## Board 权威环境导演

温度、湿度、风速、目标值、真假计划、曲线时长、强风方向、三红累计、白毛风阶段和关卡脚本标志
均由 `Board` 保存。危险线为温度 ≤-18°C、湿度 ≥85%、风速 ≥18m/s；温度单独不处罚，
高湿连续 3 秒提交一批雪穴，强风立即让抛射物在发射时锁定向上/向下一行。隐藏计划在开始时一次
锁定并连续插值，假信号最多连续一次且最多两红；三红真计划连续 5 游戏秒后提交，随后 1.5 秒爬升、
45 秒雪盲和 2 秒无害淡出。危险保持与雪盲阶段每 1.8～3.2 秒锁定一次邻近微波动目标并平滑插值：
红项始终留红，蓝项始终留在带余量的安全区，波动与白毛风提交状态解耦；活动段与目标随 Board 入档。
雪盲结束后三项再用 5 秒连续回常态，高湿/强风效果按实际阈值退出而不瞬移。8-9 最终波雪盲为
60 秒。雪盲只在统一自动索敌入口限制真实三格半径，玩家视觉、已发射弹体、即时全场能力和手动操作不受影响。

8-1 只教学高湿且全关只提交一批雪穴；8-2 只教学强风；8-3 第一轮保证白毛风；8-4～8-8
直接运行完整导演；8-9 大波警告从当前实数平滑补齐三红，已有白毛风只延长、不重启。

2026-08-29 的架构拆分把上述导演、雪穴状态推进、强风偏行、地面绘制和测试入口机械迁入
`Game/Board/BoardPolarNight.cpp`；`Board/Board.h` 的公共接口、成员布局和存档键均不变。`Board/Board.cpp`
继续保留 `CreateOrQueueWaveZombie`、`SummonNextWave`、通用索敌等核心集成点，并通过窄入口
衔接 8-9 最终波，避免环境拆分重写正式波次边界。

## 雪穴与出生事务

每个高湿段只提交一次、选择两个不同候选行的第 5～7 列空格；2 秒形成期从首帧占格，形成后持续到
`SealSnowHole` 语义或同路小推车经过。形成视觉不再画程序几何圈：同一张手绘雪穴贴图在 2 秒内按
smoothstep 从 12% 缓慢放大到 100%，末段轻微回弹；逻辑格仍从首帧保留。普通攻击、灰烬、铲子和
降雪结束均不删除雪穴；同一行最多一个，全场同时最多五个。

`CreateOrQueueWaveZombie` 只拦截已经确定实际类型、扣除预算并选定行的正式右侧波次候选。活动雪穴
把这一只保存为 1 秒待提交事务；提交时入口仍存在就在洞口创建，否则回退 `SCENE_WIDTH+40`。
事务保存实际类型、行、原波次、锁定洞口和余时，读档不重抽；内部召唤、预览、开发者直造与
`CreateZombieWithID` 不改道。出生奖励在实体真正创建成功后分配。

## 存档与验证

关卡 schema 已随适应头盔来源链升级为 v9；v7→v8 只补 `polarNightInitialized=false`、`snowHoles=[]` 和
`pendingSnowHoleSpawns=[]`，v8→v9 补适应头盔预算和旧弹丸无来源单位元，不覆盖预发布字段。`GameInfoSaver` 保存并校验全部未来行为状态；非极夜
背景加载时规范化为空。

`clang-release` 完整构建和 Win7 378 项导入审计通过，`SaveSchemaTests.exe` 通过。桌面可见默认
Vulkan `smoke_polar_night_core` 共 84 条命令 exit 0，覆盖资源、分层风雪粒子、旧天气排除、5 秒提交、三格索敌、
抛射偏行/边界落空、两雪穴、正式波次延迟改道、封穴回退、粒子和快照往返；关键截图为
`01_polar_night_cell_aligned_background.png`、`02_whiteout_gauges_and_visible_board.png`、
`03_active_snow_holes_on_grid.png`，并断言普通小推车按实际 Cell 中心清除同行雪穴。末次 UI 增量后，`smoke_polar_night_ui` 24 条命令 exit 0，
`polar_night_layered_wind_down.png` 与 `polar_night_layered_wind_up.png` 目验观测站底板、末行风速条、
三层风雪密度和两种斜向正确；`polar_snow_hole_grow_small/mid/complete.png` 目验同贴图连续成长。
主人实玩反馈后的 `smoke_polar_night_dynamics` 59 条命令 exit 0，覆盖风效淡入强度、两红安全侧波动、
三红白毛风红区波动、两类波动快照以及白毛风结束后 5 秒连续回落；原 core 84 条、UI 24 条和
CTest 3/3 同步回归通过。

2026-08-29 拆分后重新配置并完整构建 `clang-release`，Win7 378 项导入审计与 CTest 3/3 通过；
桌面可见 `smoke_polar_night_core` 84、`smoke_polar_night_dynamics` 59、
`smoke_polar_navigation_contract` 65、`smoke_polar_night_ui` 24 条命令均 exit 0，
状态、日志以及白毛风/雪穴/仪表截图已核对。
