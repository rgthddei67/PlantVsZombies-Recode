---
name: project_pvz_pink_football_zombie
description: 粉色橄榄球僵尸的夜晚出怪、首口重击、掉盔范围伤害、粉色资源与可见 AutoTest 验证
metadata:
  node_type: memory
  type: project
---

2026-07-20 新增 `ZOMBIE_PINK_FOOTBALL`：冒险模式只进入 2-8/2-9 的夜晚 spawnlist，生存随机池只允许黑夜无尽。数值为本体 220、头盔 900、位移倍率 1.85、动画 extra 1.95；按 `FastBucketZombie` 的 `GetSlowAnimFactor` 机制覆写为 0.7（减速动画保留 70%，即受 30% 影响）。对植物第一次真实啃食为 400 基础伤害，之后恢复 40。首次重击状态随 `SaveExtraData` 持久化，避免读档后重复触发。

掉盔沿用 `Zombie::HelmDrop` 的一类防具状态清理，再以僵尸逻辑坐标为圆心，对半径 120 像素圆内每株植物结算 50 点 `DamageSource::ZOMBIE` 伤害；判定使用平方距离避免开方。`PinkFootballZombie::SetupZombie` 直接调用 `FootballZombie::SetupZombie` 复用既有啃食/死亡帧事件，只补数值和速度差额，没有新增动画帧事件。

资源由 `scripts/recolor_pink_football.ps1` 使用 PowerShell/.NET `System.Drawing` 生成：只把橄榄球素材的红色材质映射到粉色，保持 PNG 尺寸、Alpha、绿色皮肤、白色条纹、银色护具和描边；两个 preset 均有独立 `PinkFootballZombie.reanim`、粉色掉盔/断臂粒子和图鉴文案。资源均需按 adding-zombie 规则用 `git add -f` 收编。

验证：初版及半径 120 调整时 clang-release 增量构建成功（C++ 编译/链接无 warning；vcpkg applocal 仍有环境既知的 objdump 缺失诊断但 exit 0）。主人当前桌面可见运行 `smoke_pink_football_helmet` 与 `smoke_pink_football_bite`，两者 exit 0、`run.log` 均 `script finished OK`；当时精确断言覆盖夜晚 spawn、220/900、半径 120 圆内两株核桃 4000→3975、圆外对角核桃保持 4000、首口 4000→3600、后续 3600→3560、死亡后 zombieCount=0，逐张截图确认站立/啃食/掉盔贴图无错位。后续将掉盔伤害调至 50、减速动画系数调至 0.7 时，主人明确要求不跑 AutoTest；`cmake --preset clang-release` 与 `cmake --build --preset clang-release` 均成功，C++ 编译/链接无 warning。

主人查收后的经验回写：①普通 shell 的 `Start-Process -WindowStyle Normal` 仍可能处于隔离会话，可靠可见启动必须同时满足 `build/<preset>` 工作目录和 `require_escalated` 桌面权限；已同步到常驻 `AGENTS.md`、`PROJECT_GUIDE.md` 与 adding-zombie 验证章。②同构变体直接调用父类 `SetupZombie()` 复用既有帧事件，再按目标倍率/父类倍率补速度差，避免重复注册事件；已改写 adding-zombie 原先“一律不调基类”的过度规则。③adding-particle 已完整覆盖本次掉盔/断臂粒子，没有新增粒子语义，故不做重复修改。
