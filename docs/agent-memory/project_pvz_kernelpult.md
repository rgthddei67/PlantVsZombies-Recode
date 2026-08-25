---
name: project-pvz-kernelpult
description: 经典玉米投手、玉米粒/黄油抛射、黄油定身、二类护盾与双绘制路径验证
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-25
---

# 经典玉米投手（Kernel-pult）

2026-08-04 完成。`KernelPult : Plant` 使用 100 阳光、7.5 秒卡冷却、300 生命；
首次攻击相位随机 0～3 秒，后续周期 2.86～3.0 秒，`anim_shooting` 按 35fps
播放，主人确认全局第 30 帧直接作为发射事件。每轮攻击开始时以 25% 概率选择
黄油，手持 `Cornpult_kernal/Cornpult_butter` 轨道随待发弹型互斥，发射后恢复玉米粒。

## 实现契约

- 玉米粒为 20 伤、0.95 缩放，黄油为 40 伤、0.8 缩放；两者共用卷心菜投手
  已验证的同行索敌、1.2 秒/210px 解析抛物线、目标提前量、下降末段碰撞、动态
  地形阴影、屋顶轨迹、对象池和正式存档字段。
- 两种投掷物主动请求绕过普通二类护盾，铁门与报纸只伤后层；加固铁门通过目标侧
  既有门禁否决绕过，伤害进入盾牌并继续服从单击上限。玉米粒播放
  `kernelpult/kernelpult2`，黄油播放 `butter` 并生成 `ButterSplat`。
- 合法黄油命中把僵尸完全定身 4 游戏秒，统一停止动画、移动、啃食和未离手的派生
  能力倒计时；冻结与黄油通过 `IsImmobilized()` 合成。飞行、无头、垂死、魅惑、
  水草抓取、冰车、Boss 及 `CanBeFrozen()` 否决阶段免疫定身，但仍承受弹丸伤害。
- 黄油时间进入 `Zombie::SaveProtectedData/LoadProtectedData`，旧档缺字段为 0；断头、
  死亡和魅惑立即清除。头贴通过基类 follower 默认跟随 `anim_head1` 完整仿射变换，
  并延迟到 Animator 末尾压在头发/眼镜等附件上方；主人对比原版后最终使用 0.8 倍
  `Cornpult_butter_splat.png`。异形轨道、巨人层内遮挡和批次契约见
  `project_pvz_zombie_butter_overlay.md`。
- `Cornpult.reanim`、卡图、三张运行时贴图、三段音效和 `ButterSplat.xml` 都从
  `build/clang-release/resources` 权威目录注册；资源键由 AutoTest 的
  `HasReanimation/GetTexture(key,false)/HasSound` 闭环断言。

## 验证证据

- 最终 `clang-release` 增量编译与 LTO 链接 exit 0；vcpkg applocal 仅报告本机未找到
  dumpbin/objdump 的既有探测提示，未中断构建。
- `smoke_kernelpult.json` 在主人当前桌面可见运行，默认实例化与 `-NoInstance`
  均 exit 0；每条路径 184 条命令、84 条状态断言全绿，日志无 FAIL、Fatal、
  WATCHDOG 或资源缺失。
- 专项覆盖资源/数值、无目标、30 帧发射、两种弹型与声音、手持黄油快照、飞行中
  快照、伤害/定身/到期、落空、对象池复位、普通二类护盾绕过、加固铁门例外、
  冰车免疫和屋顶。关键截图逐张检查，0.8 头贴与默认/慢路径显示正常。
- 共享 `smoke_cabbagepult.json` 在最终共享逻辑修改后可见 exit 0，日志无异常标记。
- 2026-08-09 通用黄油 follower 接入后，`smoke_zombie_butter_layers.json` 默认实例化与
  `-NoInstance` 均可见通过；随后 `smoke_kernelpult.json` 再次可见 `script finished OK`。
- 2026-08-06 共用目标平均根速度回归时，原脚本在第 30 帧理论边界只等待 0.32 秒，
  曾在播放头尚未越帧时提前断言发弹；玉米粒和黄油两处均改为 0.38 秒安全余量，
  不改变游戏发射时序。

## 2026-08-25 冰墙优先级

同行冰墙优先于僵尸且可单独触发攻击；已经在起手选定的玉米粒/黄油品种不重抽，发射帧把墙体
瞄准点和显式 `targetsIceWall` 标志写入弹道。玉米粒对墙 20、黄油对墙 40；黄油命中墙时只播放
原反馈，不给墙后僵尸附加定身。锁定标志入档并在对象池复位，当前父专项与共享锁墙专项均在
`clang-release` 可见路径通过。
