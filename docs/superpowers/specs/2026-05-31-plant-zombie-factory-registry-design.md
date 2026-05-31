# 设计：植物/僵尸注册式工厂（并入 GameDataManager）

日期：2026-05-31
状态：已批准（主人 2026-05-31 确认设计）

## 1. 背景与问题

新增一个植物/僵尸目前要"霰弹式"改动多处文件，代码里留了 4 个 `// TODO 新增X改这里` 路标。核心痛点是 `GameApp.cpp` 的两个工厂：

- `GameApp.cpp:559` `InstantiatePlant` —— 大 switch（每个 `PlantType` 一个 case）
- `GameApp.cpp:605` `InstantiateZombie` —— 大 switch（每个 `ZombieType` 一个 case）

这两个 switch 的唯一职责是把**运行期枚举**翻译回**编译期类型** `T`，以喂给模板 `GameObjectManager::CreateGameObjectImmediateAsShared<T>`。每个 case 还重复写死 `LAYER`、`AnimationType`、`scale`——而 `AnimationType` 已存在 `GameDataManager` 的 `PlantInfo`/`ZombieInfo` 里，属于重复数据源。

每加一个类型都要**修改**已有 switch，违反开放-封闭原则。

## 2. 目标 / 非目标

**目标**
- 消除 `InstantiatePlant` / `InstantiateZombie` 两个 switch，改为数据驱动的注册表查找。
- 把每个 case 残留的 `scale`、`AnimationType` 收进 `GameDataManager`，使工厂纯数据驱动。
- 加类型时不再修改任何 switch；登记集中在一处（`GameDataManager::InitializeHardcodedData`）。

**非目标（本次不做）**
- 不改 `GetBackgroundID`（关卡→地图映射）—— 那是独立关注点，已记入 backlog。
- 不把植物/僵尸数值外置 JSON —— backlog #2，独立任务。
- 不引入日志系统 —— backlog #4；本次错误路径沿用现有 `std::cout`。
- 不改 Card 体系、不改 `Board` 调用点。

## 3. 关键约束（已核实）

- **构造签名统一**：所有植物子类经 `using Shooter::Shooter;`/`using Plant::Plant;` 继承同一构造：
  `Plant(Board*, PlantType, int row, int column, AnimationType, float scale=1.0f, bool isPreview=false)`。
  僵尸同理：`Zombie(Board*, ZombieType, float x, float y, int row, AnimationType, float scale=1.0f, bool isPreview=false)`。
- **9 个植物里 4 个是纯头文件**（PeaShooter / SunFlower / SnowPeaShooter / Repeater）。这否决了"自注册（.cpp 静态初始化器）"方案——纯头文件无处安放静态初始化器。因此采用**集中注册**：所有登记落在 `GameDataManager.cpp` 的 `InitializeHardcodedData`，该 .cpp 直接 include 全部类型头文件，对头文件植物一视同仁。
- **当前 scale 分布**：仅 PotatoMine = 0.8f，其余植物与全部僵尸 = 1.0f。这是本设计**唯一的行为相关数据迁移点**，需视觉验证不变。
- `LAYER` 按类别固定：植物恒为 `LAYER_GAME_PLANT`，僵尸恒为 `LAYER_GAME_ZOMBIE`（在 `MakePlant`/`MakeZombie` 助手里写死，不入数据）。

## 4. 设计

### 4.1 数据结构（GameDataManager.h，轻量改动，不引入重型 include）

头文件顶部新增前向声明与函数指针类型别名：

```cpp
class Board;
class Plant;
class Zombie;

using PlantFactoryFn  = std::shared_ptr<Plant>(*)(Board*, PlantType, int, int, AnimationType, float, bool);
using ZombieFactoryFn = std::shared_ptr<Zombie>(*)(Board*, ZombieType, float, float, int, AnimationType, float, bool);
```

函数指针指向不完整类型合法，重型 include 留在 .cpp，无 include 环。

`PlantInfo` 增两个字段（带默认值，保持现有聚合构造可用）：
```cpp
float         scale   = 1.0f;
PlantFactoryFn factory = nullptr;
```
`ZombieInfo` 对称增 `float scale = 1.0f;` 与 `ZombieFactoryFn factory = nullptr;`。

> 字段加在结构体末尾并带默认值；现有的多参构造函数不接收这两项（由 Register 函数在构造后赋值），避免改动所有 `RegisterXxx` 调用的构造路径。

### 4.2 工厂助手与登记（GameDataManager.cpp）

`GameDataManager.cpp` **新增 include**（即从 `GameApp.cpp` 搬来的同一批）：全部植物头文件、全部僵尸头文件、`GameObjectManager.h`、`Board.h`、`RenderOrder.h`（取 `LAYER_*`）。

匿名命名空间放模板助手：
```cpp
namespace {
    template<typename T>
    std::shared_ptr<Plant> MakePlant(Board* b, PlantType t, int row, int col,
                                     AnimationType anim, float scale, bool preview) {
        return GameObjectManager::GetInstance()
            .CreateGameObjectImmediateAsShared<T>(LAYER_GAME_PLANT, b, t, row, col, anim, scale, preview);
    }
    template<typename T>
    std::shared_ptr<Zombie> MakeZombie(Board* b, ZombieType t, float x, float y, int row,
                                       AnimationType anim, float scale, bool preview) {
        return GameObjectManager::GetInstance()
            .CreateGameObjectImmediateAsShared<T>(LAYER_GAME_ZOMBIE, b, t, x, y, row, anim, scale, preview);
    }
}
```
`MakePlant<T>` 的每个实例化都是同一签名，可直接赋给 `PlantFactoryFn`，零捕获、零堆分配。

