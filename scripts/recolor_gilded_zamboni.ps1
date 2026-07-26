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

function Convert-BlueMaterialToGold {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [bool]$TintWholeVehicle = $false
    )

    $source = [System.Drawing.Bitmap]::new($SourcePath)
    $result = [System.Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

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
                $saturation = if ($max -gt 0.0) { ($max - $min) / $max } else { 0.0 }
                $hue = $color.GetHue()

                # Vehicle panels become gold as a whole; driver sprites only replace blue materials.
                $isBlueMaterial = $saturation -ge 0.10 -and $hue -ge 155.0 -and $hue -le 285.0
                if (-not $TintWholeVehicle -and -not $isBlueMaterial) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                # Preserve black outlines while giving white bodywork a light cream-gold cast.
                if ($TintWholeVehicle -and $max -lt 0.14 -and $saturation -lt 0.12) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                $minimumSaturation = if ($TintWholeVehicle -and $saturation -lt 0.12) { 0.22 } else { 0.46 }
                $goldSaturation = [Math]::Min(0.92, [Math]::Max($minimumSaturation, $saturation * 1.05))
                $goldValue = [Math]::Min(1.0, [Math]::Max(0.0, $max * 1.03 + 0.015))
                $rgb = Convert-HsvToRgb -Hue 48.0 -Saturation $goldSaturation -Value $goldValue
                $result.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $color.A, $rgb[0], $rgb[1], $rgb[2]))
        }
    }

    $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $result.Dispose()
    $source.Dispose()
}

$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$imageDirectory = Join-Path $resourceRoot 'image\reanim'
$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_zamboni.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\GildedZamboni.reanim'

Get-ChildItem -LiteralPath $imageDirectory -Filter 'Zombie_zamboni*.png' |
    ForEach-Object {
        $targetName = [regex]::Replace(
            $_.Name, '^Zombie_zamboni', 'Zombie_gilded_zamboni',
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        $tintWholeVehicle = $_.Name -match '^Zombie_zamboni_' `
            -or $_.Name -match '^Zombie_zambonidriver_beanie'
        Convert-BlueMaterialToGold -SourcePath $_.FullName `
            -DestinationPath (Join-Path $imageDirectory $targetName) `
            -TintWholeVehicle $tintWholeVehicle
    }

$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
$reanimText = $reanimText.Replace(
    'IMAGE_REANIM_ZOMBIE_ZAMBONI', 'IMAGE_REANIM_ZOMBIE_GILDED_ZAMBONI')
[System.IO.File]::WriteAllText(
    $targetReanim, $reanimText, [System.Text.UTF8Encoding]::new($false))

Convert-BlueMaterialToGold `
    -SourcePath (Join-Path $resourceRoot 'image\ice.png') `
    -DestinationPath (Join-Path $resourceRoot 'image\golden_ice.png')
Convert-BlueMaterialToGold `
    -SourcePath (Join-Path $resourceRoot 'image\ice_cap.png') `
    -DestinationPath (Join-Path $resourceRoot 'image\golden_ice_cap.png')

Get-ChildItem -LiteralPath $imageDirectory -Filter 'Zombie_gilded_zamboni*.png' |
    Sort-Object Name |
    Get-FileHash -Algorithm SHA256 |
    Select-Object Path, Hash
Get-FileHash -Algorithm SHA256 -LiteralPath @(
    $targetReanim,
    (Join-Path $resourceRoot 'image\golden_ice.png'),
    (Join-Path $resourceRoot 'image\golden_ice_cap.png')
) | Select-Object Path, Hash
