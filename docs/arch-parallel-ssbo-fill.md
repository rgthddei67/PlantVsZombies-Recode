# 架构笔记：并行填充实例 SSBO（reanim instancing 的每帧数据落地）

> 一句话：CPU 算好的每个 `InstanceRecord` **不攒数组、不上传**，而是直接写进一块持久映射、HOST_COHERENT、每帧独立的 SSBO；多线程并行靠的不是加锁，而是**把这块 SSBO 切成各 worker 互不重叠的私有切片**，每线程无锁写自己那段，最后主线程按顺序缝成 z-order 正确的 `vkCmdDraw`。

相关代码：`Graphics.cpp`（`BeginParallelRecord` / `AppendReanimInstance` / `ReplayAndEndParallel`）、`Game/GameObjectManager.cpp:246-272`（dispatch）、`Reanimation/Animator.cpp:348-419`（`DrawInternalInstanced` 造 `InstanceRecord`）、`Shader/reanim_inst.vert.glsl`（GPU 端按 `gl_InstanceIndex` 读取）。

---

## 1. 终点：数据要进哪块缓冲

实例化顶点着色器按实例下标读 SSBO：

```glsl
// reanim_inst.vert.glsl:42
InstanceRecord r = instances.inst[gl_InstanceIndex];   // set=0 binding=0 storage buffer
```

每个 sprite 的「2×3 仿射 + UV + 纹理槽 + 颜色 + shader 裁剪框」打包成一条 56B 的 `InstanceRecord`。每帧任务 = 把这帧全部 N 条记录填进这块 SSBO。"搬到 SSBO" 指的就是这个填充动作。

`InstanceRecord` 布局（`reanim_inst.vert.glsl:20`，与 C++ 端逐字段对齐）：

```
struct InstanceRecord {           // 56 B
    float tA, tB, tC, tD;         // 2×3 仿射的旋转/缩放部分（w*Scale / h*Scale 已烘进列）
    float tx, ty;                 // 平移
    float u0, v0, u1, v1;         // atlas UV bbox
    uint  texSlot;                // bindless 纹理槽
    uint  colorRGBA8;             // 打包颜色（含 alpha / glow tint）
    uint  clipMinXY;              // 帧缓冲 left/top，各占 16 bit
    uint  clipMaxXY;              // 帧缓冲 right/bottom，各占 16 bit
};
```

---

## 2. 地基：一块 CPU 直接写、GPU 直接读的缓冲

```
fr.instBuf = VulkanBuffer::Create(8MB initial, STORAGE_BUFFER_BIT, hostVisible=true)
             └─ HOST_VISIBLE | HOST_COHERENT | 持久映射(MappedPtr 全程稳定)
```

容量采用 grow-on-demand：某帧需求溢出后，等该帧 fence 安全回收时按真实需求扩容；不再为普通场景常驻旧的 64MB/帧。

| 属性 | 含义 | 带来的好处 |
|---|---|---|
| 持久映射 | `MappedPtr()` 返回稳定 CPU 裸指针 | 无 staging、无 `vkCmdCopyBuffer`、无每帧 map/unmap |
| HOST_VISIBLE | CPU 写的字节 GPU 直接当 SSBO 读 | 写入点**就是**显卡读取点，零拷贝 |
| HOST_COHERENT | 写完不用手动 flush | submit 时隐含 host-write barrier 自动可见 |
| 每 frame-in-flight 一块 | 本帧写自己那块 | GPU 还在读上一帧时不踩踏 |

块内用游标 `fr.instCursor` 做 bump 分配，append 一条往后推一条。

> **关键认知**：这里没有"先在 CPU 攒数组、再上传 GPU"这一步。`slice[i] = rec` 的落点本身就在显卡能读到的内存里——所以 CPU 算矩阵不是瓶颈，写入流量是零拷贝直达 GPU 的。

---

## 3. 核心数据流（一帧的时间线）

