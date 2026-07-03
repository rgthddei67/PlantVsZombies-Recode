# 粒子系统图层统一实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让粒子特效可按 RenderOrder 层级绘制——默认战场层（僵尸之上、GameMessageBox/Button 之下），显式高层可盖住全部 UI。

**Architecture:** ParticleSystem 保持独立单例，只统一"绘制排序坐标系"：ParticleEffect 携带 renderOrder；世界层粒子经 GameObjectManager::DrawAll 的 pre-overlay hook 插在主体与 overlay 之间；顶层粒子仍走场景命令槽（排序后位于 UI 命令之后）。

**Tech Stack:** C++17 / CMake preset `clang-release`（构建验证）/ AutoTest JSON 脚本 + 截图验证。

**Spec:** `docs/superpowers/specs/2026-07-03-particle-render-layer-design.md`

## Global Constraints

- 所有 `.h` 必须以 `#pragma once` 开头（pre-commit hook 强制）。
- 源码 `/utf-8` 编码，注释可用中文。
- 不得改动并行录制路径（BeginParallelRecord/Replay）内部；hook 只能在主线程串行段调用。
- 现有 19 个 `EmitEffect` 调用点不改（默认参数即战场层）。
- 构建工作目录/命令见 CLAUDE.md「Build & Run」；AutoTest 运行目录为 `build\clang-release`。
- 提交信息结尾带 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。

**路径注意**：仓库根是 `D:\PVZ\PlantsVsZombies\PlantVsZombies`，源码在其下的 `PlantVsZombies\` 子目录（下文 Modify 路径均相对仓库根）。

---

### Task 1: RenderOrder 新层 + ParticleEffect/ParticleSystem 分层接口

**Files:**
- Modify: `PlantVsZombies/Game/RenderOrder.h`
- Modify: `PlantVsZombies/ParticleSystem/ParticleEffect.h`
- Modify: `PlantVsZombies/ParticleSystem/ParticleSystem.h`
- Modify: `PlantVsZombies/ParticleSystem/ParticleSystem.cpp`

**Interfaces:**
- Produces: `LAYER_EFFECTS_WORLD = 35000`；`ParticleEffect::SetRenderOrder(int)/GetRenderOrder()`；`ParticleSystem::EmitEffect(const std::string&, const Vector&, int renderOrder = LAYER_EFFECTS_WORLD)`；`ParticleSystem::DrawBelow(int order)`；`ParticleSystem::DrawFrom(int order)`。`DrawAll()` 删除（调用点仅 Scene.cpp 一处，Task 3 同步改）。

- [ ] **Step 1: RenderOrder.h 加层**

在 `LAYER_GAME_BULLET = 30000,` 与 `// UI层` 之间插入：

```cpp
	// 战场特效层（子弹之上、UI 之下；粒子默认层）
	LAYER_EFFECTS_WORLD = 35000,
```

并把 `LAYER_EFFECTS = 80000` 上方注释改为 `// 顶层特效层（全部 UI 之上）`。

- [ ] **Step 2: ParticleEffect 加 renderOrder**

`ParticleEffect.h`：private 区加 `int renderOrder;`（构造函数已有实现文件，在 `ParticleEffect.cpp` 构造函数初始化列表补 `renderOrder(0)` 或在类内 `int renderOrder = 0;`——**采用类内初始化**，不动 .cpp）。public 区加：

```cpp
	void SetRenderOrder(int order) { renderOrder = order; }
	int GetRenderOrder() const { return renderOrder; }
```

- [ ] **Step 3: ParticleSystem 接口改造**

`ParticleSystem.h`：头部加 `#include "../Game/RenderOrder.h"`；把 `void DrawAll();` 替换为：

```cpp
	// 绘制 renderOrder < order 的特效（战场层；由 GameObjectManager overlay 前 hook 调用）
	void DrawBelow(int order);
	// 绘制 renderOrder >= order 的特效（顶层；由场景命令槽调用）
	void DrawFrom(int order);
```

`EmitEffect` 签名改为：

```cpp
	void EmitEffect(const std::string& effectName, const Vector& position,
		int renderOrder = LAYER_EFFECTS_WORLD);
```

`ParticleSystem.cpp`：`DrawAll` 实现替换为：

```cpp
void ParticleSystem::DrawBelow(int order) {
	for (auto& effect : effects) {
		if (effect->GetRenderOrder() < order) {
			effect->Draw();
		}
	}
}

void ParticleSystem::DrawFrom(int order) {
	for (auto& effect : effects) {
		if (effect->GetRenderOrder() >= order) {
			effect->Draw();
		}
	}
}
```

