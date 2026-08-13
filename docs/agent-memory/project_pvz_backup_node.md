---
name: project-pvz-backup-node
description: PvZ Git 双备份、AutoTest 证据离机归档与轻量 Linux 节点巡检契约
metadata:
  node_type: memory
  type: project
---

# PvZ 轻量备份节点

2026-08-13 建立不承担游戏构建的 Linux 后勤节点：

- 本机通过受限 `git` SSH 账号推送到 `/srv/git/PlantsVsZombies.git`；远端拒绝删除和非 fast-forward 推送。
- GitHub 每日同步到 `refs/mirrors/github/heads/*` 与 `refs/mirrors/github/tags/*`，不得覆盖通过 SSH 推送的 `refs/heads/*`。
- `scripts/upload_autotest_artifacts.ps1` 只打包 `build/<preset>/autotest/out/<script-name>/`，强制包含 `run.log` 与 `status.json`，记录 Git 状态、AutoTest 状态和逐文件 SHA-256；PNG 缺失只警告，失败用例仍可归档。
- 服务端以 systemd timer 每日镜像 GitHub、每日生成只读健康报告、每周执行完整 Git fsck；tmpfiles 保留 AutoTest 证据 90 天、健康报告 30 天。
- 所有功能复用 SSH，不新增公网端口；部署文件不得包含服务器地址、认证材料或 Hysteria 配置。Linux 节点不是 Windows `clang-release` 构建和可见 AutoTest 的替代品。

运维说明见 `docs/operations/pvz-backup-node.md`，服务端模板见 `scripts/server/pvz-backup-node/`。
