---
name: reference_pvz_assets_worktree_autotest_gotchas
description: "PvZ 跑 AutoTest 的非显然坑——resources/font 不在 git(在 build/<preset>/)新 worktree 须先拷否则 ResourceManager 初始化失败；wait_frames/wait_seconds 字段名是 \"value\" 不是 \"frames\"；状态切换后/点击前 settle 帧要 >30；④蘑菇白天睡(level 1-9)夜晚醒(level 10-19)→测蘑菇产出须 goto_level 10+；⑤AutoTest 自动收集阳光→产阳光植物的数值验证看 dump_state 的 sun 字段(不是 suns 数组)；wait_seconds 是游戏秒(scaled)、超时看门狗是墙钟"
metadata:
  node_type: memory
  type: reference
  originSessionId: 5822e83e-fd71-42db-89fe-5c2ba4bca716
---

2026-06-22 在 git worktree 里做生存词条查看面板、跑 AutoTest 验证时踩到的两个坑（都耗了时间，且第一个直接和 CLAUDE.md 写的相矛盾）：

**① `resources/` 和 `font/` 不在 git 里，实体在各 `build/<preset>/` 下、紧挨 exe。**
- `git ls-files resources/ font/` = 0（未跟踪）。仓库根**没有** `resources/`/`font/` 目录；资产被直接放进了 `build/clang-release/resources`、`build/msvc-debug/resources`（和同级 `font/`），这才是运行时 `./resources/resources.xml` 的来源。
- CMakeLists 第 107 行注释明说"**只拷 Shader**到输出目录，resources/font 不拷"——所以 **CLAUDE.md 里"CMake copies font/resources/Shader next to it"是错的/误导**：只有 Shader/spv 由 POST_BUILD 拷，resources/font 是预先人工放进 build 目录的，CMake 不管。
- **后果**：开一个**新 git worktree** 时，它的 `build/<preset>/` 是全新的，只有 CMake 产物 + Shader，**没有 resources/font** → 跑 exe 报 `加载XML配置文件失败: ./resources/resources.xml，File was not found` → `ResourceManager 初始化失败` → exit -6/250，AutoTest 一步都跑不了。
- **解法**：从主仓库的 `build/<preset>/` 把 `resources` 和 `font` 整个拷进 worktree 的同名 build 目录即可（`robocopy <主>\build\clang-release\resources <worktree>\build\clang-release\resources /E`，font 同理；robocopy exit<8 都是成功）。build/ 被 gitignore，拷进去不会污染 git status。

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
- 关卡→背景：`GameApp::GetBackgroundID` level 1-9=GROUND_DAY、**10-19=GROUND_NIGHT**、其余 DAY。所以 AutoTest 测蘑菇要 `{"op":"goto_level","level":10}`，白天 level 1 种下去 SunShroom 直接睡、永不产阳光。
- `plant` op 直调 `Board::CreatePlant`（绕过选卡/费用），`choose_cards` 对未拥有的卡会自动 `AddCard`，所以任意关卡都能种任意植物起局——但**睡眠是按背景判的**，绕不过，只能选夜晚关。

**⑤ AutoTest 模式自动收集阳光——产阳光植物的数值验证看 `dump_state` 的 `sun` 字段，不是 `suns` 数组。**
- AutoTest 下 `GameAPP::mAutoCollected` 为真，阳光一产出就被自动收走 → `Board::AddSun(SunPoint)` → `state.json` 的 `"sun"` 增加；到 `dump_state` 时 `suns` 数组通常已空。
- 这反而是**确定性数值验证**的利器：`set_sun 0` 起底、种产阳光植物、等一次产出、`dump_state` 看 `sun` 精确等于该植物的单次产值（小阳光=15、普通=25、向日葵=25）。比截图比大小更硬。小阳光验证即用此法：sun 0→15 直接证明走了 SmallSun(15) 而非 Sun(25)。
- 注意 `wait_seconds` 累加的是 **scaled 游戏秒**（`GetDeltaTime`），超时看门狗是**墙钟**（`GetUnscaledDeltaTime`，默认 15s）；`set_timescale N` 只缩短墙钟、不改游戏秒阈值。等某植物计时器到点要按**游戏秒**算 wait 值，timescale 调高只为让这些游戏秒在墙钟上更快走完（别超 15s 看门狗）。

教训：诊断 AutoTest 时序"诡异"先核对脚本字段名对不对（读 TestDriver 的 `cmd.value("…")` 实参），别先假设是引擎/暂停问题；冷启动首个交互前给足 settle；测蘑菇用夜晚关；产阳光看 sun 字段。关联 [project_pvz_perk_system](project_pvz_perk_system.md)、[project_pvz_autotest_suite](project_pvz_autotest_suite.md)、[project_pvz_cmake_migration](project_pvz_cmake_migration.md)。
