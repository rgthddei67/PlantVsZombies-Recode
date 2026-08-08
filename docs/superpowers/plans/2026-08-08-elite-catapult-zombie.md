# 导流投篮车僵尸实现计划

**目标：** 在不复制普通投篮车状态机的前提下，完成导流投篮车类型、资源、屋顶径流联动、正式波次上限、存档与 AutoTest。

**架构：** `EliteCatapultZombie` 继承 `CatapultZombie`；Board 在径流满值锁行时只查询一次候选并修改最终行掩码；Zombie 基类提供每实体径流倍率扩展点。

**设计：** `docs/superpowers/specs/2026-08-08-elite-catapult-zombie-design.md`

## 任务

- [ ] 追加 ZombieType/AnimationType、工厂、开发表、AutoTest 名表和 gamedata/info 注册。
- [ ] 重构 CatapultZombie 的侧板、投臂和爆炸资源选择点，新增 EliteCatapultZombie 的 1000 生命与径流能力。
- [ ] Board 在自然锁行时纳入最近房屋的合格导流车，保持原抽取行数；Zombie 基类应用实例倍率。
- [ ] 增加当前波导流投篮车计数、正式生成门禁、存档恢复、换波与生存轮次重置。
- [ ] 用可复现脚本生成并锁定精英车资源，注册 reanim 与独立爆炸粒子。
- [ ] 增加状态观测和专项 AutoTest，并用 `clang-release` 构建及可见运行相关回归。
- [ ] 更新项目记忆，审计并同步相关技能契约，校验技能后提交并按仓库状态决定 push。
