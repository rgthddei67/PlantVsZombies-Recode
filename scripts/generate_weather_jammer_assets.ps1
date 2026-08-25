param(
    [string]$ResourceRoot = (Join-Path $PSScriptRoot "../build/clang-release/resources")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$assetRoot = Join-Path $PSScriptRoot "assets"
$imageRoot = Join-Path $ResourceRoot "image/reanim"
$reanimRoot = Join-Path $ResourceRoot "reanim"

function New-TransparentBitmap {
    param([int]$Width, [int]$Height)
    return [System.Drawing.Bitmap]::new(
        $Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Get-AlphaBounds {
    param([System.Drawing.Bitmap]$Bitmap)

    $left = $Bitmap.Width
    $top = $Bitmap.Height
    $right = -1
    $bottom = -1
    for ($y = 0; $y -lt $Bitmap.Height; $y += 2) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += 2) {
            if ($Bitmap.GetPixel($x, $y).A -le 8) { continue }
            $left = [Math]::Min($left, $x)
            $top = [Math]::Min($top, $y)
            $right = [Math]::Max($right, $x)
            $bottom = [Math]::Max($bottom, $y)
        }
    }
    if ($right -lt $left -or $bottom -lt $top) {
        throw "源图没有可见 alpha 像素"
    }
    $padding = 4
    return [System.Drawing.Rectangle]::FromLTRB(
        [Math]::Max(0, $left - $padding),
        [Math]::Max(0, $top - $padding),
        [Math]::Min($Bitmap.Width, $right + $padding + 1),
        [Math]::Min($Bitmap.Height, $bottom + $padding + 1))
}

function Test-ReanimationTrackLengths {
    param([string]$Path)

    # Animator 会用统一播放头索引每条轨道；任一短轨道都会在片段切换后越界。
    $raw = Get-Content -Raw -LiteralPath $Path
    [xml]$xml = "<root>$raw</root>"
    $counts = @($xml.root.track | ForEach-Object { @($_.t).Count })
    if ($counts.Count -eq 0 -or $counts[0] -le 0) {
        throw "reanim 没有有效轨道: $Path"
    }
    if (($counts | Select-Object -Unique).Count -ne 1) {
        throw "reanim 轨道帧数不一致: $Path ($($counts -join ','))"
    }
}

function Test-WeatherJammerMotionContracts {
    param([string]$PackPath, [string]$TerminalPath)

    # 循环首尾必须相邻，避免播放头回绕时雷达瞬间跳回最左端。
    [xml]$packXml = "<root>$(Get-Content -Raw -LiteralPath $PackPath)</root>"
    $dishFrames = @(($packXml.root.track | Where-Object { $_.name -eq "dish" }).t)
    foreach ($start in @(0, 12)) {
        $first = [double]$dishFrames[$start].kx
        $last = [double]$dishFrames[$start + 11].kx
        if ([Math]::Abs($first - $last) -gt 2.0) {
            throw "雷达循环首尾不连续: $PackPath ($first -> $last)"
        }
    }

    # 手持终端只随稳定前臂轻动；施法片段不得再用缩放脉冲制造脱手错觉。
    [xml]$terminalXml = "<root>$(Get-Content -Raw -LiteralPath $TerminalPath)</root>"
    $terminalFrames = @(($terminalXml.root.track | Where-Object { $_.name -eq "terminal" }).t)
    foreach ($index in 12..23) {
        if ($null -ne $terminalFrames[$index].sx -or $null -ne $terminalFrames[$index].sy) {
            throw "手持终端施法片段含缩放脉冲: $TerminalPath (frame $index)"
        }
    }
}

function New-ScaledCutout {
    param(
        [string]$Path,
        [int]$Width,
        [int]$Height,
        [System.Drawing.Rectangle]$SourceRegion = [System.Drawing.Rectangle]::Empty
    )

    $source = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($SourceRegion.IsEmpty) { $SourceRegion = Get-AlphaBounds -Bitmap $source }
        $target = New-TransparentBitmap -Width $Width -Height $Height
        $graphics = [System.Drawing.Graphics]::FromImage($target)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.DrawImage($source,
                [System.Drawing.Rectangle]::new(0, 0, $Width, $Height),
                $SourceRegion.X, $SourceRegion.Y, $SourceRegion.Width, $SourceRegion.Height,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally { $graphics.Dispose() }
        return $target
    }
    finally { $source.Dispose() }
}

function New-DeviceVariant {
    param(
        [System.Drawing.Bitmap]$Base,
        [double]$Brightness,
        [System.Drawing.Color]$SignalColor,
        [string]$Destination,
        [bool]$AddSignal = $true
    )

    $target = New-TransparentBitmap -Width $Base.Width -Height $Base.Height
    $graphics = [System.Drawing.Graphics]::FromImage($target)
    try {
        $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
        try {
            $matrix = [System.Drawing.Imaging.ColorMatrix]::new()
            $matrix.Matrix00 = [single]$Brightness
            $matrix.Matrix11 = [single]$Brightness
            $matrix.Matrix22 = [single]$Brightness
            $matrix.Matrix33 = 1.0
            $matrix.Matrix44 = 1.0
            $attributes.SetColorMatrix($matrix)
            $graphics.DrawImage($Base,
                [System.Drawing.Rectangle]::new(0, 0, $Base.Width, $Base.Height),
                0, 0, $Base.Width, $Base.Height,
                [System.Drawing.GraphicsUnit]::Pixel, $attributes)
        }
        finally { $attributes.Dispose() }

        if ($AddSignal) {
            $halo = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(76, $SignalColor.R, $SignalColor.G, $SignalColor.B))
            $core = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(205, $SignalColor.R, $SignalColor.G, $SignalColor.B))
            try {
                $graphics.FillEllipse($halo, $Base.Width * 0.47, 1, $Base.Width * 0.23, $Base.Height * 0.19)
                $graphics.FillEllipse($core, $Base.Width * 0.52, 4, $Base.Width * 0.13, $Base.Height * 0.11)
            }
            finally { $halo.Dispose(); $core.Dispose() }
        }
    }
    finally { $graphics.Dispose() }
    try { $target.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png) }
    finally { $target.Dispose() }
}

