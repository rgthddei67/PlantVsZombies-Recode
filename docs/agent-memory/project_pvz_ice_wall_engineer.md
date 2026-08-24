---
name: project_pvz_ice_wall_engineer
description: 第七大关冰墙工程师、独立移动冰墙、弹道优先级、存档和验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-24
---

# 冰墙工程师与独立冰墙

## 身份与投放

`ZOMBIE_ICE_WALL_ENGINEER` 数值为 800 本体、370 安全帽、权重 2200、第五波起、生存第 11 轮起，每个正式波次最多生成一只。冒险接入 7-3、7-4、7-5、7-8、7-9；7-1/7-2 不提前泄露，7-6/7-7 为气象干扰与处刑者教学留白。

工程师复用 `ConeZombie.reanim` 的完整死亡、啃食和断肢时间轴，没有新增动画帧事件。初始及三档破帽都通过 `GetConeTextureKey()` 换图；身体轨道承载冰蓝工装、橙黄反光背带、工具袋和双箍制冰罐，领带轨道改为悬挂冰凿。可复现资产生成器为 `scripts/generate_ice_wall_engineer_assets.ps1`，输出哈希在脚本内锁定。

## 施工合同

工程师 collider 前缘越入当前第一冻结列的霜线后进入 `BUILDING`，停步 4 游戏秒并周期发射 `IceWallBuild` 碎冰。冻结、黄油和麻痹由基类早退自然暂停计时；断头、死亡、魅惑、冻土消失或全场已有墙时中止。全场已有墙的工程师到达施工点后永久消费本次能力，不会在旧墙破裂后回头补建。

完工通过 `Board::AddIceWall()` 原子提交全场唯一墙，随后工程师恢复行走。工程师和墙的生命完全独立；工程师死亡、断头或魅惑不拆除既有墙。

## 冰墙合同

`IceWall` 是由 `GameObjectManager` 持有的独立 `GameObject`，Board 只保存弱引用。初始 1800 生命、半宽 34px，以 14px/游戏秒向房屋移动；同行植物存在时在最近植物 collider 右缘外保留 8px 间隙停下，无植物时越过房屋侧回收。`THAWING` 或环境温度高于冻结阈值时以 120 生命/游戏秒融化，保留小数伤害余量。

所有同行非抛射、非玉米炮弹道在 `CollisionSystem` 僵尸回调之前以运动线段检查墙体，命中后不再伤后方僵尸；正式抛射轨迹越墙。火球和毒火球对墙两倍。融雪投手只在仍有盐晶库存时把墙作为目标；盐晶忽略沿途僵尸，在落点对现存墙结算 20 直击和 200 独立腐蚀，腐蚀不溢出到任何僵尸。

## 存档与验证

工程师额外保存施工阶段、剩余时间、目标墙中心、粒子节拍和能力是否消费；读档会修复断头、魅惑或零剩余时间的非法施工态。Board 保存每波工程师计数；墙保存行、连续中心 X、当前/最大生命和融化小数余量。旧档缺字段时保持空墙和零计数。

`smoke_ice_wall_engineer.json` 在 `clang-release` 默认 Vulkan 与 `-NoInstance` 可见路径均执行 99 条命令至 command 98、exit 0、`status=passed`；覆盖资源键、数值、平射先撞墙、抛射越墙、火焰双倍、盐晶 20＋200、施工中存读档、工程师死亡后留墙、回暖融化、每波上限和中心 X=519 的植物停点。`smoke_ice_wall_engineer_spawnlists.json` 覆盖 7-1～7-9 分布；融雪投手和冬日花园父回归同次 Release 通过。全量 LTO 编译、378 项 Win7 导入、`SaveSchemaTests` 与 `SaveMigrationTests` 均通过。
