---
name: project-pvz-iceshroom-freeze
description: 寒冰菇+僵尸冻结状态机已commit(bea4fa2)：StartFrozen/UpdateAnimSpeed单点收敛、冻结蓝光不耦合减速、豁免连伤害不吃、循环轨末帧事件可用
metadata:
  node_type: memory
  type: project
  originSessionId: 8fc581f0-a093-4424-b02d-ee4303ad460f
---

2026-07-14 寒冰菇 IceShroom 完成并 commit（bea4fa2，未 push）。skill adding-plant 已通用化并回写教训。

- **机制**：anim_idle 第16帧（区间0..16末帧，主人指定）帧事件→SOUND_FROZEN+GameScene::ShowScreenFlash 白闪+逐行 StartFrozen→Die()。忠实原版 HitIceTrap：首冻4~6s/已减速再冻3~4s、SetCooldown(20s)减速尾巴、TakeDamage(20)。75阳光/50s CD。
- **主人拍板**：持盾照冻但减速尾巴仍跳过（SetCooldown 持盾守卫保留）；魅惑/出土中伴舞免疫（CanBeChilled 虚函数）、跳跃中撑杆免冻吃减速（CanBeFrozen）；解冻特效不做。
- **架构点**：`Zombie::UpdateAnimSpeed()` = extra 速度层唯一收敛点（冻结0 > 减速×GetSlowAnimFactor(基类0.6/快速铁桶0.8覆写) > 常速）——报纸狂暴等直调 SetExtraSpeedMultiplier 的旁路已收编，加新速度状态先 grep 它。FastBucketZombie::SetCooldown 覆写已删。
- **foot-gun 实证**：①视觉别耦合别的效果——蓝 overlay 原绑减速，持盾冻结不变蓝被主人一眼抓出，改为冻结自带蓝光、ClearFrozen 时无尾巴才褪色 ②豁免语义连伤害一起豁免（原版伤害在免疫判定后），"伤害+状态"整体放 StartFrozen 别在植物侧拆开 ③**帧号口径（主人定死）：AddFrameEvent 真实帧号=预览帧号−1，但主人报的帧号默认已−1过、代码直接用不许再减**（寒冰菇16直用；勿再记成"末帧不可触发"）④resources.xml 音效条目双 preset 都要加（主人提醒）⑤原版素材库 `D:\PVZ\中文年度加强版完整版\Test\sounds\` 可按 TodFoley.cs 的 FoleyType→SOUND_XXX 捞原版音效。
- 死亡/断头/魅惑/读档各入口的冻结清理走 ClearFrozen/LoadProtectedData（frozenTimer 在 cooldown 恢复之后覆盖 extra=0+补蓝光）；dump_state 加 frozen(bool 可断言)+frozenTimer(浮点勿 equals)。
- 读档还原已由主人 2026-07-14 手动验证通过（冻结进行中存档→退出重进，停格/蓝光/冰晶/剩余时长均正确），无遗留。
- 后续 21227e2：spawnlists.json 首次补全黑夜 10~18（**9关一大关，2-9=level 18 才是黑夜收官**，19 已是 3-1）；18 初版为全阵容30波+sun字段初始阳光1500（LoadSpawnListFromJson 新增可选 sun，读档由存档覆盖）；dump_state 出怪池不再限生存模式；spawnlists.json 照 gamedata 先例 force-add 进 git。2026-07-21 的整大关节奏重排与当前最终阵容见 [project_pvz_night_spawnlist_pacing](project_pvz_night_spawnlist_pacing.md)。

关联 [project_pvz_scaredyshroom_and_adding_plant_skill](project_pvz_scaredyshroom_and_adding_plant_skill.md)、[project_pvz_animator_clip_speed](project_pvz_animator_clip_speed.md)、[project_pvz_charmed_zombie_feature](project_pvz_charmed_zombie_feature.md)。