$packPath = Join-Path $assetRoot "weather_jammer_pack_source.png"
$dishPath = Join-Path $assetRoot "weather_jammer_dish_source.png"
$terminalPath = Join-Path $assetRoot "weather_jammer_terminal_source.png"
foreach ($path in @($packPath, $dishPath, $terminalPath)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "缺少气象干扰僵尸源图: $path" }
}
New-Item -ItemType Directory -Force -Path $imageRoot | Out-Null

$pack = New-ScaledCutout -Path $packPath -Width 116 -Height 124
try {
    New-DeviceVariant -Base $pack -Brightness 0.94 -SignalColor ([System.Drawing.Color]::FromArgb(255, 77, 214, 232)) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_pack_ready.png")
    New-DeviceVariant -Base $pack -Brightness 1.08 -SignalColor ([System.Drawing.Color]::FromArgb(255, 70, 245, 255)) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_pack_channel.png")
    New-DeviceVariant -Base $pack -Brightness 0.78 -SignalColor ([System.Drawing.Color]::FromArgb(255, 255, 157, 35)) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_pack_reboot.png")
    New-DeviceVariant -Base $pack -Brightness 0.42 -SignalColor ([System.Drawing.Color]::Black) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_pack_spent.png") -AddSignal $false
}
finally { $pack.Dispose() }

# 雷达母图下半部带独立底座；运行时只裁出盘面、转轴和短桅杆，避免重复一台背包。
$dishRegion = [System.Drawing.Rectangle]::new(70, 0, 820, 820)
$dish = New-ScaledCutout -Path $dishPath -Width 82 -Height 82 -SourceRegion $dishRegion
try {
    $dish.Save((Join-Path $imageRoot "Zombie_weather_jammer_dish.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
}
finally { $dish.Dispose() }

$terminal = New-ScaledCutout -Path $terminalPath -Width 58 -Height 60
try {
    New-DeviceVariant -Base $terminal -Brightness 0.92 -SignalColor ([System.Drawing.Color]::Transparent) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_terminal_ready.png") -AddSignal $false
    New-DeviceVariant -Base $terminal -Brightness 1.14 -SignalColor ([System.Drawing.Color]::Transparent) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_terminal_channel.png") -AddSignal $false
    New-DeviceVariant -Base $terminal -Brightness 0.72 -SignalColor ([System.Drawing.Color]::Transparent) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_terminal_reboot.png") -AddSignal $false
    New-DeviceVariant -Base $terminal -Brightness 0.38 -SignalColor ([System.Drawing.Color]::Transparent) -Destination (Join-Path $imageRoot "Zombie_weather_jammer_terminal_spent.png") -AddSignal $false
}
finally { $terminal.Dispose() }

Test-ReanimationTrackLengths -Path (Join-Path $reanimRoot "WeatherJammerPack.reanim")
Test-ReanimationTrackLengths -Path (Join-Path $reanimRoot "WeatherJammerTerminal.reanim")
Test-WeatherJammerMotionContracts `
    -PackPath (Join-Path $reanimRoot "WeatherJammerPack.reanim") `
    -TerminalPath (Join-Path $reanimRoot "WeatherJammerTerminal.reanim")

Get-ChildItem -LiteralPath $imageRoot -Filter "Zombie_weather_jammer_*.png" |
    Sort-Object Name | Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "{0}  {1}" -f $_.Hash, $_.Path }
