---
name: project_pvz_weather_jammer_zombie
description: 第七大关气象干扰僵尸、整栏预报干扰、铁桶与独立背包装置、存档和验证契约
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 气象干扰僵尸与整栏预报干扰

## 身份、数值与投放

`ZOMBIE_WEATHER_JAMMER` 复用标准 `BucketZombie` 本体、动画和 1100 点可磁吸铁桶，本体仍为
270；设备本身不进入磁性物品接口。权重 2600、第五波起、生存第 13 轮起，每个正式波次最多
两只。7-6（内部 level 60）出怪池为普通、橄榄球、粉色橄榄球、气球、精英跳跳、气象干扰僵尸，并在第三波
额外保底一只：不消耗波次点数，但消费本波两个名额之一。7-7 暂退，7-8/7-9 进入组合关。

## 施法与终止语义

僵尸完全进入战场且 Board 当前累计黑障余时未到 300 秒上限时，READY 僵尸立即抢占移动和啃食，不再等待公开预报：基类窄 helper 同帧平衡目标
植物 `mEaterCount`、清空植物/僵尸目标 ID 并调用 `OnStopEating()`，随后原地播放 `anim_idle`
施法 4 游戏秒。施法期间 `ZombieMove` 与 `StartEat` 均早退；慢速让倒计时按 0.5 倍推进，冻结、
黄油与麻痹通过基类更新门禁暂停。

提交时由 `Board::BeginWeatherPanelInterference(30.0f)` 向整栏黑障当前余时追加 30 游戏秒，返回 bit0 雨雪/
待生效台风警报、bit1 寒潮、bit2 雾势与表示窗口成功补时的 bit3；即使提交当帧没有公开预报也会
正常消费能力。窗口期间后来公开的预报会被每帧截获。另一只同类在已有窗口期间可直接开始自己的
4 秒施法并再次追加 30 秒，避免两次贡献之间出现天然真空期。Board 把当前同时余时饱和在 300 秒；
达到上限的 READY 来源等待出现余量再起手。该上限只限制当下剩余时长，不改变每波最多两只的正式生成
上限，也不限制整局累计成功次数。
通用外部中断取消尚未提交动作并进入 5 游戏秒 REBOOTING；此时允许走路和啃食，结束后重试。
死亡、断头、断臂、魅惑会永久转为 SPENT；魅惑僵尸永不干扰。提交成功的 Board 结果不因来源
僵尸后来死亡或魅惑而回滚。

## Board 预报与存档

整栏干扰只改变公开可见性，不删除锁定计划。雨雪、pending 台风、寒潮和雾势的真实下一结果、
阶段计时与正式切档仍照常推进。30 秒窗口期间天气栏压缩为 72px 并只显示“气象信号受干扰”；
当前雨雪、台风/风向、寒潮实况与倒计时、雾势、径流和雷荷文字全部隐藏，独立温度计保留。雨雪/雾势各用独立 disrupted 标志，
寒潮复用既有 `DisruptColdWaveForecast()`，以稳定植物 ID 通知融雪投手、伏霜雷等依赖者撤销尚未
提交的准备。被干扰的公开大雨不显示 5 秒分级警报，也不产生揭晓失败卡；若专用大雨/暴雪中央
预警、当前天气牌或失败卡已经显示，提交会立即撤下，且不影响其他并存提示。切档仍消费原 pending
台风。disrupted 标志在对应预报揭晓或清理时归零；窗口结束不提前公开仍未揭晓的已截获预报。

关卡 schema v5 为 v4 旧档补 `weatherForecastDisrupted=false`、
`fogWeatherForecastDisrupted=false` 和 `weatherJammersSpawnedThisWave=0`；预发布档已有值不覆盖。
同为 v5 的新增 `weatherPanelInterferenceTimer` 是中性可选字段，旧档缺失按 0 秒恢复，有值则按运行
时相同的当前余时上限夹紧到 0～300 秒；Board 先恢复该计时，再恢复 UI，因此读档不会短暂泄露被遮蔽栏目。
僵尸额外保存 phase、4 秒施法余时、5 秒重启余时和已提交 mask；加载会把死亡、断肢、断头、
魅惑、已提交却非 SPENT 或零余时 CHANNELING 等非法组合收口为 SPENT。Board 先恢复天气，随后
僵尸恢复，因此已提交的公开遮蔽和能力消费可在同一快照中一致重建。

## 视觉与资源

手绘卡通设备由 `scripts/assets/weather_jammer_*_source.png` 和
`scripts/generate_weather_jammer_assets.ps1` 可复现生成。背包子 Animator 挂在 `Zombie_body`，
代码局部偏移 `(27,3)`；背包 reanim 主壳首变换 `(-24,-30)`、缩放 `0.78`，碟面变换
`(22,-39)`、缩放 `0.54`，最终与肩背和腰线重叠而不是落地或并排。手持终端改挂稳定前臂
`Zombie_outerarm_lower`，局部偏移 `(-22,27)`，与低垂手掌保持重叠，断臂后解绑并释放。终端
channel 不再做缩放脉冲；雷达 ready/channel 往复序列的首尾角差分别压到 1°/2°，消除循环瞬移。ready/channel/reboot/spent
四套贴图反馈设备状态，SPENT 和死亡均转暗；铁桶掉落或磁吸不影响设备。

两个设备 reanim 的全部轨道都固定为 48 个 `<t>`。初版终端 marker 轨只有 13/25/37 帧，4 秒
提交切到约第 36 帧后在 `Animator::DrawInternalInstanced` 越界并触发 Access Violation；不能用
渲染空指针守卫掩盖。生成器现在解析每个 reanim 并断言全轨总帧数一致，默认实例路径和
`-NoInstance` 都从同一资源闭环验证。

## 2026-08-25 验证

2026-08-25 叠加补时修订后，`clang-release` LTO 构建 exit 0、378 项 Win7 导入审计通过，
`save-schema` 与 `save-migration` 纯测试通过。当前桌面可见 AutoTest：
`smoke_weather_jammer_priority_retry` 66 条命令覆盖无预报主动施法、啃食原子抢占、单次 30 秒窗口、
严格暂停冻结、第二只活动期起手、叠加到 55.55 秒、叠加值快照、跨第一份贡献原到期点、10 倍速最终
到期、300 秒同时余时上限投影和场外不起手；`smoke_weather_jammer_winter_commit` 58 条覆盖
窗口内后来发布的雨雪/寒潮预报、活动大雪与寒潮实况整栏隐藏、温度计保留和双方快照。叠加改动前
的同日主动黑障修订还曾通过雾势/台风、装备/断肢、外部中断、硬控、原天气预报与默认/
`-NoInstance` 视觉专项；本次只改 Board 余时合成和同类起手门禁，未重跑这些未受影响用例。既有
视觉资源、投放、数值和每波上限没有改动。
