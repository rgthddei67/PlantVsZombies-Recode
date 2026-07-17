---
name: project-pvz-particle-render-layer
description: "2026-07-03 粒子按RenderOrder分层绘制完成(58ff654+9a68d7f, master未push)：世界层经GameObjectManager pre-overlay hook、顶层走场景槽"
metadata:
  node_type: memory
  type: project
  originSessionId: cdf315a1-cd1b-4713-b865-64cb24919264
---

2026-07-03 完成粒子图层统一（方案A"按层可控"，spec/plan 在 docs/superpowers/ 下 2026-07-03-particle-render-layer*）。

- `ParticleEffect` 携带 `renderOrder`；`EmitEffect(name,pos,renderOrder=LAYER_EFFECTS_WORLD=35000)`（19 个旧调用点零改动）。`DrawAll` 已删，拆为 `DrawBelow(order)`/`DrawFrom(order)`。
- **世界层粒子接入点不是场景命令槽**：GameMessageBox 在 "GameObjects" 命令内部的 overlay 段（splitIdx≥LAYER_UI）绘制，所以世界粒子必须画在 GameObjectManager::DrawAll 的主体与 overlay 之间——用 `SetPreOverlayHook(std::function)`（GameApp.cpp 注入 `DrawBelow(LAYER_UI)`），串行 fallback 与并行 replay 两条路径都插了。
- 顶层粒子（显式传 ≥LAYER_UI）走原 "ParticleSystem" 场景槽 `DrawFrom(LAYER_UI)`，排序后在 UIManager Button 之后 → 盖住全部 UI。
- 顺手修：`Scene::Draw` lazy build 后调用 `SortDrawCommands()`（基类四命令此前靠插入顺序碰巧对）。

**Why:** 三套图层机制（场景命令顺序/GameObject renderOrder/批次发射顺序）粒度错位是分层 bug 根源；统一的是"坐标系"（RenderOrder.h 语言）而非所有权。

**How to apply:** 新特效想压 UI → `EmitEffect(..., LAYER_EFFECTS)`；想在主体内精确层级做不到（粒子只按层分段，不逐对象交错）。验证脚本 `autotest/scripts/smoke_particle_layers.json`（爆炸压僵尸+被 ESC 菜单遮挡；wait_state 字段是 "state"、plant 用 "col"）。Button 仍在 UIManager 恒顶层，未 GameObject 化。关联 [reference_pvz_batch_instance_zorder](reference_pvz_batch_instance_zorder.md)。