`EmitEffect` 实现签名同步加参，`effects.push_back` 前加：

```cpp
	effect->SetRenderOrder(renderOrder);
```

- [ ] **Step 4: 编译验证（预期失败一处）**

按 CLAUDE.md 导入 VS 环境后 `cmake --build --preset clang-release`。
预期：`Scene.cpp` 报错 `no member named 'DrawAll'` —— 这是 Task 3 的接线点，属预期红灯；除此之外无其他错误（若有其他 DrawAll 调用点，一并记录并在 Task 3 处理）。

- [ ] **Step 5: Commit（与 Task 3 绿灯后一起提交，此处先不 commit）**

（本项目无单元测试，红灯=编译断点确认唯一调用点；为保持仓库可编译，Task 1+2+3 完成后统一提交。）

---

### Task 2: GameObjectManager pre-overlay hook

**Files:**
- Modify: `PlantVsZombies/Game/GameObjectManager.h`
- Modify: `PlantVsZombies/Game/GameObjectManager.cpp:230-280`（DrawAll 串行 fallback 与 overlay 段）

**Interfaces:**
- Produces: `GameObjectManager::SetPreOverlayHook(std::function<void()>)` —— DrawAll 中主体绘制完、overlay（renderOrder ≥ LAYER_UI 的对象）绘制前，在主线程调用。
- Consumes: 无（hook 内容由 Task 3 注入）。

- [ ] **Step 1: 头文件加成员与 setter**

`GameObjectManager.h`：`#include <functional>`（顶部 include 区）；private 区（`mSortDirty` 附近）加：

```cpp
	// 主体（< LAYER_UI）绘制完、overlay 绘制前的注入点（主线程串行调用）。
	// 用于世界层粒子等"压主体、不压 UI"的外部绘制，避免依赖具体子系统头文件。
	std::function<void()> mPreOverlayHook;
```

public 区加：

```cpp
	void SetPreOverlayHook(std::function<void()> hook) { mPreOverlayHook = std::move(hook); }
```

- [ ] **Step 2: DrawAll 两条路径插入 hook**

`GameObjectManager.cpp` DrawAll：

串行 fallback（现约 230-235 行）改为分段：

```cpp
	if (!g->IsParallelDrawEnabled() || parallelCount < kParallelDrawThreshold) {
		// 串行 fallback（菜单/图鉴等少对象场景）：主体 → 世界层粒子 → overlay
		PROFILE_SCOPE("6.Draw_submit(serial-fallback)");
		drawSerialRange(0, splitIdx);
		if (mPreOverlayHook) mPreOverlayHook();
		drawSerialRange(splitIdx, total);
		return;
	}
```

并行路径：`ReplayAndEndParallel()` 的作用域块之后、`8.Draw_overlay(serial)` 块之前插入：

```cpp
	// 世界层粒子（< LAYER_UI）画在主体 replay 之后、overlay 之前（主线程串行，保序）
	if (mPreOverlayHook) mPreOverlayHook();
```

- [ ] **Step 3: 编译验证**

`cmake --build --preset clang-release`。预期：仍只剩 Scene.cpp 的 DrawAll 报错（Task 3 消除）。

---

### Task 3: 接线（GameApp 注入 hook + Scene 顶层槽 + 命令排序）

**Files:**
- Modify: `PlantVsZombies/GameApp.cpp:322-326`（g_particleSystem 创建处）
- Modify: `PlantVsZombies/Game/Scene.cpp:23-30`（ParticleSystem 命令）、`Scene::Draw` lazy build 处（约 54-58 行）

**Interfaces:**
- Consumes: Task 1 的 `DrawBelow/DrawFrom`、Task 2 的 `SetPreOverlayHook`。

- [ ] **Step 1: GameApp 注入 hook**

`GameApp.cpp` 在 `g_particleSystem->LoadXMLConfigs(...)` 的 if 块之后加（需 `#include "Game/GameObjectManager.h"`，若已包含则不重复）：

```cpp
	// 世界层粒子注入 GameObjectManager：主体之上、UI overlay 之下
	GameObjectManager::GetInstance().SetPreOverlayHook([] {
		if (g_particleSystem) {
			g_particleSystem->DrawBelow(LAYER_UI);
		}
	});
```

- [ ] **Step 2: Scene 顶层槽改 DrawFrom**

`Scene.cpp` "ParticleSystem" 命令 lambda 内 `g_particleSystem->DrawAll();` 改为：

```cpp
				g_particleSystem->DrawFrom(LAYER_UI);
```

