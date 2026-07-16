---
name: adding-particle
description: Use when adding or tuning ANY particle effect (粒子特效) in PvZ — 新建/修改 resources/particles/config/*.xml、染色变种、爆发云/掉落物/命中飞溅配方。全部标签语义已从 ParticleXMLLoader/ParticleEmitter 源码实证，写粒子不用再读源码。
---

# 给 PvZ 写粒子特效（XML 配置全参考）

本文件每个标签的语义都是 2026-07-15 从 `ParticleSystem/` 源码逐行实证的（IceFumeCloud 蓝色孢子云实战），
2026-07-16 随毁灭菇 Doom.xml 移植更新（ImageFrames 序列帧实装 + 原版 XML 移植口径）。
**改了引擎消费端（ParticleEmitter/ParticleXMLLoader）要回来同步本文档。**

## 心智模型

- **一个 XML 文件 = 一个特效**，可含多个 `<Emitter>`（同时全部点燃，如 PeaBulletHit=飞溅+碎屑两发射器）。
- **特效名 = 第一个 `<Emitter>` 的 `<Name>`，不是文件名**（文件名只是惯例上取一致）。
- 目录：`build/<preset>/resources/particles/config/`，启动时全目录加载——**双 preset 都要放**；纯数据改配置**不用重编译，但要重启游戏**。
- 触发：`g_particleSystem->EmitEffect("Name", GetPosition());`（第三参 renderOrder 可选）。名字打错启动不报错，**发射时** run.log 出 `ERROR 找不到粒子特效配置`。
- 贴图：`<Image>` 填资源键（`IMAGE_*`/`PARTICLE_*`，即 resources.xml 里那些）；**没有独立粒子贴图格式**，任何已加载纹理都能当粒子。
- **键前缀由 resources.xml 段落决定**：`<GameImages>` 里的 → `IMAGE_*`，`<ParticleTextures>` 里的 → `PARTICLE_*`（粒子专用图放后者）。写错前缀=粒子静默不生成（foot-gun ③）。
- **分份贴图**：`<Texture Column="4" Row="1">` 会把图切成独立纹理 `PARTICLE_XXX_PART_0..3`（`基础键_PART_序号`，行优先）——逗号列出来即"每粒子随机一张"（splats 碎屑的原理）。**序列帧动画别用它**，用 `ImageFrames`（整图不切，见标签表）。

## 数值语法（三种，核心）

1. **标量**：`1.5`。
2. **随机范围 ValueRange**：`[60 100]`（空格分隔，方括号）→ **生成/初始化时随机一次**，之后不变。裸写 `16 32` 也会被读成范围 [16,32]。
3. **插值轨迹 InterpolationTrack**：关键帧序列 `值[,时间%] 值[,时间%] ...`，按粒子寿命归一化线性插值：
   - `1,80 0` = 前 80% 寿命恒 1（端点外**夹住**不外推），80%→100% 线性降到 0（淡出经典写法）。
   - `1 0` = 无显式时间 → 均匀铺 0..100%（=从 1 线性到 0）。
   - `0 [20 300],60 [25 310]` = 关键帧本身可以是区间；**Position 场里每颗粒子抽一个随机因子走自己的"轨道"**（云横向铺开的原理）；其他消费方取区间**中点**（确定性）。
   - `EaseOut` 等缓动词会被**跳过降级为线性**（解析器只认线性）。
   - 单独一个 `[a b]`（无时间）= 每粒子生成时随机一次、终生保持（ParticleScale 常用）。

## 标签参考（默认值 = 不写时的实际行为）

| 标签 | 类型 | 默认 | 实证语义 |
|---|---|---|---|
| `Name` | 字符串 | 必填 | EmitEffect 用的特效名（取第一个 Emitter 的） |
| `SpawnMinActive` | 范围 | 1 | **初始化瞬间爆发**的粒子数；**不计入 MaxLaunched 配额** |
| `SpawnMaxLaunched` | 范围 | 1 | `SpawnRate` 涓流的总配额（爆发型用不到，照抄 ≥MinActive 即可） |
| `SpawnRate` | int/秒 | 0 | 持续每秒生成 N 颗；0=只有初始爆发 |
| `ParticleDuration` | 范围(秒) | 1.0 | 单粒子寿命；所有插值轨迹按它归一化 |
| `SystemDuration` | float(秒) | -1 | 到时停止发射（已有粒子自然消亡）+ SystemAlpha 的归一化基准。**见 foot-gun ①** |
| `ParticleAlpha` | 轨迹 | 1 | 透明度 0..1 |
| `ParticleScale` | 轨迹 | 1 | 尺寸倍率（乘贴图原始大小） |
| `ParticleStretch` | 轨迹 | 1 | 仅纵向(高度)拉伸倍率 |
| `ParticleRed/Green/Blue` | 轨迹 | 1 | **乘法染色 0..1**（染色变种不用做新贴图！IceFumeCloud=R.35 G.35 B1） |
| `ParticleBrightness` | 范围 | 1 | RGB 整体乘数（生成时随机一次） |
| `SystemAlpha` | 轨迹 | 1 | 整团透明度，按 `systemTimer/SystemDuration` 采样（没写 SystemDuration 则恒取 1） |
| `EmitterType` | Point/Box/Circle | Point | 出生点形状；写了 `EmitterRadius` 未写 Type 自动当 Circle |
| `EmitterBoxX/Y`、`EmitterRadius` | 范围 | 0 | Box 半边长 / Circle 半径（逐粒子随机） |
| `EmitterOffsetX/Y` | float | 0 | 出生点相对 EmitEffect 坐标的偏移。**见 foot-gun ②：实际生效两倍** |
| `LaunchSpeed` | 范围(px/s) | 0 | 初速度大小 |
| `RandomLaunchSpin` | bool | false | true=初速度方向 360° 随机；**false=一律沿 +X（向右）** |
| `ParticleSpinSpeed` | 范围(**度**/s) | 0 | 贴图自旋（DrawTexture 走 glm::radians，配置里是度：碎屑 ±130、掉头 ±5） |
| `ParticleGravity` | float(px/s²) | **0** | 每帧 `vy += g*dt`；头文件成员默认写 100 是幌子，loader 无标签时 as_float(0) 覆盖 |
| `Image` | 资源键 | 无 | **必填**：没有它粒子直接不生成（静默）；逗号分隔多个=每粒子随机选一张（碎片直接复用 splats 系列） |
| `ImageFrames` | int | 1 | **序列帧动画**（2026-07-16 实装）：贴图为**横排帧条**，帧宽=图宽/N（毁灭菇爆炸底座 471×85=3 帧翻滚蘑菇云）。绘制取当前帧列，>1 才生效 |
| `AnimationRate` | float(帧/s) | 12 | 序列帧推进速度，到尾**循环**；仅在 ImageFrames>1 时有意义 |

