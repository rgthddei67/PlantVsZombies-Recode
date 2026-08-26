---
name: project_pvz_furnace_core_flower
description: 第七大关炉芯花的温暖充能、冰像封存提交前硬阻断、统一卡图场上身份、存档与双路径验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-26
---

# 第七大关炉芯花

## 当前玩法合同

- `PLANT_FURNACECOREFLOWER` 是 7-7 通关奖励，当前为 175 阳光、20 秒冷却、300 生命；初始 0 枚炉芯，最多 2 枚。
- Board 实际温度严格高于 0°C 且植物可行动时，每 10 游戏秒生成一枚炉芯。低于或等于 0°C、停机或冰封时暂停并保留库存与部分进度；库存和进度入档，读档不重播反馈。
- 冰像处刑者建立冰封前调用 `Board::TryPreventIceExecutionSeal`。Board 快照并按植物 ID 排序 3×3 内有效提供者；第一株成功者消费一枚。目标从未冰封、不受首锤伤害，处刑者的一次性能力原子进入 `SPENT` 并退化为普通僵尸。
- 炉芯花不能保护自己；冰封、停机、被压扁、被蹦极抓取或非活动实例不能提供保护。它不解救已有冰封，不改变实际温度、冻土放置、天气或子弹。

## 视觉与资源合同

- 卡图与场上对象都使用橙红花瓣、木质炉膛脸和两枚火焰炉芯，避免卡槽与实物身份分裂；两者从 `docs/art/furnace-core-flower/` 的锁定源图由 `scripts/generate_furnace_core_flower_assets.ps1` 确定性生成。
- 场上复用 `SunFlower.reanim` 时间轴，但以独立资源名 `FurnaceCoreFlower` 注册，避免按 reanim 名反查身份冲突。木质炉膛替换 `anim_idle`，两枚 `Torchwood_fire1a` 使用稳定命名静态 follower 表示库存；没有子 Animator，也没有新帧事件。

## 验证证据

- `smoke_furnace_core_flower.json` 共 120 条命令，覆盖资源与数值、温暖充能、零度暂停和部分进度、两枚上限、存档、直接拒绝冰封、稳定首 ID、后继提供者、三只处刑者耗尽回退、自身不能救援、冰封停机及 7-7 奖励。
- 2026-08-26 当前源码的 `clang-release` 构建零警告且 Win7 378 项导入审计通过；专项在当前桌面可见默认 Vulkan 与 `-NoInstance` 均执行至 command 119、exit 0、`status=passed`。同步截图确认卡槽和场上共用木质炉芯花身份、一枚/两枚火焰状态、封存前硬阻断及冰封炉芯花不能自救。
