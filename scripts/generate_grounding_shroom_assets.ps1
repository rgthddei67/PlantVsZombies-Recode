param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sourcePath = Join-Path $RepositoryRoot 'scripts\assets\grounding_shroom_source_alpha.png'
$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$reanimDirectory = Join-Path $resourceRoot 'image\reanim'
$cardDirectory = Join-Path $resourceRoot 'image\PlantImage'
$battleVisibleHeight = 86 # 战场整株由 108px 等比缩至约 0.8，画布与视觉锚点保持不变。

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing chroma-keyed source: $sourcePath"
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
        throw 'Grounding Shroom source pose has no visible pixels.'
    }
    return [System.Drawing.Rectangle]::FromLTRB($left, $top, $right + 1, $bottom + 1)
}

function Set-ShockArcPalette {
    param([Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap)

    # 只包围震击姿势的六条灰白电弧；最终 112×120 像素坐标用于避免误染本体。
    $arcRegions = @(
        [System.Drawing.Rectangle]::new(25, 28, 14, 24),
        [System.Drawing.Rectangle]::new(76, 28, 14, 24),
        [System.Drawing.Rectangle]::new(22, 75, 16, 26),
        [System.Drawing.Rectangle]::new(76, 75, 17, 26),
        [System.Drawing.Rectangle]::new(35, 80, 16, 38),
        [System.Drawing.Rectangle]::new(64, 80, 17, 38)
    )

    foreach ($region in $arcRegions) {
        for ($y = $region.Top; $y -lt $region.Bottom; $y++) {
            for ($x = $region.Left; $x -lt $region.Right; $x++) {
                $color = $Bitmap.GetPixel($x, $y)
                if ($color.A -le 8) { continue }

                $maximum = [Math]::Max($color.R, [Math]::Max($color.G, $color.B))
                $minimum = [Math]::Min($color.R, [Math]::Min($color.G, $color.B))
                $brightness = ($color.R + $color.G + $color.B) / 3.0
                if (($maximum - $minimum) -gt 28 -or $brightness -lt 24) { continue }

                # 保留原抗锯齿明暗，暗边映射为紫罗兰，亮芯映射为浅薰衣草色。
                $factor = [Math]::Min(1.0, $brightness / 255.0)
                $red = [int][Math]::Round(82 + 164 * $factor)
                $green = [int][Math]::Round(18 + 172 * $factor)
                $blue = [int][Math]::Round(170 + 82 * $factor)
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($color.A, $red, $green, $blue))
            }
        }
    }
}

function Export-Pose {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Sheet,
        [Parameter(Mandatory = $true)][System.Drawing.Rectangle]$Quadrant,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [int]$CanvasWidth = 112,
        [int]$CanvasHeight = 120,
        [int]$VisibleHeight = 108,
        [switch]$CenterVertically,
        [switch]$TintShockArcs
    )

    $pose = $Sheet.Clone($Quadrant, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $bounds = Get-VisibleBounds -Bitmap $pose
        $cropped = $pose.Clone($bounds, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $scale = [Math]::Min(
                ($CanvasWidth - 4) / [double]$cropped.Width,
                $VisibleHeight / [double]$cropped.Height)
            $width = [Math]::Max(1, [int][Math]::Round($cropped.Width * $scale))
            $height = [Math]::Max(1, [int][Math]::Round($cropped.Height * $scale))
            $left = [int][Math]::Round(($CanvasWidth - $width) / 2.0)
            $top = if ($CenterVertically) {
                [int][Math]::Round(($CanvasHeight - $height) / 2.0)
            }
            else {
                $CanvasHeight - $height - 3
            }

            # 先压到原版植物的低分辨率尺度，再由 reanim 做小幅仿射呼吸。
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
                if ($TintShockArcs) {
                    Set-ShockArcPalette -Bitmap $result
                }
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
    finally {
        $pose.Dispose()
    }
}

$sheet = [System.Drawing.Bitmap]::new($sourcePath)
try {
    $halfWidth = [int]($sheet.Width / 2)
    $halfHeight = [int]($sheet.Height / 2)
    $poses = @(
        @{ Name = 'GroundingShroom_idle.png'; Rect = [System.Drawing.Rectangle]::new(0, 0, $halfWidth, $halfHeight) },
        @{ Name = 'GroundingShroom_sleep.png'; Rect = [System.Drawing.Rectangle]::new($halfWidth, 0, $sheet.Width - $halfWidth, $halfHeight) },
        @{ Name = 'GroundingShroom_shock.png'; Rect = [System.Drawing.Rectangle]::new(0, $halfHeight, $halfWidth, $sheet.Height - $halfHeight) },
        @{ Name = 'GroundingShroom_recover.png'; Rect = [System.Drawing.Rectangle]::new($halfWidth, $halfHeight, $sheet.Width - $halfWidth, $sheet.Height - $halfHeight) }
    )
    foreach ($pose in $poses) {
        $tintShockArcs = $pose.Name -eq 'GroundingShroom_shock.png'
        Export-Pose -Sheet $sheet -Quadrant $pose.Rect `
            -DestinationPath (Join-Path $reanimDirectory $pose.Name) `
            -VisibleHeight $battleVisibleHeight -TintShockArcs:$tintShockArcs
    }

    Export-Pose -Sheet $sheet -Quadrant $poses[0].Rect `
        -DestinationPath (Join-Path $cardDirectory 'GroundingShroom.png') `
        -CanvasWidth 120 -CanvasHeight 120 -VisibleHeight 78 -CenterVertically
}
finally {
    $sheet.Dispose()
}

$outputs = @(
    'image/reanim/GroundingShroom_idle.png',
    'image/reanim/GroundingShroom_sleep.png',
    'image/reanim/GroundingShroom_shock.png',
    'image/reanim/GroundingShroom_recover.png',
    'image/PlantImage/GroundingShroom.png'
)
foreach ($relativePath in $outputs) {
    $path = Join-Path $resourceRoot ($relativePath -replace '/', '\')
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Write-Output "$relativePath $hash"
}