### Field 场（每个 `<Field>` 一个 `<FieldType>` + 可选 `<X>`/`<Y>` 轨迹）

| FieldType | 实证语义 |
|---|---|
| `Position` | **绝对偏移**（非累加）：`fieldOffset = 轨迹值`，叠加在物理位置上绘制。区间关键帧→逐粒子随机轨道（铺开的云/喷雾主力） |
| `Shake` | 每帧在 ±X/±Y 内均匀随机抖动（绘制偏移，不动物理位置） |
| `Friction` | 每帧 `v *= (1-x)`——**帧率相关**的衰减，0.1 就已经很强 |
| `Acceleration` | `v += x*dt`，帧率无关的恒加速（比 Gravity 多了 X 分量） |

### 解析了但引擎不消费（写了无效，勿浪费时间调）

`<SystemField>`、`FieldType=SystemPosition`（移动整个发射器——移植原版 XML 时把它的常量偏移**折算进 EmitterOffset 并减半**，见移植口径）、
`<FullScreen>`（全屏绘制——等效替代：`ParticleScale 4000` 的 WhitePixel 巨quad，Doom 紫闪实证）。
（`ImageFrames`/`AnimationRate` 已于 2026-07-16 实装，移入上方标签表。）

## 生命周期与渲染层

- 回收条件：发射停止（SystemDuration 到时 或 涓流配额打满）**且**存活粒子归零 → 特效对象自动销毁。
- `EmitEffect` 第三参默认 `LAYER_EFFECTS_WORLD`(35000)=世界层（植物/僵尸之上、UI 之下，GameAPP `DrawBelow(LAYER_UI)`）；传 `>= LAYER_UI` 的值则画在 UI 之上（Scene `DrawFrom(LAYER_UI)`）。
- 粒子更新吃 DeltaTime：暂停/倍速/timescale 自动正确。

## Foot-guns（血泪汇总）

