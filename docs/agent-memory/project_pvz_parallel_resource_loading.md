---
name: project_pvz_parallel_resource_loading
description: 资源加载并行化(治冷启动10s小文件风暴)——✅方案A达标已结算(基线exe已删)，5 commit 在 master 未 push
metadata:
  node_type: memory
  type: project
  originSessionId: f9501fff-0d97-463d-ad13-4e18c99c72c6
---

2026-07-02 完成实现+终审（f7b76fb..01561fc 在 master，未 push）。**2026-07-03 主人自然实验采数确认冷启动确实变快→方案 A 达标，已结算：基线 exe `PlantsVsZombies_baseline.exe` 已删（本就未被 git 跟踪，只是 build/clang-release 里的对照副本），不转方案 B。** 计时七阶段+WARN 汇总行(f7b76fb)保留为长期冷启动回归探针（Release 只剩一行 WARN，成本可忽略）。剩余：主人自行 push（push 是主人的活）。

**诊断**（主人确认热启动快→锁定）：冷启动 ~10s ≠ 带宽（资源仅 ~36MB，2490 个 PNG 平均 4KB），= ~2600 次文件 open 的固定成本（NTFS 冷缓存 + Defender 每文件首读扫描，~4ms/文件）。方案 A 多线程并行「打开+读取+解码」获批；方案 B（打包 pak）留作 A 不达标的后手——**若冷启动无明显改善，直接转 B，不在 A 上加码**。

**实现**（spec/plan 在 docs/superpowers/{specs,plans}/2026-07-02-parallel-resource-loading*）：
- f7b76fb：`LoadAllResources` 七阶段计时 + 一行 `LOG_WARN("Startup")` 汇总（Release 裁 INFO，WARN 是唯一采集通道）。基线 exe 副本 `build\clang-release\PlantsVsZombies_baseline.exe`（达标后删）。
- def7ac9：`LoadTexture` 拆成匿名 ns `DecodeImageFile`（线程安全纯解码，错误文案随 result 带回）+ 私有 `UploadDecodedTexture`（主线程，接管 surface）。
- 3978cd3：`ParallelDecodeAndUpload(jobs)`：worker=clamp(int(hw_conc)-1,2,8) 原子计数器领任务；**主线程严格按原序消费**（去重/上传/插入/日志与串行逐位一致）。三调用点改造，tiled 保持串行（key 带 `_PART_` 不相交）。
- 01561fc：终审补正——注释/spec 不再暗示背压（worker 无背压，峰值=本批总解码体积 ~30MB，量级小故无害）。

**结果**：热启动图片+reanim图 1.0s→0.5s（~2×）。**2026-07-02 主人首次重启采数：基线 1.1s / 新版 0.9s——不是 10s 场景！** 原因=当天反复跑过游戏，Defender 扫描结论缓存（按文件持久化，重启不清）还热；主人平常的 10s = 病毒库每日更新作废全部扫描缓存→次日首启全量重扫。**裁决未落锤**，改用自然实验：次日早晨首启跑基线（预期~10s）、再次日早晨首启跑新版（预期~2s）；或手动更新病毒库+重启强制复现。基线 exe 保留至裁决。终审(fable) Ready to merge=Yes，0 Critical/Important；ship-as-is 残留=①consume 循环 bad_alloc 会绕过 join→terminate（下次动此文件加 join-RAII）②仅失败时 tiled/非 tiled 错误日志顺序偏移。

**foot-gun/教训**：
- PowerShell 5.1 `1>` 重定向输出是 **UTF-16**——bash grep 搜不到中文计时行会误判"没打出来"，用 `Select-String`（会自动识别）或先确认编码。
- subagent API 断流（"Response stalled mid-stream"）后：先查 git log/工作树确认它做到哪，再 SendMessage 原 agentId 让它补报告，不要重派（会重做已提交的活）。
- `.superpowers/sdd/` 的 task-N-report.md 是**上个特性遗留**时要让实现者整文件覆盖，否则读到陈旧报告。
- `hardware_concurrency()` 无符号且可能为 0，先 cast int 再减 1（spec 阶段就写明，实现零踩坑）。

相关：[project_pvz_perf_optimization](project_pvz_perf_optimization.md)（运行时性能史）、[reference_pvz_assets_worktree_autotest_gotchas](reference_pvz_assets_worktree_autotest_gotchas.md)（AutoTest 坑）。