（命令名与 LAYER_EFFECTS 槽位不变。）

- [ ] **Step 3: 基类 lazy build 后排序**

`Scene::Draw`：

```cpp
	if (!mDrawCommandsBuilt) {
		BuildDrawCommands();
		SortDrawCommands();
		mDrawCommandsBuilt = true;
	}
```

（派生场景后续 RegisterDrawCommand + 自行 SortDrawCommands 的既有流程不受影响；`DrawCommand` 已有 `operator<`。）

- [ ] **Step 4: 编译验证（绿灯）**

`cmake --build --preset clang-release`。预期：0 error 0 warning（clang-release 是 warning 权威配置）。

- [ ] **Step 5: Commit**

```powershell
git add PlantVsZombies/Game/RenderOrder.h PlantVsZombies/ParticleSystem/ParticleEffect.h PlantVsZombies/ParticleSystem/ParticleSystem.h PlantVsZombies/ParticleSystem/ParticleSystem.cpp PlantVsZombies/Game/GameObjectManager.h PlantVsZombies/Game/GameObjectManager.cpp PlantVsZombies/GameApp.cpp PlantVsZombies/Game/Scene.cpp
git commit -m @'
feat(particle): 粒子按 RenderOrder 分层绘制，统一进图层坐标系

ParticleEffect 携带 renderOrder(默认 LAYER_EFFECTS_WORLD=35000)；
世界层经 GameObjectManager pre-overlay hook 画在主体与 UI overlay 之间，
顶层(>=LAYER_UI)走场景 LAYER_EFFECTS 槽盖住全部 UI；
Scene 基类命令列表 lazy build 后真正排序。

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 4: AutoTest 验证脚本 + 截图回归

**Files:**
- Create: `autotest/scripts/smoke_particle_layers.json`（仓库根下 autotest/scripts/）

**Interfaces:**
- Consumes: 全部前置任务；AutoTest 命令集（CLAUDE.md）。

- [ ] **Step 1: 写脚本**

场景：种樱桃豆爆炸（世界层粒子）同时开暂停/消息框，截图两张：爆炸瞬间、以及僵尸掉头盔粒子仍在僵尸上方的常规回归。

```json
{
  "commands": [
    { "op": "goto_level", "level": 1 },
    { "op": "choose_cards", "cards": ["PLANT_CHERRYBOMB"] },
    { "op": "wait_state", "value": "GAME" },
    { "op": "set_sun", "value": 500 },
    { "op": "spawn_zombie", "type": "ZOMBIE_NORMAL", "row": 2, "x": 600 },
    { "op": "wait_seconds", "value": 1 },
    { "op": "plant", "type": "PLANT_CHERRYBOMB", "row": 2, "column": 5 },
    { "op": "wait_seconds", "value": 1.0 },
    { "op": "key", "name": "escape" },
    { "op": "wait_frames", "value": 5 },
    { "op": "screenshot", "name": "explosion_under_menu" },
    { "op": "dump_state", "name": "state_menu" },
    { "op": "key", "name": "escape" },
    { "op": "wait_seconds", "value": 2 },
    { "op": "screenshot", "name": "after_explosion" },
    { "op": "quit" }
  ]
}
```

注意：樱桃爆炸粒子持续时间与 ESC 菜单弹出时机需现场调 `wait_seconds`；若 ESC 菜单不是 GameMessageBox 路径，改用能弹 GameMessageBox 的操作（以实际代码为准，执行者先 grep `GameMessageBox` 的触发点）。

- [ ] **Step 2: 运行**

```powershell
Push-Location build\clang-release
.\PlantsVsZombies.exe -AutoTest ..\..\autotest\scripts\smoke_particle_layers.json -Seed 42
$LASTEXITCODE   # 预期 0
Pop-Location
```

- [ ] **Step 3: Read 截图验证**

Read `build\clang-release\autotest\out\smoke_particle_layers\explosion_under_menu`（无 .png 扩展名，直接文件名）：
- 预期：爆炸云/烟雾被消息框遮挡（粒子在框下）。
Read `after_explosion`：预期焦僵尸/爆炸残留正常显示在场地上（世界层粒子仍盖住僵尸主体）。

- [ ] **Step 4: 既有 smoke 回归**

跑 1-2 个现有粒子相关脚本（如 `demo_peashooter.json`，含子弹命中粒子）确认 exit 0 且截图正常。

- [ ] **Step 5: Commit**

```powershell
git add autotest/scripts/smoke_particle_layers.json
git commit -m @'
test(autotest): 粒子分层冒烟脚本（爆炸在消息框下）

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```
