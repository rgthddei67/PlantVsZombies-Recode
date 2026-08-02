param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$purpleSuffixes = @(
    'BODY',
    'INNERARM_LOWER',
    'INNERARM_UPPER',
    'OUTERARM_LOWER',
    'OUTERARM_UPPER',
    'OUTERARM_UPPER2'
)
$tealSuffixes = @(
    'INNERLEG_LOWER',
    'INNERLEG_UPPER',
    'OUTERLEG_LOWER',
    'OUTERLEG_UPPER'
)
$goldFootSuffixes = @('INNERLEG_FOOT', 'OUTERLEG_FOOT')
$stickSuffixes = @('STICK', 'STICK2', 'STICK3', 'STICKDAMAGE2', 'STICK2DAMAGE2')
$timelineSuffixes = @(
    'BODY',
    'INNERARM_LOWER',
    'INNERARM_UPPER',
    'INNERLEG_FOOT',
    'INNERLEG_LOWER',
    'INNERLEG_UPPER',
    'OUTERARM_LOWER',
    'OUTERARM_UPPER',
    'OUTERLEG_FOOT',
    'OUTERLEG_LOWER',
    'OUTERLEG_UPPER',
    'STICK',
    'STICK2',
    'STICK3'
)

# Lock generated outputs so threshold or System.Drawing drift cannot change assets silently.
$expectedHashes = @{}
$expectedHashes['image/reanim/Zombie_elite_pogo_body.png'] = 'f7ecdf5ade681def442cb397c7f3ed12c813c1382cd424f201c17f4851681f22'
$expectedHashes['image/reanim/Zombie_elite_pogo_innerarm_lower.png'] = 'a104d42b9c1d98d435230bf47bac99da8fcecf93c694af992da68e91f3154026'
$expectedHashes['image/reanim/Zombie_elite_pogo_innerarm_upper.png'] = '461bb69a9525038ad2ef7782680667e77bccd7f04541c8ccd17a7ffe47262f59'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerarm_lower.png'] = 'c51f8e847eb78d1721b31faaaf31b995b24830b68bba4822d176ef1d0aa98eb6'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerarm_upper.png'] = '097d8a22d252fb394eedbf13aa65ffeefbb9eb69f69c5efeda8faa4d9d006ec6'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerarm_upper2.png'] = 'f793d9d9739a7ca52fa48d41549980c3a887aa24f5e958d94bd040c51e7132a4'
$expectedHashes['image/reanim/Zombie_elite_pogo_innerleg_lower.png'] = '28a6da107ccccf8660a424363044118bbd96e2368cc9a532be6ecf9738bbb3b1'
$expectedHashes['image/reanim/Zombie_elite_pogo_innerleg_upper.png'] = 'baefa7b26abb119869b4915a242fc2252e117710982360f95e462bcda03ba733'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerleg_lower.png'] = 'e327d4424d275eaee47c74b0d0aa7b488af23d57af9fa90ac80a9bfb4dffff71'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerleg_upper.png'] = 'd3cdccc3b7da41b97814356d786445e9130ebfd8a804cd25e6a5d8d4fe8e3cc4'
$expectedHashes['image/reanim/Zombie_elite_pogo_innerleg_foot.png'] = '88bc91df54ae7e38b6356624fe97a14b1b93b60e7cb2178fe7d810655f79a1af'
$expectedHashes['image/reanim/Zombie_elite_pogo_outerleg_foot.png'] = '4f8a54fb69889d7b5438378e92267583d0ec5d30bf28752aeb46c8e0b292583c'
$expectedHashes['image/reanim/Zombie_elite_pogo_stick.png'] = 'df8463c461a6c0502b8b14690601f0d0c69ea965598d94a2ccb0a7f83fc6643c'
$expectedHashes['image/reanim/Zombie_elite_pogo_stick2.png'] = '89c81cb683f1067149e2229f3fcd783c9634142e26dbd16c432d29700fa3ab46'
$expectedHashes['image/reanim/Zombie_elite_pogo_stick3.png'] = 'b31d427e03d2332705c15bfc1390c7ddc6df537d7ae123dfddb37c2041492b97'
$expectedHashes['image/reanim/Zombie_elite_pogo_stickdamage2.png'] = '6afa843544a18fcba01bbe8a761ff223b40fe6dcf483c64cecb39f09fdb7ce00'
$expectedHashes['image/reanim/Zombie_elite_pogo_stick2damage2.png'] = 'ec9408955c685bae6a6174eff9b1c517a4320a262aa947cf2e0ccceb75a5f715'
$expectedHashes['particles/ZombieElitePogo.png'] = '71a403208d3bb101a96c4f1047904f99eb513ce7a27d751435f6e49d4e21f11c'
$expectedHashes['reanim/ElitePogo.reanim'] = 'deddd3d2e8506529b29f3daa4b7d0a52dcacf523b70bb891a27bdf1912f85329'

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