```
主线程                          线程池 worker 0..N-1                    主线程
────────────────────────────────────────────────────────────────────────────────
GameObjectManager::DrawAll
   排序对象(renderOrder)
        │
        ▼
 BeginParallelRecord(N) ───────► 把 instBuf 剩余区间切成 N 段互不重叠切片
        │                         每段 → VkWorkerSlice{ instPtr, instBaseIdx, instCap }
        │                         切片大小 = 按上帧各 slot 占用「加权」
        ▼
 ThreadPool::Dispatch ─────────► ┌─ worker 0 ──────────┐ ┌─ worker 1 ──┐  ...
                                 │ SetWorkerSlot(0)     │ │ slot=1      │
                                 │  tl_record → slice0  │ │             │
                                 │ for obj in chunk:    │ │ for obj...  │  (并发，
                                 │   Draw→DrawInternal  │ │             │   各写各的
                                 │     Instanced        │ │             │   切片，
                                 │   造 InstanceRecord  │ │             │   无锁)
                                 │   AppendReanimInstance│ │            │
                                 │   slice0.instPtr[c++]=rec            │
                                 └──────────┬───────────┘ └──────┬──────┘
                                            └──── join ──────────┘
        ▼                                          │
 ReplayAndEndParallel ◄─────────────────────────────┘
   按 slot 顺序 0→N 走每段切片
   每段连续同 blend → vkCmdDraw(6, instCount, 0, instBaseIdx)
   instCursor 一次性推过整个区域
        │
        ▼
 overlay 层(UI, renderOrder≥LAYER_UI) 主线程串行画，恒在最上层
```

---

## 4. 无锁的秘密：切片，而不是加锁

`BeginParallelRecord` 用前缀和偏移 `iOff` 把缓冲切成 N 段连续、**互不重叠**的私有区：

```
instBuf 映射内存:  [baseInst ......................... capacity - reserve][proportional reserve]
                    │                                    │              │
       切片(前缀和) ▼                                    ▼              ▼
       ┌──────────┬─────────────┬──────────┬───────────────┐  ┌──主线程留给──┐
       │ slice 0  │   slice 1   │ slice 2  │   slice N-1   │  │ deferred text│
       │ worker0  │   worker1   │ worker2  │   workerN-1   │  │ / UI overlay │
       └──────────┴─────────────┴──────────┴───────────────┘  └──────────────┘
         instPtr→   instPtr→       ...
         instCap     instCap (大小按上帧 slot 占用加权，busy 槽更大，带地板兜底)
```

每个 worker 写入的核心只有两行（`Graphics.cpp:958-983` worker 分支）：

```cpp
if (sl.instCount >= sl.instCap) { sl.overflowed = true; return; }  // 越界保护：写满即丢，绝不踩邻居
sl.instPtr[sl.instCount++] = rec;                                  // 无锁写入自己的私有切片
```

> **为什么无锁安全**：切片之间一个字节都不共享，并发写不存在数据竞争——不需要 mutex / 原子操作，切片以 stride 对齐且跨度远大于 cache line，也不会假共享。这是经典的 **"用划分消灭同步"(partition to eliminate contention)** 模式。

---

## 5. 写得乱、放得齐：replay 还原 z-order

并发写入在物理时间上是乱序的（谁先写完不定），但渲染要求 z-order 严格。还原靠两层顺序：

```
切片内顺序  ←  instCount++ 单调递增保证
切片间顺序  ←  slot = start/chunkSize 让 slot 编号单调对应渲染顺序
                + replay 按 0→N 顺序回放
            ⇒ 全局 draw 顺序 = 排序后的 renderOrder，精确还原
```

每段切片回放成 `vkCmdDraw(6, instCount, 0, instBaseIdx)`：6 = 顶点数（着色器按 `gl_VertexIndex` 自展开 quad），`instBaseIdx` = 该切片在 SSBO 里的全局起始下标，对齐 GPU 端的 `gl_InstanceIndex`。

---

## 6. 一个易踩的细节：blend 模式分段

同一 worker 的流里 alpha 本体与 additive glow 交错（`Animator.cpp:410/419` 同 sprite 先 alpha 后 Add）。worker 在写实例前记一条 `SetBlendMode` 标记，带上「切换时的实例计数」`instOffsetAtCmd`；replay 据此把一段切片切成多个 `vkCmdDraw`、分别绑 alpha / add pipeline。

worker 还会 **save/restore `tl_blend`**，否则 glow 的 Add 会污染同槽下一个对象的影子绘制——这是注释里记载的真实历史 bug（影子被 Add 混合打成不可见）。

---

## 7. 这对"矩阵搬 compute shader"建议的回应

矩阵留在 CPU 不是瓶颈，因为写入流量已经**全并行 + 零拷贝**地进了 GPU 内存。GPU instancing 真正省掉的是 per-call 的 mat4 构造、6 顶点膨胀、7× 写带宽（`Animator.cpp:368-371` 注释原话），不是三角函数本身。把矩阵搬 compute 反而要新增「源数据上传 + dispatch + compute→vertex 同步屏障」，而 reanim 骨骼解算是树状串行、与游戏逻辑在 CPU 交织——先 profile 再说。
