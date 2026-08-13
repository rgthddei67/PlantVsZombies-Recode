param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sourcePath = Join-Path $RepositoryRoot 'scripts\assets\lightning_rod_pot_source_alpha.png'
$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$reanimDirectory = Join-Path $resourceRoot 'image\reanim'
$cardDirectory = Join-Path $resourceRoot 'image\PlantImage'

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing Lightning Rod Pot source: $sourcePath"
}

[System.IO.Directory]::CreateDirectory($reanimDirectory) | Out-Null
[System.IO.Directory]::CreateDirectory($cardDirectory) | Out-Null

function Get-VisibleBounds {
    param([Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap)

    $left = $Bitmap.Width
    $top = $Bitmap.Height
    $right = -1
    $bottom = -1
    for ($y = 0; $y -lt $Bitmap.Height; $y++) {
        for ($x = 0; $x -lt $Bitmap.Width; $x++) {
            if ($Bitmap.GetPixel($x, $y).A -le 8) { continue }
            $left = [Math]::Min($left, $x)
            $top = [Math]::Min($top, $y)
            $right = [Math]::Max($right, $x)
            $bottom = [Math]::Max($bottom, $y)
        }
    }
    if ($right -lt $left -or $bottom -lt $top) {
        throw 'Lightning Rod Pot source has no visible pixels.'
    }
    return [System.Drawing.Rectangle]::FromLTRB($left, $top, $right + 1, $bottom + 1)
}

function Set-LightningPalette {
    param([Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap)

    # imagegen 的品红去底会把相近的紫色电芯拉成灰白；只在避雷针电弧与电芯闪电区域重建紫罗兰明暗。
    $regions = @(
        [System.Drawing.Rectangle]::new(820, 190, 215, 360),
        [System.Drawing.Rectangle]::new(890, 570, 125, 205)
    )
    foreach ($region in $regions) {
        $right = [Math]::Min($Bitmap.Width, $region.Right)
        $bottom = [Math]::Min($Bitmap.Height, $region.Bottom)
        for ($y = [Math]::Max(0, $region.Top); $y -lt $bottom; $y++) {
            for ($x = [Math]::Max(0, $region.Left); $x -lt $right; $x++) {
                $color = $Bitmap.GetPixel($x, $y)
                if ($color.A -le 8) { continue }
                $maximum = [Math]::Max($color.R, [Math]::Max($color.G, $color.B))
                $minimum = [Math]::Min($color.R, [Math]::Min($color.G, $color.B))
                $brightness = ($color.R + $color.G + $color.B) / 3.0
                if (($maximum - $minimum) -gt 42 -or $brightness -lt 74) { continue }

                $factor = [Math]::Min(1.0, $brightness / 255.0)
                $red = [int][Math]::Round(72 + 172 * $factor)
                $green = [int][Math]::Round(21 + 166 * $factor)
                $blue = [int][Math]::Round(155 + 98 * $factor)
                $Bitmap.SetPixel($x, $y,
                    [System.Drawing.Color]::FromArgb($color.A, $red, $green, $blue))
            }
        }
    }
}

function Export-Sprite {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Source,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [int]$CanvasWidth,
        [int]$CanvasHeight,
        [int]$MaximumVisibleWidth,
        [int]$MaximumVisibleHeight,
        [int]$VerticalOffsetY = 0,
        [switch]$CenterVertically
    )

    $bounds = Get-VisibleBounds -Bitmap $Source
    $cropped = $Source.Clone($bounds, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $scale = [Math]::Min(
            $MaximumVisibleWidth / [double]$cropped.Width,
            $MaximumVisibleHeight / [double]$cropped.Height)
        $width = [Math]::Max(1, [int][Math]::Round($cropped.Width * $scale))
        $height = [Math]::Max(1, [int][Math]::Round($cropped.Height * $scale))
        $left = [int][Math]::Round(($CanvasWidth - $width) / 2.0)
        $top = if ($CenterVertically) {
            [int][Math]::Round(($CanvasHeight - $height) / 2.0) + $VerticalOffsetY
        }
        else {
            $CanvasHeight - $height - 2
        }

        $result = [System.Drawing.Bitmap]::new(
            $CanvasWidth, $CanvasHeight,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($result)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($cropped,
                [System.Drawing.Rectangle]::new($left, $top, $width, $height),
                0, 0, $cropped.Width, $cropped.Height,
                [System.Drawing.GraphicsUnit]::Pixel)
            $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $graphics.Dispose()
            $result.Dispose()
        }
    }
    finally {
        $cropped.Dispose()
    }
}

function Export-DischargeGlow {
    param([Parameter(Mandatory = $true)][string]$DestinationPath)

    # 单独的低分辨率光层只负责反应动画；复杂陶盆和金属结构始终来自 imagegen 本体。
    $result = [System.Drawing.Bitmap]::new(
        128, 180, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $outer = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(185, 83, 24, 180), 3.5)
        $inner = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(245, 231, 190, 255), 1.3)
        try {
            $leftArc = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(91, 72), [System.Drawing.PointF]::new(85, 79),
                [System.Drawing.PointF]::new(90, 86), [System.Drawing.PointF]::new(84, 94),
                [System.Drawing.PointF]::new(90, 102), [System.Drawing.PointF]::new(86, 110))
            $rightArc = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(102, 77), [System.Drawing.PointF]::new(108, 84),
                [System.Drawing.PointF]::new(103, 91), [System.Drawing.PointF]::new(109, 98),
                [System.Drawing.PointF]::new(104, 105), [System.Drawing.PointF]::new(108, 112))
            $graphics.DrawLines($outer, $leftArc)
            $graphics.DrawLines($outer, $rightArc)
            $graphics.DrawLines($inner, $leftArc)
            $graphics.DrawLines($inner, $rightArc)
        }
        finally {
            $outer.Dispose()
            $inner.Dispose()
        }
        $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $result.Dispose()
    }
}

$source = [System.Drawing.Bitmap]::new($sourcePath)
try {
    Set-LightningPalette -Bitmap $source
    Export-Sprite -Source $source `
        -DestinationPath (Join-Path $reanimDirectory 'LightningRodPot_body.png') `
        -CanvasWidth 128 -CanvasHeight 180 -MaximumVisibleWidth 86 -MaximumVisibleHeight 130
    Export-Sprite -Source $source `
        -DestinationPath (Join-Path $cardDirectory 'LightningRodPot.png') `
        -CanvasWidth 120 -CanvasHeight 120 -MaximumVisibleWidth 82 -MaximumVisibleHeight 96 `
        -VerticalOffsetY -6 -CenterVertically
    Export-DischargeGlow -DestinationPath (Join-Path $reanimDirectory 'LightningRodPot_glow.png')
}
finally {
    $source.Dispose()
}

$outputs = @(
    'image/reanim/LightningRodPot_body.png',
    'image/reanim/LightningRodPot_glow.png',
    'image/PlantImage/LightningRodPot.png'
)
foreach ($relativePath in $outputs) {
    $path = Join-Path $resourceRoot ($relativePath -replace '/', '\')
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Write-Output "$relativePath $hash"
}
