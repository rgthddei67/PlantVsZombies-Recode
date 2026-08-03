---
name: project-pvz-flowerpot
description: "2026-08-03 经典花盆、屋顶承载门禁、初始5/4/3列、可见阴影与台风整组搬移"
metadata:
  node_type: memory
  type: project
---

# 经典花盆与屋顶承载层

## 当前契约

- `PLANT_FLOWERPOT` 为 25 阳光、7.5 秒冷却、300 生命，使用 `Pot.reanim` 的 `anim_idle`；种下后 1 秒只阻止普通僵尸啃食，剩余时间入档，旧档缺字段默认 0。
- 花盆占 Cell `under` 层。屋顶普通植物与南瓜分别要求同格花盆后进入 `normal` / `pumpkin`，地刺系始终拒绝屋顶；水路睡莲规则未改。
- 上层植物仅在视觉锚点与落点预览上抬 5px，逻辑格和碰撞箱不变。花盆被覆盖时暂停 idle，露出后恢复。
- 新局在选卡前铺设初始花盆：九关制内部 37/38/39～45 对应 5/4/3 列；保持 C# 外层列、内层行的创建顺序，读取关卡存档时不重复生成。
- 台风按 Cell 把 `under + normal + pumpkin` 作为一个组合逐格搬移或丢失；换格、二维屋顶追赶、存读档后层级与覆盖状态保持一致。

## 资源与阴影

- 主人提供的 `Pot.reanim` 已删除 shadow 轨；运行时引用的八张 `Pot_*` 组件图注册闭环完整。
- 不使用 `Pot_shadow.png`，改用现有 `ShadowComponent` 与 `IMAGE_PLANTSHADOW`。C# 花盆实际阴影落点为 X=`-3-1=-4px`、Y=`51-5=46px`、1:1 比例；状态断言固定 X=-4/Y=46，截图确认盆底阴影肉眼可见。

## 验证证据（2026-08-03）

- `clang-release` 编译、链接成功。
- 可见 `smoke_flowerpot_starting_layout.json` exit 0，断言 5/4/3 列边界与列优先创建顺序。
- 可见 `smoke_flowerpot.json` 在默认实例路径和 `-NoInstance` 均 exit 0，覆盖数值、门禁、阴影、短暂无啃食、分层、覆盖暂停、5px 抬升、快照与双向台风；截图逐张确认。
- 可见 `smoke_roof_terrain_consumers.json`、`smoke_roof_zombie_foundation.json` 均 exit 0。`smoke_pool_basics.json` 的睡莲/承载/台风相关段落通过，末尾仅命中与本改动无关的 3-9 `maxWave` 旧期望 15、当前配置 20。
