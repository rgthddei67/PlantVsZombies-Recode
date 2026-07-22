---
name: reference_pvz_assets_worktree_autotest_gotchas
description: "PvZ 运行资产与 AutoTest 非显然坑——clang-release 持有单份 resources/font，其他 preset 用 Junction；wait字段名是value；切换后settle要大于30帧；蘑菇夜测用九关制10-18；产阳光看dump sun字段"
metadata:
  node_type: memory
  type: reference
  originSessionId: 5822e83e-fd71-42db-89fe-5c2ba4bca716
---

2026-06-22 在 git worktree 里做生存词条查看面板、跑 AutoTest 验证时踩到的两个坑（都耗了时间，且第一个直接和 CLAUDE.md 写的相矛盾）：

**① 运行资产只有一份实体：`build/clang-release/resources` 与 `font`。**
- 2026-07-22 起，`clang-playtest`、`msvc-debug` 首次 configure 会自动创建指向上述实体目录的 NTFS Junction；后续构建不会复制 40MB 资源。每个 preset 的 Shader、存档和 AutoTest 输出仍独立。
- 新增或修改 `gamedata.json`、`spawnlists.json`、`info.txt`、粒子配置、贴图等，只改 clang-release 的权威资源。不要再维护或提交 `build/msvc-debug/resources` 副本。
- 新 worktree 仍没有原版未跟踪资产：先把主仓库的 `build/clang-release/resources` 与 `font` 各复制一次到新 worktree 的 `build/clang-release/`，随后 configure 其他 preset 会自动建立联接。缺资产时仍会在初始化阶段 exit -6/250。

**② AutoTest `wait_frames` 的 JSON 字段名是 `"value"` 不是 `"frames"`。**
- `TestDriver.cpp` 里 `if(op=="wait_frames"){ mFramesLeft = cmd.value("value", 0); ...}`——读 `"value"`。写成 `{"op":"wait_frames","frames":15}` 会 `value("value",0)` 取默认 **0** → "立即完成"，**静默当没等**（不报错）。`wait_seconds` 同理用 `"value"`。
- 症状：截图截在状态切换前一帧（如点击开面板后面板还没渲染、点击关面板后关闭还没处理）→ 截图内容看着"反了"。`wait_frames` 本身按渲染帧计数（`++mFrame` 每帧、与 DeltaTime/暂停无关），**暂停时也照常推进**，所以面板暂停态下也能等。

**③ 状态/场景切换后、注入 click 或 screenshot 之前，settle 等待帧要给够（>30），别太短。**（主人 2026-06-22 明确指示）
- 现象：`wait_state GAME` 后**紧接**一个开面板 `click`，在**冷启动**（exe 刚起、Vulkan 管线 + 字体/纹理预热，头几帧特别长）时，这一次 click 可能被预热长帧吞掉 → 面板没开 → 随后四张 `screenshot` 全是空白草坪（面板就没出现过，后续翻页/关闭 click 也都打空）。再跑一遍（已预热）就正常。
- 根因关联：`add_perk` 这类 op 是**直调零帧**（瞬时改 PerkManager，不耗帧），所以脚本里 `wait_state GAME` 之后的开面板 click 是**紧贴**状态切换的，没有自然缓冲。
- 解法：在 `wait_state`/场景切换与首个 click 之间插 `{"op":"wait_frames","value":30}`（或更大）。这是**防御性**而非已复现根因修复——单次 flake 难复现，但 settle 纯加延时、零正确性风险，让冷启动也稳。smoke_perk_view 即据此加了 30 帧（commit 4e3e1b7）。
- 记忆口径：**AutoTest 脚本暂停时间不要太短，>30 帧**，不然截图截不到（截在动作生效前）。

**④ 蘑菇类植物（Shroom 子类）白天睡觉、夜晚才醒——测蘑菇产出/行为必须用夜晚关卡。**（2026-06-23 做小阳光/SunShroom 时踩到）
- `Shroom::SetupPlant`：`GetBackgroundIsNight(mBoard->mBackGround)` 为真才 `SetSleepState(false)`，否则 `SetSleepState(true)+PlayTrack("anim_sleep")`，睡着不跑产出逻辑。
- 关卡→背景：`GameApp::GetBackgroundID` 统一按每大关 9 小关分段：level 1-9=GROUND_DAY、**10-18=GROUND_NIGHT**、19-27=WATER_POOL。所以 AutoTest 测蘑菇要 `{"op":"goto_level","level":10}`，白天 level 1 种下去 SunShroom 直接睡、永不产阳光。
- `plant` op 直调 `Board::CreatePlant`（绕过选卡/费用），`choose_cards` 对未拥有的卡会自动 `AddCard`，所以任意关卡都能种任意植物起局——但**睡眠是按背景判的**，绕不过，只能选夜晚关。

**⑤ AutoTest 模式自动收集阳光——产阳光植物的数值验证看 `dump_state` 的 `sun` 字段，不是 `suns` 数组。**
- AutoTest 下 `GameAPP::mAutoCollected` 为真，阳光一产出就被自动收走 → `Board::AddSun(SunPoint)` → `state.json` 的 `"sun"` 增加；到 `dump_state` 时 `suns` 数组通常已空。
- 这反而是**确定性数值验证**的利器：`set_sun 0` 起底、种产阳光植物、等一次产出、`dump_state` 看 `sun` 精确等于该植物的单次产值（小阳光=15、普通=25、向日葵=25）。比截图比大小更硬。小阳光验证即用此法：sun 0→15 直接证明走了 SmallSun(15) 而非 Sun(25)。
- 注意 `wait_seconds` 累加的是 **scaled 游戏秒**（`GetDeltaTime`），超时看门狗是**墙钟**（`GetUnscaledDeltaTime`，默认 15s）；`set_timescale N` 只缩短墙钟、不改游戏秒阈值。等某植物计时器到点要按**游戏秒**算 wait 值，timescale 调高只为让这些游戏秒在墙钟上更快走完（别超 15s 看门狗）。

教训：诊断 AutoTest 时序"诡异"先核对脚本字段名对不对（读 TestDriver 的 `cmd.value("…")` 实参），别先假设是引擎/暂停问题；冷启动首个交互前给足 settle；测蘑菇用夜晚关；产阳光看 sun 字段。关联 [project_pvz_perk_system](project_pvz_perk_system.md)、[project_pvz_autotest_suite](project_pvz_autotest_suite.md)、[project_pvz_cmake_migration](project_pvz_cmake_migration.md)。
