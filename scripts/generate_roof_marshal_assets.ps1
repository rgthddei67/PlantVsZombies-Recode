param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# 生成结果用 SHA-256 锁定，防 System.Drawing 或筛色阈值漂移静默改变资源。
Set-Variable -Name expectedHashes -Value (@{ bootstrap = 'bootstrap' }) -Scope Script
$expectedHashes.Remove('bootstrap')
$expectedHashes['image/reanim/Zombie_roofmarshal_body.png'] = 'c3329907c88aab280b763166b5a3db84b18ccca6f20fad1af30d90e988483a29'
$expectedHashes['image/reanim/Zombie_roofmarshal_innerarm_lower.png'] = '53698dfdef0124f1aea388c0445e72e65f31246c751d19f499e3a6100e1d1dba'
$expectedHashes['image/reanim/Zombie_roofmarshal_innerarm_upper.png'] = 'af1d08e694180a3facff960ab73e00f6894dbebac387ff88a104c8b5b11a202b'
$expectedHashes['image/reanim/Zombie_roofmarshal_outerarm_lower.png'] = '72751aa10ff791db41d0d42989098fcfbe1fda8e9ff29860627ff7bec3320b3f'
$expectedHashes['image/reanim/Zombie_roofmarshal_outerarm_upper.png'] = 'fba640455ad8e97aa64be596b03527f95db9dde1a1aab30b6357f3a822b0cad4'
$expectedHashes['image/reanim/Zombie_roofmarshal_outerarm_upper2.png'] = 'd04af2828818f2e748e1563c7f10969463ce21120ba94c8c3ce638b098314270'
$expectedHashes['image/reanim/Zombie_roofmarshal_innerleg_foot.png'] = '5059398012c91936b02bf1028155e2b05a51ab378df8814b31c9d5906af6eb57'
$expectedHashes['image/reanim/Zombie_roofmarshal_outerleg_foot.png'] = '71b07b315ba0741e3e9fe5c52193da4a52b6e26253f15d70a073ab9b818efccb'
$expectedHashes['image/reanim/Zombie_roofmarshal_tie.png'] = '0c8d887363fb1af64730c02d64a37223f10125c4fef6ac2271506c7a0f3dcccb'
$expectedHashes['image/reanim/Zombie_roofmarshal_hat.png'] = '8353273413681579a745773cfe6f43b5503e3121bcb992981ce3070f8df8b9e6'
$expectedHashes['particles/ZombieRoofMarshalHead.png'] = '46e57bf8be0403c7a26b53371916d6fca64a901cf0c1d5baba86828524929032'
$expectedHashes['reanim/RoofMarshal.reanim'] = 'de944ef1484011fefeff51e4bf5084e3085e546523c1b3086ff0ad60b435a55f'

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

function Convert-UniformPart {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][ValidateSet('Navy', 'Gold', 'Boot')][string]$Mode
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
                    'Navy' {
                        $value -ge 0.07 -and $red -ge ($green * 1.12) `
                            -and $green -ge ($blue * 1.08) -and $saturation -ge 0.18
                    }
                    'Gold' {
                        $value -ge 0.06 -and $red -ge ($green * 1.15) `
                            -and $red -ge ($blue * 1.35) -and $saturation -ge 0.28
                    }
                    'Boot' { $value -ge 0.05 -and $saturation -ge 0.12 }
                }
                if (-not $matches) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $targetHue = switch ($Mode) {
                    'Navy' { 220.0 }
                    'Gold' { 42.0 }
                    'Boot' { 218.0 }
                }
                $targetSaturation = switch ($Mode) {
                    'Navy' { [Math]::Min(0.72, [Math]::Max(0.48, $saturation)) }
                    'Gold' { [Math]::Min(0.92, [Math]::Max(0.70, $saturation)) }
                    'Boot' { 0.22 }
                }
                $targetValue = switch ($Mode) {
                    'Navy' { [Math]::Min(0.72, [Math]::Max(0.08, $value * 0.84 + 0.02)) }
                    'Gold' { [Math]::Min(0.88, [Math]::Max(0.10, $value * 1.04)) }
                    'Boot' { [Math]::Min(0.34, [Math]::Max(0.04, $value * 0.46)) }
                }
                $rgb = Convert-HsvToRgb -Hue $targetHue -Saturation $targetSaturation -Value $targetValue
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

