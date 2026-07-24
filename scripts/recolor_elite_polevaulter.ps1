param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$targetHue = 122.0  # Target sportswear hue in HSV degrees.
$materialSuffixes = @(
    'BODY1',
    'BODY2',
    'HAIR',
    'INNERARM_LOWER',
    'INNERLEG_FOOT',
    'INNERLEG_LOWER',
    'INNERLEG_TOE',
    'OUTERARM_LOWER',
    'OUTERLEG_FOOT',
    'OUTERLEG_LOWER',
    'OUTERLEG_TOE',
    'POLE',
    'POLE2'
)

# Lock generated outputs so System.Drawing or threshold drift cannot silently change assets.
Set-Variable -Name expectedHashes -Value (@{ bootstrap = 'bootstrap' }) -Scope Script
$expectedHashes.Remove('bootstrap')
$expectedHashes['image/reanim/Zombie_elitepolevaulter_body1.png'] = 'dc26d6920f6e1fe8fa9db54fd08c516adaea7e2aa0c09b2f23e7c28bcd5d3086'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_body2.png'] = 'e931ca1a5f2350378a8960278877200d36e580eed621ab4456f444e7614a84fe'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_hair.png'] = '10f4391e185009273b45b89f02826b86301aea1a276351b054125b1f2579bbc0'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_innerarm_lower.png'] = 'bbe5b83e73f663bc6a0d0f04a805fcea88715bb4bcdf715bb65520d206ca7c6f'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_innerleg_foot.png'] = '7f9ffa69e5023347166dc7435b265d8f36f4755468d157a70100ae8854258180'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_innerleg_lower.png'] = 'f2f1cc98ccfdda60843d92c01351a72813e7f6ef229f0b2b8dd323709a3a45f2'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_innerleg_toe.png'] = 'b84cb8739afdcc9abf09a33ff646e325e90915d8ac329060e8c99a252c665766'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_outerarm_lower.png'] = 'd7535e01c4e6eafefd7eb54ca69e09b86fb1e8b3c95120a8e92b3189f0c606e1'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_outerleg_foot.png'] = '5bd0a3ad1c5ff21c9c9d939bfa6e47731392019da206bfbd6745100203e7a808'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_outerleg_lower.png'] = '90ed4ead4225ff425f33032f102d5c4e247f07c6729797b4f50f594a7d7bcf81'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_outerleg_toe.png'] = 'acdc85807d3d788be2d1d42cb36a155b93a2681728cd7bb77ac6f9a4fba23828'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_pole.png'] = 'dd02c42ffdf8fab3561a4faf175d1557d58da1580f99f40a6fd55af4a95231b9'
$expectedHashes['image/reanim/Zombie_elitepolevaulter_pole2.png'] = '9ce5c3e80933ed4c0cf8e3caf693a38c04668ff0ae5e9a3ed507fd479810d83e'
$expectedHashes['particles/ZombieElitePoleVaulterHead.png'] = 'c6b0933839c5618d48559ad457f744902a91aca1285f218c0810a6900be104b5'
$expectedHashes['reanim/ElitePolevaulter.reanim'] = '5c1a749438d32e98ce150239f3aeed6662f0c5053c6b30a8bf50e05d1a48d83d'

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

function Convert-RedAndBlueMaterialToGreen {
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

                # Select red and blue sportswear only; keep skin, white trim, yellow accents and outlines unchanged.
                if ($max -lt 0.08) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($saturation -lt 0.18) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($green * 1.08) -and $blue -le ($green * 1.05)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }
                if ($red -le ($blue * 1.05) -and $blue -le ($red * 1.05)) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $greenSaturation = [Math]::Min(0.92, [Math]::Max(0.55, $saturation * 1.02))
                $rgb = Convert-HsvToRgb -Hue $targetHue -Saturation $greenSaturation -Value $max
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

foreach ($suffix in $materialSuffixes) {
    $sourceName = "Zombie_polevaulter_$($suffix.ToLowerInvariant()).png"
    $targetName = "Zombie_elitepolevaulter_$($suffix.ToLowerInvariant()).png"
    $sourcePath = Join-Path $imageDirectory $sourceName
    $targetPath = Join-Path $imageDirectory $targetName
    Convert-RedAndBlueMaterialToGreen -SourcePath $sourcePath -DestinationPath $targetPath
    $generatedFiles.Add($targetPath)
}

$sourceHead = Join-Path $resourceRoot 'particles\ZombiePoleVaulterHead.png'
$targetHead = Join-Path $resourceRoot 'particles\ZombieElitePoleVaulterHead.png'
Convert-RedAndBlueMaterialToGreen -SourcePath $sourceHead -DestinationPath $targetHead
$generatedFiles.Add($targetHead)

$sourceReanim = Join-Path $resourceRoot 'reanim\Polevaulter.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\ElitePolevaulter.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in $materialSuffixes) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_POLEVAULTER_$suffix",
        "IMAGE_REANIM_ZOMBIE_ELITEPOLEVAULTER_$suffix")
}
[System.IO.File]::WriteAllText(
    $targetReanim,
    $reanimText,
    [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetReanim)

if ($expectedHashes.Count -ne $generatedFiles.Count) {
    throw "Expected hash count $($expectedHashes.Count) does not match generated file count $($generatedFiles.Count)."
}

foreach ($path in $generatedFiles) {
    $relativePath = $path.Substring($resourceRoot.Length + 1).Replace('\', '/')
    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $expectedHashes.ContainsKey($relativePath)) {
        throw "Missing expected SHA-256 for $relativePath."
    }
    if ($actualHash -ne $expectedHashes[$relativePath]) {
        throw "SHA-256 mismatch for $relativePath. Expected $($expectedHashes[$relativePath]), got $actualHash."
    }
}

Write-Output "Verified $($generatedFiles.Count) elite polevaulter resource hashes."
