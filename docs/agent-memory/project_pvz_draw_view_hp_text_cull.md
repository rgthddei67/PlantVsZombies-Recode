---
name: project-pvz-draw-view-hp-text-cull
description: 2026-05-31 会话三项优化(删除O(n+k)/Draw组件视图/HP文字视口剔除) + DataArray 已排除 + 文本绘制爆降真因与glyph-atlas方向
metadata:
  node_type: memory
  type: project
  originSessionId: 32ac1cd8-8c32-4677-a0dc-a1aed4c24425
---

2026-05-31 同一会话三项优化 + 一项架构评估。均经用户 VS 编译、"没问题"确认；**尚无干净 before/after FPS 数字**(HP-on 帧数被混淆、Draw 视图未单独计时)——下次拿到数补进来。

**① GameObjectManager 删除 O(n·k)→O(n+k)**：`GameObjectManager.cpp:82` 旧实现逐个 `std::remove` 全表扫描。改为 `unordered_set<GameObject*>` + 单趟 `remove_if`。守住三不变量:renderOrder 有序(用 stable `remove_if` 而非 swap-pop，`:162`排序+`:207`二分依赖)、realloc 安全(保留下标循环+拷贝 shared_ptr，因 DestroyAllComponents 回调可能追加待删项)、组件销毁顺序。值钱场景=群体死亡爆发删除。

**② GameObject::Draw 组件视图**(镜像 phase-3 的 `mUpdatableComponents`)：旧 `GameObject::Draw` 每对象每帧 `new vector` + 遍历 unordered_map + `stable_sort`(`GameObject.cpp`)。加 `mDrawableComponents` + `mDrawableSortDirty`，AddComponent/RemoveComponent 同步、懒排序。**关键:`Component::SetDrawOrder` 移到 GameObject.cpp 定义并回调 `MarkDrawableSortDirty`**——旧码每帧重排隐含支持运行时改 draw order，必须保留该契约(非 YAGNI)。多线程 draw worker 只读相机/各画各对象，无竞争。植物/僵尸经 AnimatedObject::Draw→GameObject::Draw 真正受益。

**③ HP 文字视口剔除**：`Graphics::IsWorldPointVisible(x,y,margin=128)`(`Graphics.cpp` SetWindowSize 后)用真实 `m_projection*m_viewMatrix` 判定→相机 zoom/pan 自动跟随。`Zombie::Draw`/`Plant::Draw` 在 DrawText 前 `if(!g->IsWorldPointVisible(GetPosition()))return`。修了 11000 僵尸 batch VBO(128MB)溢出。

**架构评估:DataArray/PlantSet(宝开 union-buffer) 已排除**。群友建议。方案本身聪明(max-size type-erase 让继承子类也能连续存)，但 by-value 内联存储 ⊥ 本仓库 shared_ptr 所有权 + 组件系统；换 = 重写对象模型，且组件仍指针化使 cache 收益打折。component 实为 `unique_ptr` 非 shared_ptr(无原子开销)，"原子顺序"担忧是误解。

**文本绘制爆降真因(重要)**：`Graphics::DrawText`→`GetOrCreateTextTexture`(`Graphics.cpp:1065`)是"**整串→一张 GPU 纹理**"，按 `text|font|size|color` 进 **256 条 LRU**(`TEXT_CACHE_MAX_SIZE`,`Graphics.h:787`，驱逐销毁纹理)。血量每掉血变新字符串→11000 僵尸上万互异串 >> 256 → **100% cache thrashing**：每帧上万次 `TTF_RenderUTF8_Blended` CPU 光栅化 + 纹理 create/destroy = 爆降真凶(非 quad 数)。**视口剔除一石二鸟**:可见串几十<256→命中→光栅化抖动同时消失。worker 线程不能建纹理(`:1188` 未命中返空)→新串首帧由主线程预热。
**优化文本方向**:(A 治本)glyph atlas 逐字形 quad——每字形光栅化一次复用，变化数字成本≈0，惠及全部动态文本；(B 快旋钮)调大 TEXT_CACHE_MAX_SIZE；(C)HP 改血条(1-2 quad 无光栅化)。**剔除后正常对局文本已不贵——动 glyph atlas 前先量。**

延续 [project-pvz-perf-optimization](project_pvz_perf_optimization.md) [project-pvz-phase3-component-update-skipping](project_pvz_phase3_component_update_skipping.md) [project-pvz-gpu-instancing-reanim](project_pvz_gpu_instancing_reanim.md) [reference-pvz-batch-instance-zorder](reference_pvz_batch_instance_zorder.md)；协作风格见 [feedback-collaboration-style](feedback_collaboration_style.md)。