function Convert-MaterialHue {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][double]$TargetHue,
        [bool]$PreserveRedAccent = $false
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

                # Preserve transparent antialiasing, black outlines, and the red chest badge.
                $isRedAccent = $PreserveRedAccent -and $saturation -gt 0.42 -and $red -gt ($green * 1.35) -and $red -gt ($blue * 1.35)
                if ($max -lt 0.07 -or $isRedAccent) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $targetSaturation = [Math]::Min(0.88, [Math]::Max(0.52, 0.58 + $saturation * 0.22))
                $rgb = Convert-HsvToRgb -Hue $TargetHue -Saturation $targetSaturation -Value $max
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

function Convert-RedToGold {
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
                $isRedMaterial = $saturation -ge 0.28 -and $red -ge ($green * 1.18) -and $red -ge ($blue * 1.18)
                if ($max -lt 0.08 -or -not $isRedMaterial) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                $rgb = Convert-HsvToRgb -Hue 44.0 -Saturation ([Math]::Min(0.92,
                    [Math]::Max(0.68, $saturation))) -Value $max
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

foreach ($suffix in $purpleSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_pogo_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_pogo_$($suffix.ToLowerInvariant()).png"
    Convert-MaterialHue -SourcePath $sourcePath -DestinationPath $targetPath -TargetHue 282.0 -PreserveRedAccent ($suffix -eq 'BODY')
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $tealSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_pogo_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_pogo_$($suffix.ToLowerInvariant()).png"
    Convert-MaterialHue -SourcePath $sourcePath -DestinationPath $targetPath -TargetHue 188.0
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $goldFootSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_pogo_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_pogo_$($suffix.ToLowerInvariant()).png"
    Convert-MaterialHue -SourcePath $sourcePath -DestinationPath $targetPath -TargetHue 44.0
    $generatedFiles.Add($targetPath)
}
foreach ($suffix in $stickSuffixes) {
    $sourcePath = Join-Path $imageDirectory "Zombie_pogo_$($suffix.ToLowerInvariant()).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elite_pogo_$($suffix.ToLowerInvariant()).png"
    Convert-RedToGold -SourcePath $sourcePath -DestinationPath $targetPath
    $generatedFiles.Add($targetPath)
}

$sourceParticle = Join-Path $resourceRoot 'particles\ZombiePogo.png'
$targetParticle = Join-Path $resourceRoot 'particles\ZombieElitePogo.png'
Convert-RedToGold -SourcePath $sourceParticle -DestinationPath $targetParticle
$generatedFiles.Add($targetParticle)

$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_pogo.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\ElitePogo.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in $timelineSuffixes) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_POGO_$suffix",
        "IMAGE_REANIM_ZOMBIE_ELITE_POGO_$suffix")
}
# The stick-hand sprite is exposed skin, so keep the original key after the STICK prefix rewrite.
$reanimText = $reanimText.Replace(
    'IMAGE_REANIM_ZOMBIE_ELITE_POGO_STICKHANDS',
    'IMAGE_REANIM_ZOMBIE_POGO_STICKHANDS')
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

Write-Output "Generated $($generatedFiles.Count) elite pogo resources."
