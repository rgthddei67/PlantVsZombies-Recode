# 设置控制台输出编码为 UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$PSDefaultParameterValues['*:Encoding'] = 'utf8'

# 检查外网连通性（超时 5 秒）
try {
        Invoke-WebRequest -Uri "www.discord.com" -TimeoutSec 5 -UseBasicParsing -ErrorAction Stop | Out-Null
}
catch {
    Write-Host "[错误] 无法连接外网，请先开启 VPN 再运行 Claude Code！" -ForegroundColor Red
    Read-Host "按 Enter 键退出"
    exit 1
}

# 提示用户选择模式
Write-Host "是否启动 Claude Code？ 注意: 请使用 Taiwan-1 节点! 观察WiFi图片是否变成网卡的图标（必须先确认！）"
$choice = Read-Host "Press 1 to start, Press 2 to start with dangerous mode. Other to stop"

switch ($choice) {
    "1" {
        Clear-Host
        claude
    }
    "2" {
        Clear-Host
        claude --dangerously-skip-permissions
    }
    default {
        Write-Host "已取消启动。"
        Read-Host "按 Enter 键退出"
        exit 0
    }
}

# claude 退出后，脚本暂停等待用户操作
Read-Host "`nClaude 已退出，按 Enter 键关闭窗口"