param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$woodSuffixes = @('BODY', 'BODY_OVERLAY1', 'BODY_OVERLAY2')
$greenSuffixes = @('SIDING')
$steelSuffixes = @('BASKET', 'BASKET_OVERLAY', 'MANHOLE', 'MANHOLE_OVERLAY')

# 生成结果用 SHA-256 锁定，防 System.Drawing 或筛色阈值漂移静默改变资源。
Set-Variable -Name expectedHashes -Value (@{ bootstrap = 'bootstrap' }) -Scope Script
$expectedHashes.Remove('bootstrap')
$expectedHashes['image/reanim/Zombie_elite_catapult_body.png'] = 'aa6187b7791d663eaac676e427ea7b59d407f4d606e0007b55d122ed049e6c7d'
$expectedHashes['image/reanim/Zombie_elite_catapult_body_overlay1.png'] = '828a1cc20e2e4e40d3bc9df67e9e09f4565f5a0a4f01f2ae54b1a7b10c64be18'
$expectedHashes['image/reanim/Zombie_elite_catapult_body_overlay2.png'] = '06d7cf566e11f8c9f784b7a52ab7d7230d1f58352be6e4780b87bb40e3226cd0'
$expectedHashes['image/reanim/Zombie_elite_catapult_siding.png'] = 'feb645f684da8e761f512f94306df8e3c0bbeed35afdf1b701b8f7b6a3cbdd44'
$expectedHashes['image/reanim/Zombie_elite_catapult_basket.png'] = '06c5f94a49e5b016b58e8dc4d605d6fb2c77973151eda10abb5227b7f25a4733'
$expectedHashes['image/reanim/Zombie_elite_catapult_basket_overlay.png'] = 'ca2c15097a7574688e08d53dda7a0f3cd9527a8843ae5af48947ae25481ae86f'
$expectedHashes['image/reanim/Zombie_elite_catapult_manhole.png'] = '0f25c2a8b940512cabf1a0a132e4ef60a5596869127f6925ea5360ec12ff71cc'
$expectedHashes['image/reanim/Zombie_elite_catapult_manhole_overlay.png'] = '95273c3c38c7719f7132e314be1b6473695e3c7e655846d6a25312d69b5f368c'
$expectedHashes['image/reanim/Zombie_elite_catapult_siding_damage.png'] = 'da0511db38049580672e4e106c2e455c014adf2a2d78a5c1d6898d61a4b73b38'
$expectedHashes['reanim/EliteCatapult.reanim'] = '8584b38b83458e3107447788384e5117947404f4b681928fa5becd8c1f889867'
$expectedHashes['particles/config/EliteCatapultExplosion.xml'] = 'b4ead1018e65081561ad5fb392904c6d30b1f806343692e4f19e0f8608bc4127'

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

function Convert-SelectiveHue {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][ValidateSet('Wood', 'Green', 'Steel')][string]$Mode
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
                $value = [Math]::Max($red, [Math]::Max($green, $blue))
                $minimum = [Math]::Min($red, [Math]::Min($green, $blue))
                $saturation = if ($value -gt 0.0) { ($value - $minimum) / $value } else { 0.0 }
                $matches = switch ($Mode) {
                    'Wood' { $value -ge 0.10 -and $red -ge ($green * 1.08) -and $green -ge ($blue * 1.02) }
                    'Green' { $value -ge 0.08 -and $green -ge ($red * 1.15) -and $green -ge ($blue * 1.12) }
                    'Steel' { $value -ge 0.10 -and $saturation -le 0.32 }
                }
                if (-not $matches) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                $targetHue = switch ($Mode) {
                    'Wood' { 188.0 }
                    'Green' { 179.0 }
                    'Steel' { 202.0 }
                }
                $targetSaturation = switch ($Mode) {
                    'Wood' { [Math]::Min(0.76, [Math]::Max(0.48, $saturation)) }
                    'Green' { [Math]::Min(0.92, [Math]::Max(0.68, $saturation)) }
                    'Steel' { [Math]::Min(0.62, [Math]::Max(0.38, $saturation + 0.32)) }
                }
                $rgb = Convert-HsvToRgb -Hue $targetHue -Saturation $targetSaturation -Value $value
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