`RegisterPlant`/`RegisterZombie` 签名各增 `float scale` 与 factory 指针参数。在 `InitializeHardcodedData` 中，每条登记补 `scale` 与 `&MakePlant<具体类>`：

```cpp
RegisterPlant(PlantType::PLANT_PEASHOOTER, 100, 7.5f, "PLANT_PEASHOOTER",
    ResourceKeys::Textures::IMAGE_PEASHOOTER, AnimationType::ANIM_PEASHOOTER,
    ResourceKeys::Reanimations::REANIM_PEASHOOTER, Vector(-37.6f, -44),
    /*scale*/ 1.0f, &MakePlant<PeaShooter>);
// PotatoMine: scale 填 0.8f, &MakePlant<PotatoMine>
```

僵尸 5 条对称补 `1.0f, &MakeZombie<具体类>`（FastBucket → `&MakeZombie<FastBucketZombie>`）。

### 4.3 派发入口（GameDataManager 新公有方法）

```cpp
std::shared_ptr<Plant> CreatePlant(PlantType t, Board* b, int row, int col, bool preview) const {
    auto it = mPlantInfo.find(t);
    if (it == mPlantInfo.end() || !it->second.factory) {
        std::cout << "[GameDataManager] 未注册或缺工厂的植物类型: " << static_cast<int>(t) << std::endl;
        return nullptr;
    }
    const PlantInfo& i = it->second;
    return i.factory(b, t, row, col, i.animType, i.scale, preview);
}

std::shared_ptr<Zombie> CreateZombie(ZombieType t, Board* b, float x, float y, int row, bool preview) const {
    auto it = mZombieInfo.find(t);
    if (it == mZombieInfo.end() || !it->second.factory) {
        std::cout << "[GameDataManager] 未注册或缺工厂的僵尸类型" << std::endl;
        return nullptr;
    }
    const ZombieInfo& i = it->second;
    return i.factory(b, t, x, y, row, i.animType, i.scale, preview);
}
```

### 4.4 GameApp 收口

`InstantiatePlant` / `InstantiateZombie` 函数**签名保持不变**（Board 调用点零改动），函数体 switch 整体替换为一行委托：

```cpp
std::shared_ptr<Plant> GameAPP::InstantiatePlant(PlantType plantType, Board* board, int row, int column, bool isPreview) {
    return GameDataManager::GetInstance().CreatePlant(plantType, board, row, column, isPreview);
}
std::shared_ptr<Zombie> GameAPP::InstantiateZombie(ZombieType zombieType, Board* board, float x, float y, int row, bool isPreview) {
    return GameDataManager::GetInstance().CreateZombie(zombieType, board, x, y, row, isPreview);
}
```

`GameApp.cpp` 顶部删除 13 个**具体子类** include（9 植物 + 4 僵尸子类）；**保留基类 `Zombie.h` 并显式补基类 `Plant.h`**——因为 `Board.h` 对 `Plant`/`Zombie` 只前向声明，而两个 `Instantiate*` 的返回类型 `shared_ptr<Plant>`/`<Zombie>` 需完整定义。

## 5. 数据流

```
Board::CreatePlant(type,row,col)
  → GameAPP::InstantiatePlant(...)               // 薄委托（保留）
    → GameDataManager::CreatePlant(...)           // 查 mPlantInfo，取 factory + animType + scale
      → factory == &MakePlant<ConcreteT>          // 运行期指针 → 编译期类型
        → GOM::CreateGameObjectImmediateAsShared<ConcreteT>(LAYER_GAME_PLANT, ...)
```

## 6. 错误处理

未注册类型或 `factory==nullptr` → 打印日志并返回 `nullptr`，与现有 `default:` 分支语义一致。上层（`Board::CreatePlant` 等）已处理 nullptr 返回。

## 7. 验证（项目无自动化测试；用户在 VS F7 构建 + 实跑）

1. F7 构建通过（assistant 不构建；用户验证）。
2. 逐一种植已实现的 9 植物 + 5 僵尸，确认能正常创建、动画/位置正常。
3. **重点**：PotatoMine 视觉尺寸与改造前一致（scale 0.8 已正确由数据携带）。
4. 选卡预览（isPreview=true 路径）正常。
5. 读档创建路径（`CreatePlantWithID`/`CreateZombieWithID` 仍走 `InstantiatePlant`/`InstantiateZombie`）正常。
6. 触发未注册类型（如临时传入未登记枚举）走 nullptr 分支不崩溃。

## 8. 风险与缓解

- **include 重定位引入编译错误/环**：GameDataManager.h 只加前向声明 + 函数指针 typedef（无重型 include）；重型 include 全在 .cpp。风险低。
- **scale 迁移遗漏**：仅 PotatoMine 非 1.0，已在验证清单单列。
- **静态初始化顺序**：登记发生在 `GameDataManager::Initialize()` 运行期调用（`GameApp::InitializeResourceManager` 内，`GameApp.cpp:187`），非静态初始化期，无顺序问题。

## 9. 加一个植物的新流程（净效果）

1. `PlantType` 枚举加一项
2. 写植物类（头文件即可）
3. `GameDataManager.cpp` 加一行 `RegisterPlant(..., scale, &MakePlant<新类>)`（含全部数据 + 工厂）
4. 加 `Card` 条目

从 6 触点降到 4，两个 switch 及 scale/anim 重复彻底消失。
