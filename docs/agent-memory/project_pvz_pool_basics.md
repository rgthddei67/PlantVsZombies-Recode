---
name: project_pvz_pool_basics
description: 2026-07-24 第三大关泳池基础与 3-1 至 3-4 出怪；含六行、睡莲双层占格、地形僵尸替换、PoolCleaner 和旧档边界
metadata:
  node_type: memory
  type: project
---

# 第三大关泳池基础系统

## 当前范围（2026-07-24）

已开放冒险 3-1（level 19）至 3-4（level 22）的基础系统与出怪表。3-3 引入绿色精英撑杆，
3-4 加入普通撑杆、铁桶、粉色橄榄球与精英撑杆组合压力；3-5～3-9 出怪表和其他泳池植物仍未实现，
后续必须逐关推进，不得将本基础当作整个第三大关已完成。

资源由主人提供：泳池地图、睡莲/三种水路僵尸动画、`PoolCleaner` 已在 `resources.xml` 注册。2026-07-23 后续任务又从 `D:\PVZ\原！版！Test\images` 导入原版水面底图、AlphaGrid 阴影蒙版和焦散源图，增加动态水面；仍未新增通用入水水花或额外泳池粒子，PoolCleaner reanim 自带轨道不在此限制内。

## 核心契约

- `Background::WATER_POOL` 与其他背景共用 `mBackgroundY`，不再额外上移；日间/夜间泳池的六行网格首行顶部均为 Y=85。泳池使用 6 行、85px 行高，0-based 第 2/3 行是水路，列宽仍为 80px。网格派生位置必须通过 `Board::GetCellCenterPosition/GetCellHeight`。
- `GameScene` 在静态泳池背景之后、游戏对象之前调用 `Graphics::DrawPoolEffect`。专用 Vulkan pipeline 每帧绘制原版 `15×5` 规则网格的底图/阴影/焦散三层（每层 450 顶点、共 3 draw call）；`pool.vert` 用原版五组相位在 GPU 扭曲三层 UV，`pool.frag` 从静态 `256×256` 灰度源图复刻双线性焦散与字节阈值，不创建或上传逐帧动态纹理。日间/夜间分别使用原版 base/shading，夜间焦散按原版降亮；水面复用 matrix/bindless descriptors、相机 `projView` 和逐顶点 shader ClipRect，保持屏幕抖动一致且不增加动态 scissor。
- `Cell` 有 `under/normal` 两个植物槽。睡莲只能放在空水格的 under 层；普通植物只能放在已有睡莲且 normal 层为空的水格；土豆雷仍禁止下水。铲子、僵尸啃咬与 UI 预览都以 top 层为准。
- 水格已有睡莲时，卡片悬停生成的半透明落点预览必须取当前 top 植物的 `renderOrder + 1`，保证待种植物显示在睡莲上方；空格预览使用 `LAYER_GAME_PLANT`。
- 睡莲种下后 1 秒只免疫啃咬，计时器进存档；水面植物只做±2px 绘制浮动，Transform/碰撞与存档仍固定在逻辑格。
- 普通/路障/铁桶僵尸在水路由正式波次入口替换为 `ZOMBIE_POOL_*`，而非为出怪表添加独立权重；这三种专用类型仍只允许水路，陆地版本不会实际创建在水中。其他僵尸可直接抽到水路并保留自身类型与行为。`Board::CanZombieTypeSpawnInPool` 是集中禁水扩展点，当前所有有效僵尸类型均允许；未来只在此追加禁水类型。`Zombie` 基类用前/后双探针统一维护 `mInPool`，本项目将下水/出水切换位置相对原探针向右校正 70px；水中统一隐藏陆地阴影，并在 `Zombie::Draw` 内用 `PushClipBottom/PopClipBottom` 把水面以下裁掉。该接口现为通用 shader `PushClipRect` 的全宽别名，不触发批处理 flush 或 `vkCmdSetScissor`，并自动与伴舞出土等既有矩形 Clip 取交集。专用泳池僵尸仍保留 swim、水中死亡和泳圈贴图；水中断头/断臂不生成悬浮的陆地碎片。
- 泳池僵尸仅在 `mInPool=true` 时拒绝化灰：植物爆炸仍走正式伤害与水中死亡轨道，但不创建 `ZombieCharred` 烧焦残影；尚未下水或已经离水时继续沿用陆地化灰表现。这与 C# `ApplyBurn` 的 `mInPool -> DieWithLoot` 分支一致。
- 所有僵尸生成 Y 由网格行中心派生。第一、二大关继续使用已确认正确的 `kZombieSpawnBaseOffsetY=+2px`；主人实测后的泳池背景公共偏移为 `kPoolBackgroundZombieSpawnYOffset=0px`，第三大关所有行再应用 `kThirdAreaZombieAlignmentOffsetY=+10px`。水路必须与同地图陆地行分开，`GetZombieSpawnY` 对 `IsPoolRow(row)` 额外应用 `kPoolRowZombieSpawnYOffset=+25px`，因此只下移第 3/4 行水中僵尸的 Transform 与画面，不影响陆地行；`GetZombieCollisionY` 不包含这段美术下沉，`Zombie` 基础碰撞框在构造时反向抵消差值并锚定逻辑行，避免同排子弹漏判。不要通过放大子弹或僵尸碰撞箱补偿视觉偏移。四个可调偏移均集中在 `Board.cpp` 顶部；旧名 `kPoolZombieSpawnYOffset` 因容易误解为水路专属而弃用。
- 樱桃炸弹的纵向爆区按 `plantRow-1..plantRow+1` 三个逻辑行桶结算，X 轴仍使用爆心±130px；`CreateBoom` 必须显式接收植物行，禁止再用植物/僵尸 Transform 的 Y 差判断，否则水路额外美术下沉会让相邻行命中依赖像素阈值。
- `anim_eat` 会重新显露双腿并把 `Zombie_duckytube` 换回无水面接触纹理的普通贴图；腿部不再逐轨道隐藏，而由基类 shader 水线对所有僵尸统一裁剪。专用泳池僵尸只在啃食期间把泳圈覆盖为 `IMAGE_REANIM_ZOMBIE_DUCKYTUBE_INWATER` 保留涟漪，停止啃食和读档恢复时对称撤销/重建覆盖。
- 僵尸已锁定睡莲后若同格新种上层植物，必须把啃食目标和 `mEaterCount` 从睡莲迁移到新顶层植物，且不重播啃食动画；`StartEat` 的碰撞 stay 主动迁移，`EatTarget` 在每个伤害帧前再兜底检查一次，避免碰撞更新顺序让一口伤害误落到睡莲。该规则按同格顶层通用实现，不把坚果或睡莲类型写死。
- `CollisionSystem::Update` 会先收集本帧全部碰撞 pair，再在主线程依次派发回调；回调中关闭碰撞体不能取消同批次已经收集的第二个回调。睡莲+上层植物因此会同时命中撑杆：首个回调进入 `JUMPING`，旧实现的第二回调又误进 `StartEat`，随后 exit 回调把轨道切到 `anim_walk`，落地帧事件永远不到；读档又因 `LoadExtraData` 对 `JUMPING` 直接 `EndJump()` 而表现为“重进后已跳过”。`Polevaulter::StartEat` 现以虚函数入口统一限制为仅 `WALKING` 可啃食，lambda 也只在 `WALKING` 转发，不能只依赖关闭碰撞体或单个回调守卫。
- 台风对同格睡莲+上层植物必须整叠搬运/丢失，不允许拆叠。
- `PoolCleaner` 使用 `LAND/ENTERING/IN_POOL/EXITING` 高度状态；只改绘制偏移，不改行与碰撞。水中稳态相对原 28px 下沉位置向上校正 13px，入水/出水阶段按当前浸入比例渐进叠加该校正以避免跳变，原始深度仍照常存档。陆地/水中吞噬分别用 `anim_landsuck/anim_suck`，稳态用 `anim_land/anim_water`，运动速度按 C# 水车:草车 `2.5:3.33` 换算。
- level 19～27 存档写入 `poolGridVersion=2`。缺字段、旧五行/单植物槽或旧版上移 40px 坐标存档均保留文件但拒绝读取，让关卡按当前坐标重新开始。
- 3-1 为 10 波 `{normal, cone}`；3-2 为 15 波 `{normal, cone, bucket}`；3-3 为 15 波
  `{normal, cone, elite polevaulter}`；3-4 为 20 波
  `{normal, cone, polevaulter, bucket, pink football, elite polevaulter}`。水路替换在选行后发生，
  成本/权重仍使用出怪表中的基础类型。精英撑杆详细契约见
  [project_pvz_elite_polevaulter_zombie](project_pvz_elite_polevaulter_zombie.md)。
