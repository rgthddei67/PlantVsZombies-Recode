# 双向射手设计

## 目标

实现经典双向射手（`PLANT_SPLITPEA`），保持原版 125 阳光、7.5 秒卡牌冷却、
1.5 秒攻击周期与 300 生命。前头向前单发，后头向后双发；两个方向独立索敌，
同一轮可同时开火。

## 动画与发射

`SplitPea.reanim` 基础帧率为 12fps，根 Animator 播放 `anim_idle`。两个独立子
Animator 都附着到根 `anim_idle` 轨道，并用其首帧 `(37.6, 48.7)` 的逆基准姿态
抵消附件矩阵：

- 前头：`anim_head_idle → anim_shooting → anim_head_idle`，按 C# 的 45fps 播放；
  主人确认真实发射帧为 95。
- 后头：`anim_splitpea_idle → anim_splitpea_shooting → anim_splitpea_idle`，
  按 C# 的 35fps 播放；主人确认真实发射帧为 57。
- 两个帧号已经是 `AddFrameEvent` 口径，不再减一。前头轨为 88～112，后头轨为
  50～62，两个持久帧事件不会扫入对方待机或射击轨。

每颗豌豆沿用普通射手的 Throw/Throw2 随机音效。前向弹保持 `+290px/s`；后向弹
设为 `-290px/s`，因此继续复用既有碰撞、台风方向倍率、火炬树桩转换和对象池存档。
前枪口沿用项目普通射手的稳定视觉偏移；后枪口按原版前后枪口相差 88px 换算。

## 索敌与节奏

- 每 1.5 秒检查一次本行，使用 `ForEachZombieInRow`，不扫描全表。
- 前头只认植物逻辑 X 及其右侧目标；后头只认植物攻击矩形左界以内的目标。
- 过滤魅惑、无头、死亡/不可被地面弹丸命中的僵尸，并继续经过
  `Board::CanPlantAcquireZombie` 的雾中可见性入口。
- 后头第一颗出膛后立即排队重播一次射击动画，第二次第 57 帧再发一颗；整轮开始后
  即使第一颗击杀目标，也会把已经启动的双发补完。
- 攻击计时和两个射击动画统一乘现有生存攻速与雨势行动倍率。

## 存档

`Shooter` 继续保存攻击计时和前头完整播放状态。双向射手额外保存：

- 后头完整 Animator 状态，包括一次性返回轨、返回速度和返回混合；
- 后头两发之间的 `pendingSecondShot` 与 `isSecondShot` 瞬态。

关卡快照必须覆盖第二颗后向豌豆尚未出膛的中途状态，读档后只能续完当前一轮，
不能把后头射击轨恢复为循环。

## 注册、数据与图鉴

- 复用已经预留的 `PlantType::PLANT_SPLITPEA` 和 AutoTest 名称表。
- 新增 `AnimationType::ANIM_SPLITPEA`、`IMAGE_SPLITPEA`、`REANIM_SPLITPEA`，
  并在 `GameDataManager` 注册 `SplitPea` 工厂。
- `gamedata.json` 填写 125/7.5、截图后校准的 offset/scale，以及
  `baseHealth:300, attackDps:13.33`；轻量防线推演只计算稳定的前向输出，
  不把条件式后向伤害虚增到常驻 DPS。
- `info.txt` 增加双向射手名称和经典能力说明。

## 验证

`smoke_splitpea.json` 覆盖：

1. 卡图、数值、simulation 画像、待机轨、双头站位和影子截图；
2. 仅前方目标时只生成一颗正向豌豆；
3. 仅后方目标时生成两颗负向豌豆；
4. 两侧目标同时存在时同轮产生一前两后三颗豌豆及三次 Throw 请求；
5. 后头第二发动画中保存/重载关卡快照，随后准确续发并回到待机；
6. 通关内部关卡 31 后，冒险进度解锁 `PLANT_SPLITPEA`。
