@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

curl --max-time 5 -s www.discord.com > nul 2>&1
if errorlevel 1 (
    echo [错误] 无法连接外网，请先开启 VPN 再运行 Claude Code！
    pause
    exit /b 1
)

echo VPN 已连通，是否启动 Claude Code？ 注意: 请使用 Taiwan-1 节点! 观察WiFi图片是否变成网卡的图标
set /p confirm="Press 1 to start, other to stop: "
if "%confirm%"=="1" (
    cls
    claude
) else (
    echo 已取消启动。
)

pause