# 魅惑僵尸（Mind-Controlled Zombie）设计文档

日期：2026-07-02
状态：已获主人批准
范围：僵尸侧魅惑机制全套（触发入口、豁免、互啃、移动/边界、视觉、存档、AutoTest）。**魅惑菇（HypnoShroom）本体不在本次范围**，但触发接口本次留好。

## 背景与现状

- `Zombie::mIsMindControlled` 已部分存在：被魅惑后反向（向右）走（`Zombie.cpp` ZombieMove）、豌豆射手选靶/土豆雷/大嘴花无视它（`Board.cpp:626` / `PotatoMine.cpp:22` / `Chomper.cpp:16`）、存档字段 `isMindControlled` 已带。
- 缺失：触发入口、不可魅惑豁免、啃僵尸、右边界处理、视觉表现。
- 碰撞侧 2026-07-01 seeker/target 拆分时已为本功能预留钩子（见 memory `project_pvz_charmed_zombie_collision_hook`）：按 `layerMask==ZOMBIE` 拆 `mRowZombies`（被动目标）/`mRowOthers`（seeker），seeker 二分搜排序僵尸数组。**CollisionSystem 本次零改动。**
- 原版参考：`D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn\Zombie\Zombie.cs` — `StartMindControlled()`(6170)、`FindZombieTarget()`(6998，判定 `mMindControlled != 对方` 且同行，**对称=双向互啃**)、`EffectedByDamage`(2005，普通伤害对魅惑免疫)、渲染镜像+粉红 tint+additive 高光(1721)。

## 主人已定的玩法决策

1. **自家子弹不打魅惑僵尸**（照原版）。
2. **双向互啃**：普通僵尸也会反啃魅惑僵尸（照原版）。
3. **走出右边界即移除**（无奖励，照原版）。
4. **视觉 = 红光变色 + 简单翻身**（水平镜像）；碰撞箱/影子不随翻转调整，瑕疵已豁免。

## 设计

### 1. 触发入口与豁免（虚函数方案，主人提议）

```cpp
// Zombie.h
public:
    void StartMindControlled();                       // 唯一魅惑入口（非虚）
    virtual bool CanBeCharmed() const { return true; } // 子类豁免点
```

- `StartMindControlled()` 首行守卫：`if (mIsMindControlled || mIsDying || !CanBeCharmed()) return;`
- 通过后：置 `mIsMindControlled = true`；若正在啃植物先走 StopEat 清理（`mEaterCount` 平衡）；播魅惑音效；应用红光 + 翻身；改碰撞掩码（见 §2）。
- **Polevaulter override**：`bool CanBeCharmed() const override { return mVaultState == VaultState::WALKING; }` — RUNNING/JUMPING 态魅惑无效（对应原版"撑杆跳过魅惑菇根本不吃它"的实际效果）。
- 将来其他不可魅惑僵尸（气球、雪橇车等）各自一行 override。

### 2. 碰撞接线（照 memory 配方）

- `ColliderComponent.h` 的 `CollisionLayer` 加 `CHARMED = 1 << 5`。
- 魅惑瞬间改自身碰撞体：
  - `layerMask = CHARMED` → phase-2 分桶自动落进 `mRowOthers`，成为廉价 seeker（魅惑数量少）。
  - `collisionMask = ZOMBIE` — 不含 PLANT（不误啃植物）、不含 BULLET（子弹判不中）。
- 普通僵尸 `collisionMask` 加 `CHARMED` 位（`Zombie.cpp:46`），使 pair 双向掩码检查通过；普通僵尸 `layerMask` 仍是 ZOMBIE，不变 seeker。
- 子弹掩码不动（只搜 ZOMBIE 层）→ 自家子弹自动打不到魅惑僵尸。已知副作用（接受）：小推车也压不到魅惑僵尸。
- 天然性质：魅惑×魅惑同为 CHARMED 层、掩码互不含 CHARMED → 不互啃；普通×普通照旧不扫。

**实现修订**：`CanCollide` 是双向 OR（`CollisionSystem.h:58`，`(a.layerMask & b.collisionMask) || (b.layerMask & a.collisionMask)`），魅惑侧 `collisionMask` 含 ZOMBIE 单边即可与普通僵尸成对；普通僵尸 `collisionMask` **无需**再加 CHARMED 位，上文第 3 条按此简化实现。

### 3. 双向互啃

- `Zombie` 新增 `int mEatZombieID = NULL_ZOMBIE_ID;`（与 `mEatPlantID` 并列；同一时刻只啃一个目标）。
- `StartEat(other)` 加 `OBJECT_ZOMBIE` 分支：同行、`IsMindControlled() != 对方->IsMindControlled()`、对方未死才啃；播 `anim_eat`；记 `mEatZombieID`。
- `StopEat(other)` 对应清理并回走路动画。
- `EatTarget()`（帧事件回调）加僵尸目标分支：对目标 `TakeDamage(mAttackDamage)` — 走正常护盾→头盔→本体伤害链；**不过** `ScaleZombieDamage` 词条缩放（那是僵尸对植物的词条）。
- 目标死亡：被啃方 `Die()` 已有碰撞体禁用 → 触发 onTriggerExit → StopEat；另在 `EatTarget` 里目标查不到时自行清理兜底。
- **pair 次序 foot-gun（已知，必须遵守）**：碰撞产 pair 时僵尸（ZOMBIE 层）恒放 a、seeker（魅惑）放 b，`HandleCollisionEnter` 先 a 后 b。两侧回调各自 StartEat 对方，先后次序对本逻辑无害——设计上依赖"两侧独立、互不抢状态"。
- `ValidateEatingState`（读档校验）同步照顾 `mEatZombieID`：目标不存在则清理回走路。
- 啃食对象互斥：已在啃植物则不啃僵尸，反之亦然（`mEatPlantID`/`mEatZombieID` 双空才开新啃）。