1. **爆发型特效必须写 `SystemDuration`**：`SpawnRate=0` 时发射器的"配额打满"判定永远不成立（初始爆发不计入 particlesEmitted），没有 SystemDuration 的特效对象**永不回收**、每帧空转。所有现存配置都带它（≈ 最长粒子寿命 + 一点余量）。
2. **`EmitterOffsetX/Y` 生效两次**：ParticleEffect 定位发射器时加一次，每次 spawn 取出生点又加一次——FumeCloud 写 25 实际前移 50px。调偏移按"写入值 × 2"心算，或干脆改 EmitEffect 传入坐标。
3. **没写 `<Image>` 或键打错 = 粒子静默不生成**（这是刻意设计，供"纯计时"发射器用），特效"发了却看不见"先查这里；特效名打错才有 run.log ERROR。
4. 染色走 `ParticleRed/Green/Blue`（0..1 乘法），等价心算：目标 overlay 色 (80,80,255)/255 ≈ (.31,.31,1)。**别去做染色贴图**。
5. `RandomLaunchSpin` 不写时初速度**恒向右**——掉落物（头/手臂）必须写 `1`，否则一律向右飞。
6. Position 场是**绝对偏移**：想让粒子"随时间飘远"，轨迹要从小值渐变到大值（`0 [20 300],60 ...`），写常量它就钉在那不动。
7. 双 preset 都要放 XML；改完**重启**游戏才生效（启动时一次性加载）。
8. **负数随机区间写升序** `[-300 -200]`：原版 XML 里的 `[-200 -300]` 直接照抄会把 min/max 反着喂给 GameRandom::Range，行为未定义。
9. **移植原版 XML 前先 Compare 两 preset 的 resources.xml**：msvc-debug 的历史上整体陈旧过（缺 12 个文件条目），只 append 新行会漏掉旧账——发现漂移直接整文件覆盖 + 补拷缺失资源。

## 配方（照抄改数）

**一次性爆发云**（FumeCloud/IceFumeCloud）：`SpawnMinActive [16 32]` + `ParticleAlpha .9,80 0` + Position 场区间轨迹铺开 + `Shake 1` + `SystemDuration 1.25`。染色版只加三行 RGB。

**掉落物**（ZombieHeadOff）：`SpawnMinActive 1` + `LaunchSpeed [60 100]` + `RandomLaunchSpin 1` + `ParticleGravity 140` + `ParticleSpinSpeed [-5 5]` + Position 场常量（出生点修正 -10,-50）。

**命中飞溅**（PeaBulletHit，双发射器）：主溅斑（1颗、`ParticleScale 1.2 0.4` 缩小消失）+ 碎屑环（`EmitterType Circle` + `LaunchSpeed [65]` + `Friction 0.0,10 0.1` 先快后刹 + `Acceleration Y=5` 微下坠）。

**原版 XML 移植口径**（Doom.xml→10 发射器大特效实证，逐条机械换算）：
1. 时间字段全部**厘秒→秒（÷100）**：ParticleDuration 150→1.5；SystemDuration 别照抄 400→4（原版仅回收判定），取"最长粒子寿命+余量"即可（→1.6）。
2. **EmitterOffsetX/Y 减半**（本引擎双倍生效，foot-gun ②）：原版 -75 → 写 -37.5。
3. **SystemField/SystemPosition 折算进 EmitterOffset**（本引擎不消费）：原版 SysPos(-90,-120) → EmitterOffset(-45,-60)（折算后同样减半）。
4. `FullScreen` 闪光 → `ParticleScale 4000` + WhitePixel（1×1 白图，键 PARTICLE_WHITEPIXEL），RGB/Alpha 轨迹原样保留。
5. `AnimationRate`/`ImageFrames` **原值照抄**（单位本就是帧/秒）；对应贴图整图入库**不加 Column 属性**（切开就不是帧条了）。
6. 负数区间改升序（foot-gun ⑧）；`Image` 键按素材入库段落改前缀（IMAGE_/PARTICLE_）。
7. **特效名=第一个 Emitter 的 Name**：把首发射器 Name 改成 EmitEffect 要用的名字（Doom.xml 首发射器 DoomStem→"Doom"）。

## 验证

粒子寿命都是亚秒级，AutoTest 截图要卡时机：帧事件/命中发生后 `wait_frames` 5~20 再 `screenshot`（多截几张挑）。逐张 Read 核对：颜色、铺开范围、方向、有没有"看不见"（foot-gun ③）。改数值免编译，跑脚本前重启即可。

## 关联

触发端惯例见 adding-plant / adding-zombie skill（帧事件里 EmitEffect）；渲染分层背景见记忆 [[project_pvz_particle_render_layer]]。