$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$imageDirectory = Join-Path $resourceRoot 'image\reanim'
$generatedFiles = [System.Collections.Generic.List[string]]::new()

foreach ($suffix in $woodSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_catapult_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_catapult_$($suffix.ToLowerInvariant()).png"
    Convert-SelectiveHue -SourcePath $sourcePath -DestinationPath $targetPath -Mode Wood
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $greenSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_catapult_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_catapult_$($suffix.ToLowerInvariant()).png"
    Convert-SelectiveHue -SourcePath $sourcePath -DestinationPath $targetPath -Mode Green
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $steelSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_catapult_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_catapult_$($suffix.ToLowerInvariant()).png"
    Convert-SelectiveHue -SourcePath $sourcePath -DestinationPath $targetPath -Mode Steel
    $generatedFiles.Add($targetPath)
}

$damagedSiding = Join-Path $imageDirectory 'Zombie_elite_catapult_siding_damage.png'
Convert-SelectiveHue `
    -SourcePath (Join-Path $imageDirectory 'Zombie_catapult_siding_damage.png') `
    -DestinationPath $damagedSiding `
    -Mode Green
$generatedFiles.Add($damagedSiding)

$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_catapult.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\EliteCatapult.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in ($woodSuffixes + $greenSuffixes + $steelSuffixes)) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_CATAPULT_$suffix",
        "IMAGE_REANIM_ZOMBIE_ELITE_CATAPULT_$suffix")
}
# BASKET 是 BASKETBALL 的前缀；篮球没有重配色，必须显式恢复共享普通键。
$reanimText = $reanimText.Replace(
    'IMAGE_REANIM_ZOMBIE_ELITE_CATAPULT_BASKETBALL',
    'IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL')
[System.IO.File]::WriteAllText(
    $targetReanim,
    $reanimText,
    [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetReanim)

$sourceParticle = Join-Path $resourceRoot 'particles\config\CatapultExplosion.xml'
$targetParticle = Join-Path $resourceRoot 'particles\config\EliteCatapultExplosion.xml'
$particleText = [System.IO.File]::ReadAllText($sourceParticle, [System.Text.Encoding]::UTF8)
$particleText = $particleText.Replace(
    '<Name>CatapultExplosion</Name>',
    '<Name>EliteCatapultExplosion</Name>')
$particleText = $particleText.Replace(
    'IMAGE_REANIM_ZOMBIE_CATAPULT_MANHOLE',
    'IMAGE_ZOMBIE_ELITE_CATAPULT_MANHOLE')
[System.IO.File]::WriteAllText(
    $targetParticle,
    $particleText,
    [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetParticle)

foreach ($path in $generatedFiles) {
    $relativePath = $path.Substring($resourceRoot.Length + 1).Replace('\', '/')
    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($expectedHashes.Count -gt 0) {
        if (-not $expectedHashes.ContainsKey($relativePath)) {
            throw "Missing expected SHA-256 for $relativePath."
        }
        if ($actualHash -ne $expectedHashes[$relativePath]) {
            throw "SHA-256 mismatch for $relativePath. Expected $($expectedHashes[$relativePath]), got $actualHash."
        }
    }
    else {
        Write-Output "`$expectedHashes['$relativePath'] = '$actualHash'"
    }
}
if ($expectedHashes.Count -gt 0 -and $expectedHashes.Count -ne $generatedFiles.Count) {
    throw "Expected hash count $($expectedHashes.Count) does not match generated file count $($generatedFiles.Count)."
}

Write-Output "Generated $($generatedFiles.Count) elite catapult resources."
