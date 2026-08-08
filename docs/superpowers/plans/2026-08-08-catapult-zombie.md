# 投篮车僵尸实现计划

**目标：** 实现经典投篮车本体、篮球弹道、车辆碾压与损坏、地刺和两类死亡表现，并留下叶子保护伞拦截 TODO。

**技术路线：** `CatapultZombie` 拥有车辆状态机与库存；已有 `Bullet` 解析抛物线增加植物碰撞模式；`Zombie` 提供通用地刺车辆事件；独立 `CatapultCharred` 播放无剪辑标记的完整时间线。

**设计依据：** `docs/superpowers/specs/2026-08-08-catapult-zombie-design.md`

## 任务 1：身份和资源闭环

- [x] 把 `ZOMBIE_CATAPULT` 移入有效枚举并补动画枚举、资源键、工厂、gamedata、图鉴和开发/AutoTest 名表。
- [x] 登记篮球音效，移植 `CatapultExplosion` 多发射器粒子并强制暂存主人提供的 reanim/PNG。
- [x] 增加运行时本体、灰烬、篮球和损坏材质加载断言。

## 任务 2：篮球弹丸

- [x] `BulletPool` 启用 `BULLET_BASKETBALL`，对象池 reset 完整恢复植物碰撞配置。
- [x] 增加 75 点伤害、篮球纹理/缩放/自旋、解析抛物线、植物层级命中和落空回收。
- [x] 在实际植物扣血前留下 `TODO(叶子保护伞)`，保证未来只需接管该入口。

## 任务 3：投篮车本体

- [x] 实现 850 HP、随机车速、手动位移、碰撞框与同排压扁。
- [x] 实现 `WALKING/SHOOTING/RELOADING/CALTROP_DYING`，第 46 帧投球、六发库存与篮筐显隐。
- [x] 实现两段损坏、烟雾、自损、不可魅惑、火焰耐性和水路过滤。

## 任务 4：地刺、死亡与存档

- [x] 把地刺特殊命中提升为 `Zombie` 虚入口，并回归普通/鎏金冰车语义。
- [x] 实现普通爆炸、`anim_bounce` 延迟爆炸和第 29 帧专属灰烬。
- [x] 保存/恢复状态机、库存、速度、烟雾和地刺死亡瞬态。

## 任务 5：AutoTest、文档与交付

- [x] 增加投篮车/篮球/灰烬/粒子状态投影和专项可见脚本。
- [x] 按主人追加要求把投篮车加入屋顶 5-3、5-4，并增加完整出怪表/预览专项。
- [x] `clang-release` 配置与构建，当前桌面可见运行专项，检查退出码、日志、状态 JSON、断言和截图。
- [x] 更新仓库记忆，审计并完善 `adding-zombie` / `adding-particle`，用 skill-creator `quick_validate.py` 校验。
- [x] 复核强制暂存资源、提交，并按上游与工作树风险决定是否 push。
