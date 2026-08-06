param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$uniformSuffixes = @(
    'BODY',
    'BODY2',
    'INNERARM_UPPER',
    'OUTERARM_LOWER',
    'OUTERARM_UPPER',
    'OUTERARM_UPPER2'
)
$ladderSuffixes = @(
    '1',
    '1_DAMAGE1',
    '1_DAMAGE2',
    '2',
    '3',
    '4',
    '5'
)

# 生成结果用 SHA-256 锁定，防 System.Drawing 或筛色阈值漂移静默改变资源。
Set-Variable -Name expectedHashes -Value (@{ bootstrap = 'bootstrap' }) -Scope Script
$expectedHashes.Remove('bootstrap')
$expectedHashes['image/reanim/Zombie_elite_ladder_body.png'] = '86a2fbc99db514f7557a0b6011c7bf4477cb4c8353329e5175a6e16c5aace7c1'
$expectedHashes['image/reanim/Zombie_elite_ladder_body2.png'] = 'fc329b291a3355530ba3b3db7e2b0dcafd11a583d904a3f0f3c2665d97ba4aaf'
$expectedHashes['image/reanim/Zombie_elite_ladder_innerarm_upper.png'] = '3dc93ba633f5070f72c8460f6e598457f89ba9aabdab2596d2422f05653d8bf5'
$expectedHashes['image/reanim/Zombie_elite_ladder_outerarm_lower.png'] = 'b6545c39d3b25bc70e0664eeb0f75c32c4d6019388088647b134cfa823d4a6f6'
$expectedHashes['image/reanim/Zombie_elite_ladder_outerarm_upper.png'] = 'e0d8524e79e9d0484fc89149b0f41b8b4d4fd7c70a4132655759a91967f228e3'
$expectedHashes['image/reanim/Zombie_elite_ladder_outerarm_upper2.png'] = '9ac141217f96262769494f0973b86ece597ad5b756fe0d5314e4a32eb5a83842'
$expectedHashes['image/reanim/Zombie_elite_ladder_1.png'] = 'f4fe88fdf4d8906f0d76e6b6b56e650be58596c28a2616796df3d79d0cec7e46'
$expectedHashes['image/reanim/Zombie_elite_ladder_1_damage1.png'] = 'dea1eb3f13523dc10cf367eeb24da4a1a621e4c20120d37c47833401cd8406f9'
$expectedHashes['image/reanim/Zombie_elite_ladder_1_damage2.png'] = '3707f1b4bcda5ba8e39054d9dd24ac9f1abc10f378bbfe30e70ba5b0dc63c80d'
$expectedHashes['image/reanim/Zombie_elite_ladder_2.png'] = 'b3468f70a8b60e42f50459db55751b7460f28378aa8584c6b83df1dc558baa69'
$expectedHashes['image/reanim/Zombie_elite_ladder_3.png'] = 'bf0b56010188ae1bcdac2e9586be6961c8470f46bfda2ff9fe93562a8c656c9b'
$expectedHashes['image/reanim/Zombie_elite_ladder_4.png'] = 'bcd05103e03081e35433fbbafca73addbee31eca4e28e390a64bb4dfccf3daca'
$expectedHashes['image/reanim/Zombie_elite_ladder_5.png'] = '75fa881f9b2ab0562c6264c9457ac9a56f08d6d04fd8548f449009d327cb72fd'
$expectedHashes['reanim/EliteLadder.reanim'] = 'd02c54befeba3d0f0eabec7e4a590f8f16dd0670335fd538d1b70a32e32874a3'

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

function Convert-RedUniformToBlue {
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
                $saturation = if ($max -gt 0.0) { ($max - $min) / $max } else { 0.0 }
                $isRedUniform = $saturation -ge 0.22 -and $red -ge ($green * 1.12) -and $red -ge ($blue * 1.10)
                if ($max -lt 0.07 -or -not $isRedUniform) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                $targetSaturation = [Math]::Min(0.90, [Math]::Max(0.55, $saturation))
                $rgb = Convert-HsvToRgb -Hue 216.0 -Saturation $targetSaturation -Value $max
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

function Convert-SteelLadderToSafetyGold {
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
                $value = [Math]::Max($red, [Math]::Max($green, $blue))
                if ($value -lt 0.09) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                # 保留黑色轮廓和孔洞，把银灰明暗完整映射为安全金黄色金属。
                $targetSaturation = [Math]::Min(0.92, 0.58 + (1.0 - $value) * 0.22)
                $rgb = Convert-HsvToRgb -Hue 47.0 -Saturation $targetSaturation -Value $value
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

foreach ($suffix in $uniformSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_ladder_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_ladder_$($suffix.ToLowerInvariant()).png"
    Convert-RedUniformToBlue -SourcePath $sourcePath -DestinationPath $targetPath
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $ladderSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_ladder_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_ladder_$($suffix.ToLowerInvariant()).png"
    Convert-SteelLadderToSafetyGold -SourcePath $sourcePath -DestinationPath $targetPath
    $generatedFiles.Add($targetPath)
}

$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_ladder.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\EliteLadder.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in ($uniformSuffixes + $ladderSuffixes)) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_LADDER_$suffix",
        "IMAGE_REANIM_ZOMBIE_ELITE_LADDER_$suffix")
}
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

Write-Output "Generated $($generatedFiles.Count) elite ladder resources."
