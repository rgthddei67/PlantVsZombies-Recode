---
name: project-pvz-pumpkin-shell
description: 经典南瓜头的第三植物层、前后片夹层绘制、软啃声、破损材质、存档、水池与台风组合契约
metadata:
  node_type: memory
  type: project
---

# 经典南瓜头（PumpkinShell）

2026-08-02 完成。`PumpkinShell : Plant` 对齐 C# 原版 125 阳光、30 秒冷却、4000 生命，
可以独立种在陆地，也可以包住 normal 层植物；水路必须已有睡莲。主人目检后把本体
`gamedata` Y offset 从 -44 调到 -34，使豌豆头部清楚露出。

## 实现契约

- `Cell` 新增独立 `pumpkin` 槽，战斗顶层顺序为 `pumpkin > normal > under`。创建、带 ID 读档创建、
  释放、全格清理、render order、僵尸啃食和台风都消费该层级；外壳死亡后内层植物保留。
- 2026-08-04 起铲子不再复用战斗顶层：点击南瓜中空中心选择 `normal`，点击下半部可见外圈
  选择 `pumpkin`，没有普通内层的空壳仍整格选南瓜。外圈环心、内外半径与上边界均按当前
  Cell 宽高比例派生。本次按主人要求不新增或运行 AutoTest，只完成构建与静态路径核对。
- 根 Animator 隐藏 `Pumpkin_back`，独立同步 Animator 隐藏 `Pumpkin_front`。普通植物绘制前插入
  后片，南瓜实体随后绘制前片；空壳/预览/压扁由自身补后片，避免 Draw 中改共享轨道状态而破坏
  默认并行实例化。默认与 `-NoInstance` 截图必须一致。
- 生命低于最大值 `2/3` 与 `1/3` 时，前片依次换为 `pumpkin_damage1`、`Pumpkin_damage3`。
  阶段完全由通用生命存档派生，`LoadExtraData` 只恢复终态，不重播反馈。
- C# `Zombie::AnimateChewSound` 把 Wallnut、Tallnut、Garlic、Pumpkinshell 归入 `ChompSoft`。
  C++ 通用啃食路径同样按这四类选择 `SOUND_CHOMPSOFT`，普通植物仍随机 Chomp/Chomp2。
- 植物血量文字位置由 `Plant::GetHealthTextOffset` 提供品种覆写点；南瓜头在通用位置基础上
  向下偏移 20px，使同格普通植物与外壳的两行血量不再重叠。
- 图鉴文案明确说明精英小丑与粉色橄榄球使用 `×6`、爆破工头使用 `×5`，并注明普通小丑
  仍会直接摧毁整组植物，避免玩家把所有爆炸误认为都受外壳保护。
- `Board::ApplyPumpkinProtectedZombieAreaDamage` 是特殊僵尸范围扣血的统一入口：先沿各技能
  原几何收集命中植物，再按逻辑格归并；格内有活动南瓜层时只让外壳承受一次基础伤害
  `×6`，精英矿工爆破通过显式重载使用独立 `×5`；没有外壳时维持 under/normal 各自
  承伤。当前接入精英小丑投盒、精英矿工爆破和粉色橄榄球掉盔；普通小丑的直接 `Die()`
  爆炸明确不接入。精英小丑贪心与蒙特卡洛
  快照也保存南瓜层并使用同一承伤集合。
- 台风把 `under/normal/pumpkin` 当一个占格组合搬运或一起出界/坠入弹坑；移动统计仍按实体 ID，
  三层水池组合移动一次得到 `lastGustMovedPlants=3`。

## 验证证据

- `clang-release` 配置、构建和音效增量构建均成功；vcpkg applocal 仍打印找不到
  dumpbin/objdump 的既存诊断，但可执行文件正常链接。
- `smoke_pumpkin.json` 在主人当前桌面可见运行，默认与 `-NoInstance` 均 exit 0；覆盖资源、
  125/30/4000 数值、正反种植顺序、顶层啃食、`ChompSoft` 且普通啃声为 0、两档破损、外壳死亡、
  陆地/水池快照往返和三层台风搬运。6 张最终同步截图已逐张检查。
- 既有 `smoke_typhoon.json` 可见 exit 0。`smoke_pool_basics.json` 的本次相关双层种植与阵风段
  全部通过，之后在无关的旧 `maxWave=15` 预期处失败，当前源码实际为 20。
- `smoke_pumpkin_zombie_area_damage` 的当前期望精确锁定精英小丑 `50×6`、精英矿工
  `150×5`、水路三层粉色橄榄球 `50×6`，内层生命不变；同时锁定无壳精英矿工仍扣
  150、普通小丑仍直接清空整组。此前 ×5 版本与精英小丑完整/蒙特卡洛、精英矿工、
  粉色橄榄球和普通小丑存量脚本同批可见回归均 exit 0；当前 ×6/×5 调整按主人要求
  仅做静态一致性检查，未重新构建或运行 AutoTest。
- 2026-08-02 血量文字下移 20px 后，`clang-release` 构建成功；`smoke_pumpkin` 在主人当前
  桌面可见运行 exit 0，`01_pumpkin_stacking_orders.png` 已确认普通植物与外壳血量上下分行，
  豌豆头部未被数字遮挡。其余受伤、水池、存档与台风段同批通过。
- 2026-08-04 分层铲除改动完成 `clang-release` 配置、编译和链接，exit 0；仅出现既存的
  vcpkg applocal 找不到 dumpbin/objdump 提示。按主人要求未新增或运行 AutoTest；静态核对确认
  中心点落在内半径内并选择 `normal`，下半部外圈点落在环带内并选择 `pumpkin`。
