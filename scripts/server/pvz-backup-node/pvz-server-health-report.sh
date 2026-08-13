#!/usr/bin/env bash
set -euo pipefail

# 生成可离线读取的单机健康快照；只报告状态，不自动重启或修改任何服务。
report_directory=/srv/codex/health
install -d -m 0750 -o codex -g codex "$report_directory"
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
temporary_report=$(mktemp "$report_directory/.health-${timestamp}.XXXXXX")
final_report="$report_directory/${timestamp}.txt"
trap 'rm -f -- "$temporary_report"' EXIT

ssh_state=$(systemctl is-active ssh.service 2>/dev/null || true)
hysteria_state=$(systemctl is-active hysteria-server.service 2>/dev/null || true)
ufw_state=$(ufw status 2>/dev/null | sed -n '1p' || true)
disk_percent=$(df --output=pcent / | tail -n 1 | tr -dc '0-9')
memory_available_kib=$(awk '/^MemAvailable:/ { print $2 }' /proc/meminfo)
pending_summary=$(apt-get -s -o Debug::NoLocking=1 full-upgrade 2>/dev/null | grep -E '^[0-9]+ upgraded' | tail -n 1 || true)
failed_count=$(systemctl --failed --no-legend 2>/dev/null | grep -c . || true)

warnings=()
[[ "$ssh_state" == active ]] || warnings+=("ssh.service is $ssh_state")
[[ "$hysteria_state" == active ]] || warnings+=("hysteria-server.service is $hysteria_state")
[[ "$ufw_state" == 'Status: active' ]] || warnings+=("UFW is not active")
(( disk_percent < 85 )) || warnings+=("root filesystem usage is ${disk_percent}%")
(( memory_available_kib >= 131072 )) || warnings+=("available memory is below 128 MiB")
(( failed_count == 0 )) || warnings+=("systemd has ${failed_count} failed unit(s)")

overall_status=OK
if (( ${#warnings[@]} > 0 )); then
    overall_status=WARN
fi

{
    printf 'status=%s\n' "$overall_status"
    printf 'generated_at_utc=%s\n' "$(date -u --iso-8601=seconds)"
    printf 'hostname=%s\n' "$(hostname)"
    printf 'kernel=%s\n' "$(uname -r)"
    printf 'uptime=%s\n' "$(uptime -p)"
    printf 'ssh=%s\n' "$ssh_state"
    printf 'hysteria=%s\n' "$hysteria_state"
    printf 'ufw=%s\n' "$ufw_state"
    printf 'root_disk_percent=%s\n' "$disk_percent"
    printf 'memory_available_kib=%s\n' "$memory_available_kib"
    printf 'pending_upgrades=%s\n' "${pending_summary:-unknown}"
    printf 'failed_units=%s\n' "$failed_count"
    printf 'github_mirror_result=%s\n' "$(systemctl show pvz-github-mirror.service -p Result --value 2>/dev/null || true)"
    printf 'git_integrity_result=%s\n' "$(systemctl show pvz-git-integrity.service -p Result --value 2>/dev/null || true)"

    printf '\n[warnings]\n'
    if (( ${#warnings[@]} == 0 )); then
        printf 'none\n'
    else
        printf '%s\n' "${warnings[@]}"
    fi

    printf '\n[resources]\n'
    free -h
    df -h /

    printf '\n[listeners]\n'
    ss -lntup

    printf '\n[failed-units]\n'
    systemctl --failed --no-pager || true

    printf '\n[timers]\n'
    systemctl list-timers pvz-github-mirror.timer pvz-server-health-report.timer pvz-git-integrity.timer --no-pager || true
} > "$temporary_report"

chmod 0640 "$temporary_report"
chown codex:codex "$temporary_report"
mv -f -- "$temporary_report" "$final_report"
ln -sfn -- "$(basename "$final_report")" "$report_directory/latest.txt"
trap - EXIT
printf '%s\n' "$final_report"
