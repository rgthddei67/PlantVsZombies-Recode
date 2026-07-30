param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$clothingSuffixes = @(
    'BODY1',
    'BODY2',
    'INNERARM_LOWER',
    'INNERARM_UPPER',
    'INNERLEG_FOOT',
    'INNERLEG_LOWER',
    'INNERLEG_UPPER',
    'OUTERARM_LOWER',
    'OUTERARM_LOWER2',
    'OUTERARM_UPPER',
    'OUTERLEG_FOOT',
    'OUTERLEG_LOWER',
    'OUTERLEG_UPPER'
)
$boxSuffixes = @(
    'BOX',
    'BOX2',
    'HANDLE'
)

# Generated outputs are locked so threshold or runtime drift cannot silently alter art.
Set-Variable -Name expectedHashes -Value (@{ bootstrap = 'bootstrap' }) -Scope Script
$expectedHashes.Remove('bootstrap')
$expectedHashes['image/reanim/Zombie_elitejackbox_body1.png'] = 'e491dfefe510b57cda652941babc4bc5dd542ac7c3290aae1a4b565608b03842'
$expectedHashes['image/reanim/Zombie_elitejackbox_body2.png'] = '7fb0a4327160ec8563900bd4f44a76e38f4e1a89afcd52f09fdc098e582e1bfc'
$expectedHashes['image/reanim/Zombie_elitejackbox_box.png'] = 'ec72c5e9f7116e3182f24999e6f66a7b5ddd90e899e8b0224d99718d4f57dd8f'
$expectedHashes['image/reanim/Zombie_elitejackbox_box2.png'] = 'b318bab14e6c2c9ef3526ace97f0f3eeff2188955ad1d98726875bde57736aa8'
$expectedHashes['image/reanim/Zombie_elitejackbox_handle.png'] = '4e5bbf52c5e9c69adc5696a08d0106dc87891a8636ce2ad7b6abf9b998ccd59b'
$expectedHashes['image/reanim/Zombie_elitejackbox_innerarm_lower.png'] = '35f2b72276f25a5fe7c0dfe6e7b308e1040f3c912e0020e8283709aa7a6f92bb'
$expectedHashes['image/reanim/Zombie_elitejackbox_innerarm_upper.png'] = '66028aab6dbb20d0303ce9ac38ca97c9574fbdef5af2188d9e3efcaf5676a15f'
$expectedHashes['image/reanim/Zombie_elitejackbox_innerleg_foot.png'] = '3ae4f9cd42cc4aaa3d3691e5599ba3905a764d78240852ca3d44dffb3c07f88c'
$expectedHashes['image/reanim/Zombie_elitejackbox_innerleg_lower.png'] = '78789f1af18df21d85d5e0af51d94cec974fd8bf217d65334df88e5b13d47114'
$expectedHashes['image/reanim/Zombie_elitejackbox_innerleg_upper.png'] = 'fb2c6dbd1ea78ce18684a29b31481a2c24efbd263d29e378049a04d6c24dab0e'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerarm_lower.png'] = 'e73495bc9843a6a6133295959615e95f36d47f24eb3fa17e66a6898d864e09d2'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerarm_lower2.png'] = 'fdfe75a035a6587afb4ac2d1f4eb64e5a08ea4b1b3351279faafcece495384f2'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerarm_upper.png'] = '79531f515055f0ae81b8ca04f48ce0f52ebec1f3ead58fbf17be6573a793f84d'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerleg_foot.png'] = '19687ab96206291be288c699a362a95c99f112d36e58bb566fed4119200d9490'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerleg_lower.png'] = 'ed6972e5029d6049ec93ee238a2cdfee3fa1f32bdf032b8f1a475c970a0d3958'
$expectedHashes['image/reanim/Zombie_elitejackbox_outerleg_upper.png'] = '16a5ce8d2c172e09fe158d26d491c9b38bef78ba2fee2f9e851f0bde08d06bc2'
$expectedHashes['particles/ZombieEliteJackboxArm.png'] = '51e1164fe82b926e9a98a175d7e9a1e541d5a8dea852d298342697612bd386e8'
$expectedHashes['reanim/EliteJackBox.reanim'] = 'a22cfeca498dc098466f703f078e35e2e41b1c939f1efe786a1159a8889a9cbd'

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

