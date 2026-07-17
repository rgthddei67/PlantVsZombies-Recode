---
name: project_pvz_factory_registry
description: PvZ
metadata:
  node_type: memory
  type: project
  originSessionId: 46e3568e-2999-484f-9d6d-47b9b5702afe
---

2026-05-31 同会话完成 backlog #1（见 [project_pvz_improvement_backlog](project_pvz_improvement_backlog.md)）。构建通过 + 实跑全正常（9 植物 / 5 僵尸 / 预览 / 读档 / 无"未注册"日志；**土豆雷 0.8 缩放视觉正常 = 唯一行为相关迁移点已验**）。

**做了什么**
- `GameDataManager.h`：`PlantInfo`/`ZombieInfo` 各加 `float scale` + 函数指针 `factory`；加 `class Board/Plant/Zombie;` 前向声明 + `PlantFactoryFn`/`ZombieFactoryFn` typedef；新增 `CreatePlant`/`CreateZombie` 公有派发声明；两个 `Register*` 扩 `scale,factory` 参。
- `GameDataManager.cpp`：新增全部子类头 + `GameObjectManager.h`/`Board.h`/`RenderOrder.h` include；匿名命名空间模板助手 `MakePlant<T>`/`MakeZombie<T>`（内写死 `LAYER_GAME_PLANT`/`LAYER_GAME_ZOMBIE`）；`InitializeHardcodedData` 14 条登记各补 `scale,&Make...<类>`（PotatoMine=0.8f 其余 1.0f）；实现 `CreatePlant`/`CreateZombie`（查表→`i.factory(...,i.animType,i.scale,...)`，未注册返回 nullptr+日志）。
- `GameApp.cpp`：两个 `Instantiate*` 各收成一行委托 `GameDataManager::GetInstance().Create*`；删 13 个具体子类 include。

**关键设计决策（复用价值）**
- **函数指针而非 std::function**：工厂零捕获（类型 T 在注册点编译期定死），`&MakePlant<T>` 指向 TU-local 模板实例、代码段常驻，零堆分配。
- **集中注册（并入 GameDataManager）而非自注册**：9 植物中 4 个纯头文件（PeaShooter/SunFlower/SnowPeaShooter/Repeater），无 .cpp 安放静态初始化器 → 否决自注册；集中登记对头文件植物一视同仁。
- **include 取舍**：`Board.h` 对 Plant/Zombie 只**前向声明**，GameApp.cpp 删子类 include 后必须**保留基类 Zombie.h + 显式补 Plant.h**，否则 `Instantiate*` 返回类型 `shared_ptr<Plant>/<Zombie>` 不完整。函数指针 typedef 指向不完整类型合法，重型 include 全留 .cpp，无环。

**新增植物/僵尸的权威指引**写在 `GameDataManager.cpp::InitializeHardcodedData` 顶部注释（4 步）；`AnimationTypes.h:5` 与 `ResourceKeys.h:218` 旧 TODO 已改为指向该处。新流程触点：enum → 类 → 一行 Register（含数据+工厂）+ 顶部 include → Card。

未提交（主人 VS 端驱动）。spec：`docs/superpowers/specs/2026-05-31-plant-zombie-factory-registry-design.md`；plan：`docs/superpowers/plans/2026-05-31-plant-zombie-factory-registry.md`。
