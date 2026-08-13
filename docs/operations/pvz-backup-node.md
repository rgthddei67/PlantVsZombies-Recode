# PvZ 轻量备份节点

这套配置把一台小型 Linux VPS 用作 Git 与 AutoTest 证据的辅助副本。它不承担 Windows 游戏构建，也不改变 Hysteria 等既有网络服务。

## 数据边界

- `/srv/git/PlantsVsZombies.git` 是通过 SSH 接收的权威裸仓库；禁止删除远端分支和非 fast-forward 推送。
- `refs/mirrors/github/heads/*` 与 `refs/mirrors/github/tags/*` 是服务器每日从 GitHub 取得的独立镜像引用，不覆盖 `refs/heads/*`。
- `/srv/pvz-artifacts/runs/<run-id>/` 保存 `evidence.zip`、独立 `manifest.json` 和 `SHA256SUMS`。
- AutoTest 证据保留 90 天，健康报告保留 30 天。VPS 不是唯一副本，仓库仍应保留本机与上游托管副本。

## 上传 AutoTest 证据

AutoTest 结束后，从仓库根目录执行：

```powershell
$env:PVZ_ARTIFACT_SERVER = '<server-address>'
.\scripts\upload_autotest_artifacts.ps1 -TestName smoke_example
```

脚本默认读取 `build\clang-release\autotest\out\<script-name>\`。其他预设必须显式传入 `-Preset`。上传前可加 `-DryRun`，它会校验、生成清单和压缩包，但不连接服务器。

清单记录提交号、分支、工作区是否有未提交改动、AutoTest 的 `status.json` 以及每个证据文件的 SHA-256。失败用例也允许归档；缺少 `run.log` 或 `status.json` 时拒绝上传，没有 PNG 时只警告。

## 服务端定时任务

- `pvz-github-mirror.timer`：每日同步 GitHub 引用。
- `pvz-git-integrity.timer`：每周执行 `git fsck --full --strict`。
- `pvz-server-health-report.timer`：每日把只读健康快照写入 `/srv/codex/health/`。
- `systemd-tmpfiles-clean.timer`：按 `pvz-artifacts.conf` 清理过期归档。

常用检查：

```bash
systemctl list-timers 'pvz-*'
systemctl status pvz-github-mirror.service pvz-git-integrity.service pvz-server-health-report.service
cat /srv/codex/health/latest.txt
git --git-dir=/srv/git/PlantsVsZombies.git show-ref
```

灾难恢复裸仓库：

```bash
git clone ssh://git@<server-address>/srv/git/PlantsVsZombies.git
```
