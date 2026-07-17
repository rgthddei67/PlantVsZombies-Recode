---
name: project_pvz_icefumeshroom
description: 寒冰大喷菇(自创变种)a70506b+削弱de5e528均commit未push；数值主人已裁定中档(2.5s间隔/2.0s减速/150阳光)；AOE减速=乘法光环的平衡论证；Trophy按枚举值=关卡号解锁的发现
metadata:
  node_type: memory
  type: project
  originSessionId: edc4e752-c86b-4448-b25b-48cf8d948ce7
---

寒冰大喷菇 IceFumeShroom（2026-07-15, a70506b 初版 + de5e528 削弱，未 push）。大喷菇变种：
蓝 overlay(80,80,255,240=减速同色)、射程 360(-30)、蓝孢子云 IceFumeCloud.xml。
**数值终版（主人裁定"中档"）：150阳光/7.5s冷却/间隔2.5s/10伤/减速2.0s**。
平衡论证（再调数值先读这段）：AOE 减速是**乘法光环**——0.6速让整行输出×1.67、行越肥越超值，
且减速连啃食都慢40%（移动/啃食都动画驱动，GetTrackVelocity×动画速度）；
**减速时长必须<攻击间隔**制造恢复空窗（2.0/2.5=80%覆盖率→整行增伤+47%）；
减速力度不按来源削（蓝色=0.6速是全局视觉语言，狂暴铁桶0.8是按僵尸的抗性=另一个轴；
按来源强度需新机制+存档字段，不值）；若再嫌强下一刀砍射程（近场控制），不动力度。
改数值：售价 gamedata.json 免编译；伤害/射程/间隔在 IceFumeShroom::SetupPlant、
减速时长 kIceSlowSeconds（IceFumeShroom.cpp）；改间隔要同步 smoke 等待时序（断言余量≥0.5s防脆）。
AutoTest 的 plant 指令直造不扣阳光（同 spawn_zombie），sun 断言别指望扣款；卡面售价核对看截图。

结构：FumeShroom 常量提为 protected 成员 mFumeDamage/mFumeReach + 虚 FumeParticleName()/OnFumeHit(Zombie*)
（帧事件 lambda 里调虚函数→注册一份、运行时派发）；子类只写差异。帧 27 帧事件沿用未新增。

**发现（重要）：Trophy.cpp 用 static_cast<PlantType>(mBoard->mLevel) 解锁卡——PlantType 枚举顺序=关卡解锁顺序**。
新植物插枚举必须考虑占位：ICEFUMESHROOM 占 14（ICESHROOM 后、DOOMSHROOM 前）=下一关解锁；
已实现植物值不动才不破坏旧存档 havecards。

foot-guns：
- 穿透语义实测=护盾照常掉血**且**全额透到本体（TakeDamage penetrateShield，一喷铁门 shield-10 且 body-10），非"绕过护盾"。
- 持盾免减速在 SetCooldown 内部守卫（no-op），植物侧不用判；新减速来源守卫用 CanBeChilled() 虚函数。
- msvc-debug 的 info.txt 曾整体陈旧掉队（缺魅惑菇/胆小菇/铁门/橄榄球条目）——同步用整文件覆盖勿手补；寒冰菇 info.txt 条目此前漏了，本次一并补上。
- 粒子染色走 <ParticleRed/Green/Blue> 插值轨道(0..1,默认1)，不用做新贴图；卡图染色 ColorMatrix R,G×0.354 B×1 数学等价 overlay(80,80,255,240)。
- dump_state 僵尸新增 slowed bool 投影字段（slowCooldown 浮点勿 assert）。

关联 [project_pvz_iceshroom_freeze](project_pvz_iceshroom_freeze.md)（减速/冻结基建）、[project_pvz_gamedata_json](project_pvz_gamedata_json.md)（双preset）。