### 4. 移动与右边界

- 反向走已有（`ZombieMove` 按 `mIsMindControlled` 取速度正负）。
- 新增：`Zombie::Update` 中 `mIsMindControlled && GetPosition().x > 950`（出屏阈值，实现时按棋盘常量微调）→ `Die()`。无奖励；`mZombieNumber--` 与 `CheckWin` 走正常链（魅惑僵尸被移除算"消灭"，可正常触发关卡胜利）。

**实现修订**：右边界移除复用既有出屏清理（`Zombie.cpp:258` 的 `x > SCENE_WIDTH+65 → Die()`，普通僵尸反向出左边界同款逻辑对魅惑僵尸正向出右边界天然生效），未新增专门代码。

### 5. 视觉：红光 + 翻身

- **红光**：复用现成 overlay 管线（`EnableOverlayEffect` + `SetOverlayColor`，减速蓝光同款）+ 可选 additive 微光。色调照原版粉红（约 255, 100, 100 档，实现时目测微调）。
- **已知冲突**：减速蓝光共用同一 overlay。处理：魅惑后子弹已打不中（不会再新增减速），仅需处理"减速中被魅惑"与"魅惑时残留减速倒计时归零"两个时点的覆盖优先级——魅惑红光优先、且减速结束的关灯逻辑不得关掉魅惑红光。
- **翻身**：`Animator::SetFlipX(bool)` 新接口。实现＝在 **两条绘制路径**（`DrawInternalInstanced` 快路径 + `DrawInternal` mat4 慢路径）的矩阵构造处应用绕支点水平镜像：`tA→−tA, tC→−tC, tx→2·pivot−tx`（pivot 取僵尸身体中线的动画局部 x，实现时目测定值）。
- 注意：现有 `SetLocalScale`/`mLocalScaleX` 是**写而不读的死字段**，不要用它做翻转。
- 碰撞箱、影子不动（主人已豁免瑕疵）。
- 血量文字 `Draw` 不受影响（独立于 Animator 矩阵）。

### 6. 存档

- `isMindControlled` 字段已存在（`SaveProtectedData`/`LoadProtectedData`）。
- 读档后需按标志**重建派生状态**：碰撞掩码、红光、翻身（放 `LoadProtectedData` 之后的统一恢复点或 `ZombieItemUpdate`）。
- 互啃进行时状态（`mEatZombieID`）**不持久化**：读档后僵尸回走路，碰撞下一帧自动重建互啃。
- 旧存档兼容：字段已有默认值，零迁移。

### 7. HypnoShroom 触发接口（本次只留口，不实现）

- 将来在 `Zombie::EatTarget()` 植物分支加：`plant->mPlantType == PlantType::HYPNOSHROOM` → `plant->Die()` + `this->StartMindControlled()`（照原版 AnimateChewSound 9380：一口触发、蘑菇即死；夜间/唤醒态判定到时随 Shroom 睡眠机制走）。
- 本次交付的 `StartMindControlled()` 即该接口的全部依赖。

### 8. 验证（AutoTest）

- TestDriver 新增 op `charm_zombie`（直调 `StartMindControlled`，按行/序号选目标僵尸）。
- 脚本覆盖：
  1. 刷 2–3 只普通僵尸 → 魅惑其一 → 截图验红光+翻身+反向走；
  2. dump_state 验双向互啃掉血、被啃方死亡后魅惑者恢复走路；
  3. 魅惑者一路向右 → 验出右边界被移除（zombie 数归零）；
  4. 撑杆 RUNNING 态 `charm_zombie` → 验无效（仍向左、无红光）；跳跃落地 WALKING 后再魅惑 → 生效；
  5. 种豌豆射手 → 验子弹穿过魅惑僵尸打后方普通僵尸、豌豆不选魅惑为靶。

### 范围伤害

- 大喷菇锥形伤害（`FumeShroom::FumeAttack`）**豁免**魅惑僵尸：原版 `DoRowAreaDamage(20, 2U)` 的 `damageRangeFlags=2U` 不含 bit7，不炸魅惑目标；触发检测 `HasZombieInRow` 同样跳过魅惑（全行只剩魅惑时不空放喷射动画，与 Chomper/PotatoMine 索敌跳过同一惯例）。
- 樱桃炸弹（`Board::CreateBoom`）**保留**能炸魅惑：原版爆炸 `damageRangeFlags=0xFF` 含 bit7，Jack 爆炸/樱桃对魅惑僵尸一视同仁，本次不改。
- 魅惑音效：因无资产暂缺，随将来 HypnoShroom 魅惑菇本体一并补齐。

## 备选方案（已否决）

- **每帧 FindZombieTarget 扫描**（照抄 C#）：每只僵尸每帧 O(n) 扫全表，与本仓库"取全集再过滤是 foot-gun"经验相悖；碰撞钩子已备好，否决。
- **SetLocalScale(-1) 翻转**：死字段，绘制不读，不可用。
- **翻转做进 TransformComponent/变换栈**：影响碰撞箱与影子，改动面大；主人已豁免瑕疵，绘制层镜像即可。
