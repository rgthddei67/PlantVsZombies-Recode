# GameDataManager 数值 JSON 化（gamedata.json）设计

日期：2026-07-07
状态：已获主人批准
来源：改进 backlog #2「游戏数值 JSON 化」

## 目标

把 `GameDataManager::InitializeHardcodedData`（`Game/Plant/GameDataManager.cpp`）中硬编码的植物/僵尸**数值**外置到 JSON，调平衡/调偏移不再重编译。血量/速度/子弹伤害**不在本期范围**（它们散在各类构造里，牵扯词条缩放与存档，另行立项）。

## 已定决策（主人裁决）

1. **范围 = 仅 GameDataManager 注册表数值**：植物 `cost/cooldown/offset/scale`；僵尸 `weight/appearWave/survivalRound/offset/scale`。血量不做。
2. **JSON 是数值唯一事实来源**（非"硬编码保底+覆盖"）：代码里不再保留第二份数字，防止"改了代码忘了 JSON"。
3. **快速失败**：没有 JSON / 解析失败 → 报错、不进游戏。
4. **严格校验**：任何已注册类型缺条目、或条目缺**任何一个**必填字段 → 报错、不进游戏（主人原话：但凡只要少一个就报错）。
5. **offset/scale 一并进 JSON**：代码=身份，JSON=数值，边界干净。

## JSON 文件

- 路径：`resources/gamedata.json`（与 `spawnlists.json` 同级；CMake 既有 copy 规则自动带到 `build/<preset>/resources/`，AutoTest 零改动）。
- 结构：按**枚举名字符串**做键的对象（非数组）——天然去重、O(1) 查找、diff 可读：

```json
{
  "plants": {
    "PLANT_PEASHOOTER": { "cost": 100, "cooldown": 7.5, "offset": [-37.6, -44.0], "scale": 1.0 },
    "PLANT_POTATOMINE":  { "cost": 25,  "cooldown": 20.0, "offset": [-30.0, -35.2], "scale": 0.8 }
  },
  "zombies": {
    "ZOMBIE_NORMAL": { "weight": 1000, "appearWave": 1, "survivalRound": 1, "offset": [-50.0, -85.0], "scale": 1.0 }
  }
}
```

- 初始内容 = 现有硬编码值的逐字迁移（12 植物 + 9 僵尸），行为零变化。

## 代码改动

### GameDataManager（核心）

- `RegisterPlant` 瘦身为纯身份注册：`(type, enumName, textureKey, animType, animName, factory)`；删 sunCost/cooldown/offset/scale 参数。
- `RegisterZombie` 同理：`(type, enumName, animType, animName, factory)`；删 weight/appearWave/survivalRound/offset/scale 参数。
- `PlantInfo`/`ZombieInfo` 结构体的数值字段保留，仅由 JSON 回填。
- 新增私有 `bool LoadNumbersFromJson()`：身份注册完成后调用，`FileManager::LoadJsonFile("./resources/gamedata.json", ...)` 读取，按 enumName 回填每个已注册类型。
- `Initialize()` 返回 `bool`。

### 调用点

- `GameAPP.cpp`（`InitializeResourceManager` 内，现 179-180 行）：`plantMgr.Initialize()` → `if (!plantMgr.Initialize()) return false;`（该函数已有 bool 失败路径，天然接入）。

### 校验规则（收集全部错误后一次性报告，非遇错即停）

| 情形 | 处理 |
|---|---|
| 文件缺失 / 解析失败 | 失败 |
| 已注册类型无条目 | 失败，报 `gamedata.json: 缺 PLANT_X 的条目` |
| 条目缺必填字段（植物 4 个 / 僵尸 5 个） | 失败，报 `PLANT_X 缺字段 "cooldown"` |
| 字段类型错（如 offset 非双元素数字数组） | 失败，同上格式 |
| JSON 有未注册的多余键 | `LOG_WARN` 不阻断（避免删类型后旧 JSON 锁死游戏） |

失败呈现：逐条 `LOG_ERROR` + 汇总 `SDL_ShowSimpleMessageBox`（nullptr 父窗口，建窗前也能弹）→ `Initialize()` 返回 false → 启动中止。

## 配套更新

- `InitializeHardcodedData` 顶部「新增植物 4 步」注释改 5 步：新增「在 gamedata.json 加条目」；严格校验保证漏了会在启动时指名道姓报错。
- `DebugPrintAll` 增补打印 cost/cooldown/weight/survivalRound 等生效数值，作为"当前生效值"权威查询口。

## 验证

1. `clang-release` 构建零警告。
2. 现有 AutoTest 回归：`demo_peashooter` + `smoke_perks` 等，`-Seed` 固定下产物与改造前一致（固定步长主循环保证逐字节确定性）。
3. 负向测试（手动，AutoTest 无法覆盖启动失败）：
   - 改名 `gamedata.json` → 退出码非 0，日志含 ERROR；
   - 删一个字段 → 拒绝启动，错误信息含枚举名+字段名。

## 非目标

- 血量/护甲/速度/子弹伤害外置（第二期候选）。
- BulletPool 的子弹 switch 收编（见 backlog）。
- spawnlists.json 结构变更。
