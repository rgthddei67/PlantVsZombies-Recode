param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Convert-HsvToRgb {
    param(
        [double]$Hue,
        [double]$Saturation,
        [double]$Value
    )

    $chroma = $Value * $Saturation
    $sector = $Hue / 60.0
    $x = $chroma * (1.0 - [Math]::Abs(($sector % 2.0) - 1.0))
    $r1 = 0.0
    $g1 = 0.0
    $b1 = 0.0

    if ($sector -lt 1.0) { $r1 = $chroma; $g1 = $x }
    elseif ($sector -lt 2.0) { $r1 = $x; $g1 = $chroma }
    elseif ($sector -lt 3.0) { $g1 = $chroma; $b1 = $x }
    elseif ($sector -lt 4.0) { $g1 = $x; $b1 = $chroma }
    elseif ($sector -lt 5.0) { $r1 = $x; $b1 = $chroma }
    else { $r1 = $chroma; $b1 = $x }

    $m = $Value - $chroma
    return @(
        [Math]::Min(255, [Math]::Max(0, [int][Math]::Round(($r1 + $m) * 255.0))),
        [Math]::Min(255, [Math]::Max(0, [int][Math]::Round(($g1 + $m) * 255.0))),
        [Math]::Min(255, [Math]::Max(0, [int][Math]::Round(($b1 + $m) * 255.0)))
    )
}

function Convert-RedMaterialToPink {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $source = [System.Drawing.Bitmap]::new($SourcePath)
    $result = [System.Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

    try {
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $color = $source.GetPixel($x, $y)
                if ($color.A -eq 0) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $red = $color.R / 255.0
                $green = $color.G / 255.0
                $blue = $color.B / 255.0
                $max = [Math]::Max($red, [Math]::Max($green, $blue))
                $min = [Math]::Min($red, [Math]::Min($green, $blue))
                $delta = $max - $min
                $saturation = if ($max -gt 0.0) { $delta / $max } else { 0.0 }

                # Select red material only; keep white stripes, green skin, metal and outlines unchanged.
                if ($red -lt 0.18) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($green * 1.12)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($blue * 1.08)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($saturation -lt 0.22) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $pinkSaturation = [Math]::Min(0.84, [Math]::Max(0.48, $saturation * 0.90))
                $pinkValue = [Math]::Min(1.0, [Math]::Max(0.0, $max * 1.06 + 0.015))
                $rgb = Convert-HsvToRgb -Hue 330.0 -Saturation $pinkSaturation -Value $pinkValue
                $result.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $color.A, $rgb[0], $rgb[1], $rgb[2]))
            }
        }

        $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $result.Dispose()
        $source.Dispose()
    }
}

foreach ($preset in @('clang-release', 'msvc-debug')) {
    $resourceRoot = Join-Path $RepositoryRoot "build\$preset\resources"
    $imageDirectory = Join-Path $resourceRoot 'image\reanim'
    $sourceReanim = Join-Path $resourceRoot 'reanim\FootballZombie.reanim'
    $targetReanim = Join-Path $resourceRoot 'reanim\PinkFootballZombie.reanim'

    Get-ChildItem -LiteralPath $imageDirectory -Filter '*football*.png' |
        Where-Object { $_.Name -notmatch '_glow\.png$' -and $_.Name -notmatch 'pinkfootball' } |
        ForEach-Object {
            $targetName = [regex]::Replace(
                $_.Name, 'football', 'pinkfootball',
                [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            Convert-RedMaterialToPink -SourcePath $_.FullName `
                -DestinationPath (Join-Path $imageDirectory $targetName)
        }

    $reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
    $reanimText = $reanimText.Replace('IMAGE_REANIM_ZOMBIE_FOOTBALL_',
        'IMAGE_REANIM_ZOMBIE_PINKFOOTBALL_')
    [System.IO.File]::WriteAllText($targetReanim, $reanimText,
        [System.Text.UTF8Encoding]::new($false))
}
