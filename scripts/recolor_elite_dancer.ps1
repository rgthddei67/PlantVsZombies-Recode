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

function Convert-RedMaterialToPurple {
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

                # Select red cloth only; preserve gloves, skin, shoes, outlines and translucent edges.
                if ($red -le 0.0) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($green * 1.05)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($blue * 1.03)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($saturation -lt 0.12) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                # Preserve source luminance and alpha so the result keeps the original cloth shading.
                $purpleSaturation = [Math]::Min(0.90, [Math]::Max(0.54, $saturation * 0.96))
                $purpleValue = [Math]::Min(1.0, [Math]::Max(0.0, $max * 0.97 + 0.01))
                $rgb = Convert-HsvToRgb -Hue 282.0 -Saturation $purpleSaturation -Value $purpleValue
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

$suitParts = @(
    'body1', 'body2', 'colar',
    'innerarm_lower', 'innerarm_upper', 'outerarm_lower', 'outerarm_upper',
    'innerleg_lower', 'innerleg_upper', 'outerleg_lower', 'outerleg_upper'
)

foreach ($preset in @('clang-release', 'msvc-debug')) {
    $resourceRoot = Join-Path $RepositoryRoot "build\$preset\resources"
    $imageDirectory = Join-Path $resourceRoot 'image\reanim'

    foreach ($part in $suitParts) {
        Convert-RedMaterialToPurple -SourcePath (Join-Path $imageDirectory "Zombie_Jackson_$part.png") `
            -DestinationPath (Join-Path $imageDirectory "Zombie_EliteJackson_$part.png")
    }

    $sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_Jackson.reanim'
    $targetReanim = Join-Path $resourceRoot 'reanim\Zombie_EliteJackson.reanim'
    $reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
    foreach ($part in $suitParts) {
        $suffix = $part.ToUpperInvariant()
        $reanimText = $reanimText.Replace(
            "IMAGE_REANIM_ZOMBIE_JACKSON_$suffix",
            "IMAGE_REANIM_ZOMBIE_ELITEJACKSON_$suffix")
    }
    [System.IO.File]::WriteAllText($targetReanim, $reanimText,
        [System.Text.UTF8Encoding]::new($false))
}
