# 粒子系统图层统一设计（方案 A：按层可控）

日期：2026-07-03
状态：待实现

## 问题

`ParticleSystem` 是独立于 `GameObjectManager` 的全局单例，整个系统固定占场景绘制命令列表的一个槽位（`Scene::BuildDrawCommands` 中的 "ParticleSystem"，LAYER_EFFECTS=80000）。后果：

- 所有粒子作为整体，永远画在 GameObjects 命令之后 → 盖住 GameMessageBox（它是 LAYER_UI 的 GameObject，在 GameObjects 命令内部的 overlay 分支绘制）。
- 排序后粒子槽(80000)在 UI 槽(40000)之后，理论上应盖住 Button —— 实际观察 Button 在上（基类 BuildDrawCommands 未排序时按插入顺序 UI 在后；部分场景调 SortDrawCommands 后行为反转）。层级行为依赖"是否碰巧调了排序"，不可靠。
- `ParticleEffect` 没有任何层级字段，无法表达"这个特效属于战场/属于 UI 之上"。

## 目标

发射粒子时可指定其归属层；默认（战场特效）画在植物/僵尸/子弹之上、GameMessageBox 与 Button 之下；显式指定高层的特效可画在全部 UI 之上。不改变粒子的更新逻辑、所有权模型和并行绘制管线。

## 非目标

- 粒子与单个 GameObject 逐对象交错排序（PVZ 特效无此需求）。
- Button/UIManager 的 GameObject 化。

## 关键约束

GameMessageBox 在 "GameObjects" 命令**内部**绘制（GameObjectManager::DrawAll 用 `splitIdx` 把 renderOrder ≥ LAYER_UI 的对象分到 overlay 串行段，画在主体之后）。因此"战场粒子在 MessageBox 之下"无法靠场景命令槽顺序实现，必须把世界层粒子的绘制插入 DrawAll 内部的主体与 overlay 之间——正好复用现成的 `splitIdx` 分割点。

## 设计

### 1. RenderOrder.h

新增 `LAYER_EFFECTS_WORLD = 35000`（LAYER_GAME_BULLET 之上、LAYER_UI 之下），语义：战场特效层。`LAYER_EFFECTS = 80000` 保留，语义变为"全 UI 之上的顶层特效"。

### 2. ParticleEffect / ParticleSystem

- `ParticleEffect` 增加 `int mRenderOrder`（含 getter；由 InitializeFromConfig 之后 setter 或构造参数传入）。
- `EmitEffect(const std::string& name, const Vector& pos, int renderOrder = LAYER_EFFECTS_WORLD)` —— 现有 19 个调用点全是战场特效（爆炸、掉头盔、子弹命中），默认值正确，零改动。
- `DrawAll()` 拆为：
  - `DrawBelow(int order)` —— 绘制 `mRenderOrder < order` 的 effects；
  - `DrawFrom(int order)` —— 绘制 `mRenderOrder >= order` 的 effects。
  同层内保持现有插入顺序（vector 顺序遍历，天然稳定）。

### 3. 世界层粒子的绘制注入

`GameObjectManager` 增加 `std::function<void()> mPreOverlayHook`（setter：`SetPreOverlayHook`）。`DrawAll` 在主体绘制（并行 replay 或串行 `[0, splitIdx)`）完成后、overlay 段 `[splitIdx, total)` 串行绘制前调用该 hook。

GameApp 初始化（创建 g_particleSystem 处）注入：
```cpp
GameObjectManager::GetInstance().SetPreOverlayHook(
    []{ if (g_particleSystem) g_particleSystem->DrawBelow(LAYER_UI); });
```
用 hook 而非直接调用，避免 GameObjectManager 依赖 ParticleSystem 头文件。

注意：hook 在主线程串行段调用（replay 之后），不进入并行录制路径，无线程安全问题；批次发射顺序 = 主体 → 世界粒子 → overlay，天然正确。

### 4. 顶层粒子槽

`Scene::BuildDrawCommands` 中现有 "ParticleSystem" 命令改为调用 `g_particleSystem->DrawFrom(LAYER_UI)`，槽位保持 LAYER_EFFECTS。

### 5. 命令排序修正（顺手修隐患）

`Scene::Draw` 的 lazy build 后（或 `BuildDrawCommands` 末尾）调用 `SortDrawCommands()`，使基类四命令也按声明的 renderOrder 排序，消除"靠插入顺序碰巧成立"。排序后基类顺序：GameTextures(-10000) → GameObjects(0) → UI(40000) → ParticleSystem(80000)。顶层粒子从此稳定盖住 Button；世界粒子经 hook 稳定处于 MessageBox/Button 之下。

需回归确认各派生场景（GameScene/MainMenu/Almanac 等已自行调 SortDrawCommands）行为不变。

## 错误处理

- `g_particleSystem` 为空：hook 与场景命令均已判空。
- 传入非法 renderOrder：不校验，按数值分段（< LAYER_UI 即世界层）。

## 测试

AutoTest 脚本验证（build\<preset> 下运行，截图 Read 验证）：
1. 樱桃炸弹爆炸帧 + 同帧弹出 GameMessageBox（如暂停菜单）：截图确认爆炸云在对话框之下。
2. 显式高层特效（临时测试调用 EmitEffect(..., LAYER_EFFECTS)）：截图确认在 Button 之上。
3. 现有 smoke 脚本回归：掉头盔/子弹命中粒子仍显示在僵尸之上。

## 涉及文件

- `PlantVsZombies/Game/RenderOrder.h`
- `PlantVsZombies/ParticleSystem/ParticleSystem.{h,cpp}`、`ParticleEffect.h`
- `PlantVsZombies/Game/GameObjectManager.{h,cpp}`（hook + splitIdx 处调用）
- `PlantVsZombies/Game/Scene.cpp`（顶层槽改 DrawFrom + 排序）
- `PlantVsZombies/GameApp.cpp`（注入 hook）
