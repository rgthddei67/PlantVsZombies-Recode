---
name: project-pvz-plant-die-visibility
description: 植物立即死亡、GameObject 延迟销毁与 Animator 停止重置之间的可见生命周期契约
metadata:
  node_type: memory
  type: project
---

# 植物立即死亡的可见生命周期

## 2026-08-04 当前契约

- `GameObjectManager::DestroyGameObject` 只登记待移除对象，到下一次 `GameObjectManager::Update` 开头才释放组件并移出对象表。
- `AnimatedObject::StopAnimation` 会调用 `Animator::Stop`，把当前帧重置到轨道起点。植物若只停止动画并登记销毁，本次逻辑更新后的绘制仍会提交一次起始姿态；咖啡豆碎裂结束和倭瓜落地自毁因此会出现一帧闪回。
- `Plant::Die` 现在先用 `IsActive()` 防重入并立即 `SetActive(false)`，再保留原有停止动画、禁用碰撞、释放格位和延迟销毁顺序。`GameObjectManager::DrawAll` 与其他玩法查询会在当帧排除该植物，内存与组件仍由下一次更新安全释放。
- 压扁植物需要保留残影，继续使用独立的 `Plant::Squish` 生命周期，不受立即死亡契约影响。

## 验证状态

- `clang-release` 配置、编译与 LTO 链接退出 0。
- 按主人要求未运行 AutoTest；咖啡豆、倭瓜的实际画面待主人亲自验收。
