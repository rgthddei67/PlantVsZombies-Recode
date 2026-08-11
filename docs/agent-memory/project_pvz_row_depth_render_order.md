---
name: project_pvz_row_depth_render_order
description: 植物与僵尸按 mRow 交错绘制，保持同排僵尸在上且不改变小推车和子弹层
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-11
---

# 植物与僵尸逐行绘制深度

2026-08-11 修复屋顶上排行僵尸无条件覆盖下排行植物的视觉问题。旧实现虽然让 `Plant::GetSortingKey()` 与 `Zombie::GetSortingKey()` 都返回 `mRow`，但两个类型分别固定在 `LAYER_GAME_PLANT=10000` 和 `LAYER_GAME_ZOMBIE=20000`，行号只在各自类别内部生效。

## 当前契约

- `GameObjectManager` 在植物起点到子弹层之间把每行编成两个 1000 号段：`row N 植物/组合层 → row N 僵尸与扶梯`；随后才进入 `row N+1`。因此同排僵尸继续覆盖植物，下一行植物会覆盖上一行越界伸下来的身体。
- 对象的语义 `RenderLayer` 不变。小推车仍是 `LAYER_GAME_OBJECT`，子弹仍是 `LAYER_GAME_BULLET`；只改变植物/僵尸战场主体的实际 `renderOrder`。
- 大蒜改道、蹦极选格、屋脊督军换行、植物跨行搬格与扶梯随格移动在 `mRow` 改变时调用 `RefreshRenderOrderForSortingKey`，回收旧行号段并把对象放入新行号段；挂在僵尸语义层的焦黑残影继承死亡瞬间的行深度。
- `Board::RefreshPlantStackRenderOrder` 仍只在同格植物组合内部保持 `under → normal → pumpkin → overlay`，与跨行号段正交。

## 验证

- `clang-release` 配置与构建退出码 0。
- 可见 AutoTest `smoke_row_depth_render_order.json -Seed 42` 在默认实例化和 `-NoInstance` 两条路径均 exit 0、`script finished OK`。
- 屋顶目标号段为 `10000..10999（0 行植物）→11000..11999（0 行僵尸）→12000..12999（1 行植物）→13000..13999（1 行僵尸）`；小推车语义层仍为 0，子弹语义层仍为 30000。
- 同一脚本第二场让蹦极僵尸从出生行 0 选择第 2 行目标，并断言其绘制号刷新到 `15000..15999`；第三场断言第 2 行普通僵尸的焦黑残影仍处于同一僵尸号段。
- 两条路径截图都显示：上排行僵尸盖住同排花盆坚果，但其脚部被下一行花盆坚果正确遮挡。