function New-RoofMarshalHat {
    param([Parameter(Mandatory = $true)][string]$DestinationPath)

    # 帽子沿用 Zombie_hair 的 64x31 画布和原锚点，避免普通时间线在转头时产生跳动。
    $result = [System.Drawing.Bitmap]::new(
        64, 31, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    try {
        $outline = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 18, 18, 20), 2.0)
        $navy = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 31, 39, 61))
        $navyLight = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 53, 64, 92))
        $brim = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 24, 25, 31))
        $gold = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 184, 132, 38))
        $goldDark = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 87, 58, 13), 1.0)
        try {
            $graphics.FillEllipse($navy, 9, 1, 48, 18)
            $graphics.DrawEllipse($outline, 9, 1, 48, 18)
            $graphics.FillPolygon($navyLight, @(
                [System.Drawing.Point]::new(12, 12),
                [System.Drawing.Point]::new(54, 11),
                [System.Drawing.Point]::new(57, 17),
                [System.Drawing.Point]::new(9, 19)))
            $graphics.DrawLine($outline, 10, 18, 56, 16)
            $graphics.FillPolygon($brim, @(
                [System.Drawing.Point]::new(6, 18),
                [System.Drawing.Point]::new(49, 15),
                [System.Drawing.Point]::new(62, 19),
                [System.Drawing.Point]::new(47, 24),
                [System.Drawing.Point]::new(11, 25)))
            $graphics.DrawPolygon($outline, @(
                [System.Drawing.Point]::new(6, 18),
                [System.Drawing.Point]::new(49, 15),
                [System.Drawing.Point]::new(62, 19),
                [System.Drawing.Point]::new(47, 24),
                [System.Drawing.Point]::new(11, 25)))
            $graphics.FillEllipse($gold, 28, 5, 9, 9)
            $graphics.DrawEllipse($goldDark, 28, 5, 9, 9)
            $graphics.DrawArc($goldDark, 30, 7, 5, 4, 20, 140)
            $graphics.DrawArc($goldDark, 30, 8, 5, 4, 200, 140)
            $graphics.FillEllipse($gold, 50, 15, 4, 4)
        }
        finally {
            $outline.Dispose()
            $navy.Dispose()
            $navyLight.Dispose()
            $brim.Dispose()
            $gold.Dispose()
            $goldDark.Dispose()
        }
        $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $result.Dispose()
    }
}

function New-RoofMarshalHeadParticle {
    param(
        [Parameter(Mandatory = $true)][string]$HeadPath,
        [Parameter(Mandatory = $true)][string]$HatPath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    # 一颗粒子只接受一张纹理；把头和军帽预合成，确保抛飞与旋转期间始终保持相对位置。
    $head = [System.Drawing.Bitmap]::new($HeadPath)
    $hat = [System.Drawing.Bitmap]::new($HatPath)
    $result = [System.Drawing.Bitmap]::new(
        72, 76, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    try {
        $graphics.DrawImageUnscaled($head, 4, 15)
        $graphics.DrawImageUnscaled($hat, 4, 1)
        $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $result.Dispose()
        $hat.Dispose()
        $head.Dispose()
    }
}

$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$imageDirectory = Join-Path $resourceRoot 'image\reanim'
$generatedFiles = [System.Collections.Generic.List[string]]::new()

foreach ($part in @('body', 'innerarm_lower', 'innerarm_upper', 'outerarm_lower', 'outerarm_upper',
        'outerarm_upper2')) {
    $target = Join-Path $imageDirectory "Zombie_roofmarshal_$part.png"
    Convert-UniformPart `
        -SourcePath (Join-Path $imageDirectory "Zombie_$part.png") `
        -DestinationPath $target `
        -Mode Navy
    $generatedFiles.Add($target)
}
foreach ($part in @('innerleg_foot', 'outerleg_foot')) {
    $target = Join-Path $imageDirectory "Zombie_roofmarshal_$part.png"
    Convert-UniformPart `
        -SourcePath (Join-Path $imageDirectory "Zombie_$part.png") `
        -DestinationPath $target `
        -Mode Boot
    $generatedFiles.Add($target)
}

$tieTarget = Join-Path $imageDirectory 'Zombie_roofmarshal_tie.png'
Convert-UniformPart `
    -SourcePath (Join-Path $imageDirectory 'Zombie_tie.png') `
    -DestinationPath $tieTarget `
    -Mode Gold
$generatedFiles.Add($tieTarget)

$hatTarget = Join-Path $imageDirectory 'Zombie_roofmarshal_hat.png'
New-RoofMarshalHat -DestinationPath $hatTarget
$generatedFiles.Add($hatTarget)

$headParticleTarget = Join-Path $resourceRoot 'particles\ZombieRoofMarshalHead.png'
New-RoofMarshalHeadParticle `
    -HeadPath (Join-Path $resourceRoot 'particles\ZombieHead.png') `
    -HatPath $hatTarget `
    -DestinationPath $headParticleTarget
$generatedFiles.Add($headParticleTarget)

$sourceReanim = Join-Path $resourceRoot 'reanim\NormalZombie.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\RoofMarshal.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($part in @('BODY', 'INNERARM_LOWER', 'INNERARM_UPPER', 'OUTERARM_LOWER', 'OUTERARM_UPPER',
        'INNERLEG_FOOT', 'OUTERLEG_FOOT', 'TIE')) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_$part",
        "IMAGE_REANIM_ZOMBIE_ROOFMARSHAL_$part")
}
$reanimText = $reanimText.Replace(
    'IMAGE_REANIM_ZOMBIE_HAIR',
    'IMAGE_REANIM_ZOMBIE_ROOFMARSHAL_HAT')
[System.IO.File]::WriteAllText(
    $targetReanim,
    $reanimText,
    [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetReanim)

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

Write-Output "Generated $($generatedFiles.Count) roof marshal resources."
