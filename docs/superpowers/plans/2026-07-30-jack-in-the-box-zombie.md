# 小丑僵尸实现计划

**目标：** 按已确认 spec 实现小丑僵尸状态机、完整音效、断肢断头、专属爆炸、存档、冒险出怪和可见 AutoTest。

**Spec：** `docs/superpowers/specs/2026-07-30-jack-in-the-box-zombie-design.md`

- [x] 接入枚举、动画类型、资源键、工厂注册和 gamedata。
- [x] 实现 `JackInTheBoxZombie` 的 RUNNING/POPPING 状态机与 45/66/89 帧事件。
- [x] 实现循环音效引用生命周期及 boing/surprise/explosion/limbs 音效。
- [x] 实现敌我分流的圆形爆炸结算、屏幕震动和专属 `JackExplode`。
- [x] 实现 lower2 断臂残肢、专属手臂粒子、原版普通完整头粒子及读档视觉重建。
- [x] 入库 reanim、46 张部件图、4 条音效、2 张粒子图与 2 份粒子 XML。
- [x] 将四大关 4-1/4-2 的读报占位替换为小丑并补图鉴文本。
- [x] 扩展 AutoTest 状态投影，编写完整行为、循环声所有权、4-1/4-2 图鉴和樱桃扩散脚本。
- [x] 修复小丑与樱桃爆炸云的高阻力聚团，并锁定世界包围盒宽高至少 200px。
- [x] 运行 clang-playtest 0 warning 构建和当前桌面可见 AutoTest；检查退出码、run.log、状态和截图。
- [x] 更新项目记忆，审计 adding-zombie / adding-particle 技能并校验，提交并在安全时推送。