function Convert-Image {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][ValidateSet('Clothing', 'Box')][string]$Material
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
                $replace = $false
                $targetHue = 0.0
                $targetSaturation = 0.0
                $targetValue = $max

                if ($Material -eq 'Clothing') {
                    # Select white/gray suit fabric only; keep gold straps, outlines and transparent edges.
                    $replace = $max -ge 0.18 -and $saturation -le 0.24
                    $targetHue = 276.0
                    $targetSaturation = [Math]::Min(
                        0.72, [Math]::Max(0.58, 0.62 + (1.0 - $max) * 0.06))
                    $targetValue = [Math]::Min(
                        0.78, [Math]::Max(0.10, $max * 0.72 + 0.04))
                }
                else {
                    $isRed = $saturation -ge 0.25 `
                        -and $red -gt ($green * 1.15) `
                        -and $red -gt ($blue * 1.08)
                    $isCyan = $saturation -ge 0.20 `
                        -and $blue -gt ($red * 1.10) `
                        -and $green -gt ($red * 1.05)
                    if ($isRed) {
                        $replace = $true
                        $targetHue = 278.0
                        $targetSaturation = [Math]::Min(
                            0.90, [Math]::Max(0.64, $saturation))
                        $targetValue = [Math]::Min(
                            0.78, [Math]::Max(0.12, $max * 0.74 + 0.04))
                    }
                    elseif ($isCyan) {
                        $replace = $true
                        $targetHue = 45.0
                        $targetSaturation = [Math]::Min(
                            0.90, [Math]::Max(0.70, $saturation))
                        $targetValue = [Math]::Min(
                            0.96, [Math]::Max(0.22, $max * 1.02))
                    }
                }

                if (-not $replace) {
                    $result.SetPixel($x, $y, $color)
                    continue
                }

                $rgb = Convert-HsvToRgb `
                    -Hue $targetHue `
                    -Saturation $targetSaturation `
                    -Value $targetValue
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

foreach ($suffix in $clothingSuffixes) {
    $sourceName = "Zombie_jackbox_$($suffix.ToLowerInvariant()).png"
    $targetName = "Zombie_elitejackbox_$($suffix.ToLowerInvariant()).png"
    $sourcePath = Join-Path $imageDirectory $sourceName
    $targetPath = Join-Path $imageDirectory $targetName
    Convert-Image -SourcePath $sourcePath -DestinationPath $targetPath -Material Clothing
    $generatedFiles.Add($targetPath)
}

foreach ($suffix in $boxSuffixes) {
    $sourceName = "Zombie_jackbox_$($suffix.ToLowerInvariant()).png"
    $targetName = "Zombie_elitejackbox_$($suffix.ToLowerInvariant()).png"
    $sourcePath = Join-Path $imageDirectory $sourceName
    $targetPath = Join-Path $imageDirectory $targetName
    Convert-Image -SourcePath $sourcePath -DestinationPath $targetPath -Material Box
    $generatedFiles.Add($targetPath)
}

$sourceParticle = Join-Path $resourceRoot 'particles\ZombieJackboxArm.png'
$targetParticle = Join-Path $resourceRoot 'particles\ZombieEliteJackboxArm.png'
Convert-Image `
    -SourcePath $sourceParticle `
    -DestinationPath $targetParticle `
    -Material Clothing
$generatedFiles.Add($targetParticle)

$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_JackBox.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\EliteJackBox.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in @($clothingSuffixes + $boxSuffixes)) {
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_JACKBOX_$suffix",
        "IMAGE_REANIM_ZOMBIE_ELITEJACKBOX_$suffix")
}
[System.IO.File]::WriteAllText(
    $targetReanim,
    $reanimText,
    [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetReanim)

$actualHashes = @{}
foreach ($path in $generatedFiles) {
    $relativePath = $path.Substring($resourceRoot.Length + 1).Replace('\', '/')
    $actualHashes[$relativePath] =
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

if ($expectedHashes.Count -ne $generatedFiles.Count) {
    foreach ($relativePath in ($actualHashes.Keys | Sort-Object)) {
        Write-Output "`$expectedHashes['$relativePath'] = '$($actualHashes[$relativePath])'"
    }
    throw "Expected hash count $($expectedHashes.Count) does not match generated file count $($generatedFiles.Count)."
}

foreach ($relativePath in $actualHashes.Keys) {
    if (-not $expectedHashes.ContainsKey($relativePath)) {
        throw "Missing expected SHA-256 for $relativePath."
    }
    if ($actualHashes[$relativePath] -ne $expectedHashes[$relativePath]) {
        throw "SHA-256 mismatch for $relativePath. Expected $($expectedHashes[$relativePath]), got $($actualHashes[$relativePath])."
    }
}

Write-Output "Verified $($generatedFiles.Count) elite jack-in-the-box resource hashes."