- 所有泳池背景的自然波次选行在第 1～4 波只允许陆地行，第 5 波起才开放水路。该门槛只属于自然波次选行；AutoTest/开发者显式指定行的造怪仍按静态地形兼容性执行，便于独立验证水路表现。

## 验证

`smoke_pool_basics.json` 覆盖 3-1/3-2 背景、六行、出怪表、睡莲分层/保护/禁种、天气、台风整叠搬运、地形僵尸替换与水中死亡。`smoke_pool_cleaner.json` 覆盖待机、启动、入水、水中、出水和回到陆地，并锁定水中上移 13px 后 `visualY≈309.5`、回到陆地为 `visualY≈294.5`。`smoke_pool_zombie_visuals.json` 在 x=930 断言向右校正后的入水切换，并同时锁定恢复通用背景基线后第三大关水路 Y≈335、陆地行 Y≈140、第一/二大关仍为 Y≈340；水中 `anim_eat` 保持腿部轨道激活但画面由 shader 水线裁掉，额外生成报纸僵尸验证非专用类型也能进入水路，并断言 `poolBlockedZombieTypeCount=0` 与 `graphics.lastFrameScissorChanges=0`。`smoke_pool_zombie_interactions.json` 用陆地化灰作对照，断言水中爆炸 `charredZombieCount=0`，并现场验证补种上层植物后睡莲/坚果 `eaterCount` 从 `1/0` 迁移为 `0/1`。2026-07-24 又增加“陆地 row 1 樱桃命中相邻水路 row 2 僵尸”回归；`clang-playtest` 零警告构建后在主人当前桌面可见运行 exit 0，日志断言目标进入 `anim_waterdeath`，截图确认无烧焦残影。2026-07-23 同场景同种子实测逐僵尸矩形 Clip 为 `21 draw + 4 scissor`，改成 shader 水线后为 `19 draw + 0 scissor`（2 只水中僵尸）；通用化后默认实例路径与 `-NoInstance` 回退路径再次可见运行 exit 0、截图正确。完整通用 Clip 契约见 [project_pvz_shader_clip_rect](project_pvz_shader_clip_rect.md)。

