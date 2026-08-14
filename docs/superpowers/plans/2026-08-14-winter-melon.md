# 冰瓜实施计划

1. 将 `MelonPult` 的投射物类型收敛为可覆写品种点，新增 `WinterMelon` 并登记紫卡基础株、动画、卡图、gamedata、图鉴与 6-5 奖励。
2. 完整接通 `BULLET_WINTERMELON` 的对象池、100 点伤害、三行溅射、二类盾穿透、10 秒群体减速、独立贴图、音效和存档表现。
3. 迁移原始冰瓜碎片贴图与 `WinterMelonSplash` 配置，闭合 `resources.xml`、manifest、资源键和 AutoTest 加载断言。
4. 新增 `smoke_winter_melon.json`，并同步修正已与当前 120 点西瓜权威数值漂移的西瓜专项期望。
5. 运行 `clang-release` 配置/构建、桌面可见默认与 `-NoInstance` 专项，检查退出码、日志、状态、断言与截图。
6. 审计并更新新增植物、粒子技能与项目记忆，运行 skill 校验后提交并按仓库状态评估常规 push。
