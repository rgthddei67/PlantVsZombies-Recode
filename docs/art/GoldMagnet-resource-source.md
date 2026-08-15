# GoldMagnet 资源来源

- `build/clang-release/resources/reanim/GoldMagnet.reanim` 取自本地经典资源副本 `D:\PVZ\中文年度加强版完整版\Test\reanim\GoldMagnet.reanim`，导入时 SHA-256 为 `6CE8B9F127113EC4F2C26BAF464847E41E4356B56BD87AFD734EB7E498021CF9`。
- 时间线引用的 `GoldMagnet_*.png` 与卡图 `GoldMagnet.png` 在本仓库权威运行资源中原已存在，本次未重绘或改色。
- 战斗版只复用原版 `anim_idle` 与 `anim_attract` 包装轨；充能和白天睡眠分别复用 `anim_idle`，睡眠身份由通用 `Z.reanim` 指示。玩法触发不依赖新增动画帧事件。
- 实机屋顶花盆合成最终使用原生 `scale=1.0` 与 `offset=[-34,-44]`；专项在真实初始花盆上锁定水平中心和最终包围盒，避免无承载层截图掩盖站位误差。
- `GoldMagnetEMP.xml` 是本项目新增的轻量表现配方，复用已注册的 `PARTICLE_RAIN_CIRCLE` 与 `PARTICLE_STAR40`，不新增位图。