`smoke_pool_effect.json` 固定白天战斗视角截取相隔 45 帧的水面，并补验夜间泳池资源分支、动画计数与 `graphics.lastFrameScissorChanges=0`。2026-07-23 `clang-playtest` 可见运行 exit 0；白天内框 ROI 有 `107434/108724` 像素变化，上方草坪对照 ROI 为 0，证明动画只作用于水面。随后 `smoke_pool_zombie_visuals.json` 可见回归 exit 0，实体层序、水线裁剪与跨关卡坐标断言全部保持通过。

`smoke_pool_visual_fixes.json` 用真实卡片点击与 `move_mouse` 悬停覆盖睡莲上方预览层级，并在 3-1/3-2 各连续推进前四波断言 `earlyWavePoolZombieCount=0`。本次实现已通过 `clang-playtest` 编译；专项脚本首次可见运行在旧卡片边界坐标处未拿起卡片，修正坐标后主人明确取消后续 AutoTest，因此不记录运行通过结论。

`smoke_pool_spawnlists_3_1_to_3_4.json` 逐关锁定 3-1～3-4 的背景、波数和完整出怪数组，
并对 3-3/3-4 选卡预览留图。2026-07-24 `clang-playtest` 可见运行退出码 0；3-4 状态明确包含
`ZOMBIE_PINK_FOOTBALL` 与 `ZOMBIE_ELITE_POLEVAULTER`，日志 `script finished OK`。

`smoke_pool_instanced_shadows.json` 用 8 组睡莲+豌豆射手和 140 只屏外静止陆地僵尸稳定跨过 200
对象并行绘制阈值。旧路径把同一 worker blend 段的 batch 影子整体提前到 instance 睡莲之前，导致
上层植物影子被睡莲反盖；透明预览只因改变 worker 分片而让少量影子暂时恢复。`ShadowComponent`
现于默认路径将影子写入同一 instance 流，`-NoInstance` 保留 batch 兜底；修复后两路径可见运行
均 exit 0，截图显示全部 8 个影子，状态为 16 株植物、140 只僵尸、14 draw、0 scissor。

`smoke_pool_polevaulter_stacked_plant.json` 固定 level 19 水路同格 `LILYPAD` under +
`WALLNUT` normal，先断言双层格成立，再断言撑杆中段保持 `JUMPING/anim_jump/isEating=false`，
最终落地为 `WALKING` 且跳距整数投影 `150000`。2026-07-24 修复前的现有 `clang-release`
可见运行稳定失败于 `JUMPING` 但实际轨道 `anim_walk`；修复后同预设 LTO 编译通过，专项脚本和
既有 `smoke_polevaulter_vault_walk.json` 均在主人当前桌面可见运行 exit 0，日志无
`ERROR/FAIL/WATCHDOG`，跳跃中段、泳池落地和陆地回归截图均正常。

2026-07-24 将水路 `+25px` 美术下沉与逻辑碰撞基线分离，并把三线射手斜向初速按泳池 85px 行高缩放后，`clang-playtest` 重新配置并完整编译通过。主人要求本项不跑 AutoTest、改由实战验收，因此不记录自动化命中结论。
