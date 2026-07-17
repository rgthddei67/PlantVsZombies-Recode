---
name: project_pvz_smallsun_sunshroom_review
description: "2026-06-23 code-review of 小阳光/SunShroom/Animator-playstate-save/Profiler-gate changes — findings + what's verified-correct"
metadata:
  node_type: memory
  type: project
  originSessionId: a9a1c579-34b6-4786-b865-7230fa3696c7
---

2026-06-23 我对 53657f2..133a9f1（6/22 之后的全部更改）做了 code-review（xhigh）。范围=小阳光 SmallSun + SunShroom 植物 + GameInfoSaver 通用 Animator 播放状态机存读档(SaveAnimState/RestoreAnimState) + Profiler -Profile 门控 + 生存词条平衡数值。

**核实为正确(不要再当 bug 报)**：① 通用 SaveAnimState/RestoreAnimState 与旧逐类型代码等价(僵尸 clip 经 PlayTrack(track,clipSpeed) 参数恢复，等价于旧的 SetClipSpeed-after；SetCurrentFrame 末位)；② SmallSun 存读档经 `s["small"]=dynamic_cast<SmallSun*>(coin)` + CreateSmallSun(WithID) 闭环，SunPoint=15 在收集时(OnReachTargetBack 读虚数据)生效；③ PLANT_SUNSHROOM 注册完整(PlantType.h/GameDataManager/IMAGE_SUNSHROOM in ResourceKeys.h/TestDriver 名表，Card 数据驱动派生)；④ ANIM_SUNSHROOM 经 RegisterPlant 的 `mAnimToString[animType]=animName` 解析，无 -Wswitch 风险(AnimationType 无 exhaustive switch)；⑤ **植物产的阳光不抛物线/不下落**(StartLinearFall 仅天降阳光 Board.cpp:248 调，StartParabolaFall 是死代码)→smoke_small_sun.json wait 7 游戏秒能收集到→dump_state.sun=15 成立(sweep agent 的"2s 抛物线"前提是错的)；⑥ PLAY_NONE→PLAY_REPEAT、OpenPerkView IsPaused 守卫=既有行为/有意硬化，非新 bug。

**真实发现(都非严重,无崩溃)**：SunShroom.h 缺 include guard(全仓唯一)；SunShroom.h 头文件里 `namespace{ float GROW_TIME/PRODUCE_TIME }` 非 const 可变全局每 TU 一份(应 static constexpr 类成员)；Profiler.h "零开销"不实(ScopedProfile 构析仍 2×Clock::now()+Add 形参把 const char* 构造成 std::string 临时，门控在 Add 内部为时已晚，应下移到 ScopedProfile)；SunFlower.h private→protected 是废动作(SunShroom 继承 Shroom 非 SunFlower 且自己重声明三个计时成员)；AnimationTypes.h 加 8 个枚举只用 1 个(7 个 FUMESHROOM/GRAVEBUSTER 等是投机死值)；GetPlantRegenHpCap 注释"满层(达 maxStacks)…调参自动跟随"在 PLANT_REGEN maxStacks 7→8 后自相矛盾(解锁硬编码 >=5≠满层8)；SunShroom.cpp 注释"2.5秒发光"实为 0.55s(从 SunFlower 抄错)；grow-mid-glow 边界一次性按 mIsGrown 在发光"末"判定大/小阳光(15 vs 25 极小偏差)；3 文件缺行末换行 + CMakeLists 多个空格。

教训：本仓库 reanim 播放状态(PlayState)默认 PLAY_NONE，PlayTrackOnce 永远写 PLAY_ONCE_TO(即使 returnTrack 空)，一次性播完→PLAY_REPEAT+mIsPlaying=false。see [project_pvz_animator_playstate_save_fix](project_pvz_animator_playstate_save_fix.md) [project_pvz_animator_clip_speed](project_pvz_animator_clip_speed.md) [reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)

**2026-06-23 后续：主人要修 1/3/4/5/6/9/10，已全做+clang-release 0warning+smoke_small_sun 实跑 sun=15 验过，commit 5b7c1a4(master 未push)**。修复 1=发光"启动时"快照成熟度 mGlowProducesGrownSun(随存档)，修长大边界发光中途错付 25↔15；4=GROW_TIME/PRODUCE_TIME 改类内 static constexpr(C++17 隐含 inline,cpp 裸名不用改)；5=Profiler 门控下移到 ScopedProfile 构析。
**+主人采纳"自动头文件守卫"方案 commit 4aeb8c1(master 未push)**：`.githooks/pre-commit`(拒绝缺 #pragma once/#ifndef 的 staged .h，读 staged blob，**BOM-aware=头512字节查token存在性不锚^**——本仓库 BOM/无BOM 头混用，锚^会把 BOM+#pragma 的文件如 GameProgress.h 误判) + CMakeLists configure 期 `git config core.hooksPath .githooks` 自动启用 + WARNING 列遗漏头；约定写进 CLAUDE.md Coding Conventions。BOM-aware 是本仓库任何 .h lint 的必备前提(grep `^\s*#pragma` 在 BOM 文件上会假阴)。
