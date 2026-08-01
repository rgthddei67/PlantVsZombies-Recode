param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$resourceRoot = Join-Path $RepositoryRoot 'build\clang-release\resources'
$imageDirectory = Join-Path $resourceRoot 'image\reanim'
$generatedFiles = [System.Collections.Generic.List[string]]::new()

$materials = [ordered]@{
    'body' = 'Clothing'
    'innerarm_lower' = 'Clothing'
    'innerarm_upper' = 'Clothing'
    'outerarm_lower' = 'Clothing'
    'outerarm_upper' = 'Clothing'
    'outerarm_upper2' = 'Clothing'
    'hardhat' = 'Hardhat'
    'hardhat2' = 'Hardhat'
    'hardhat3' = 'Hardhat'
    'pickaxe' = 'Pickaxe'
    'rise2' = 'RiseComposite'
    'rise3' = 'RiseComposite'
    'rise4' = 'RiseComposite'
    'rise5' = 'RiseComposite'
    'rise6' = 'RiseComposite'
}

# Generated outputs are hash-locked after the first deterministic generation.
$expectedHashes = @{}
$expectedHashes['image/reanim/Zombie_elitedigger_body.png'] = '8d4e8264f56ca5298575af531578172d6cdc5a542a967b8c569cab8c3fe36983'
$expectedHashes['image/reanim/Zombie_elitedigger_hardhat.png'] = 'deccc9ecf0636a411a0717ba4ad20560ded6b0fd779791a229276dc68ed23907'
$expectedHashes['image/reanim/Zombie_elitedigger_hardhat2.png'] = '09326180b7f89df9d0b7d2e64f352abe71d22e4f5fb3716fe6390fbb2a6c33a5'
$expectedHashes['image/reanim/Zombie_elitedigger_hardhat3.png'] = 'a03b5a2288c71d92d332286bf0c53d3d563d7ec6182d46e0f4cd357683890ef3'
$expectedHashes['image/reanim/Zombie_elitedigger_innerarm_lower.png'] = '055204ddc44494dbe19b12ce37b8ada5a76b4a057999c4986c73c0933c17549c'
$expectedHashes['image/reanim/Zombie_elitedigger_innerarm_upper.png'] = '6c1e7db95f7f9adca033586a02058fa3c184608b6dcbda3863e450a889d2a7cf'
$expectedHashes['image/reanim/Zombie_elitedigger_outerarm_lower.png'] = '25e2bf960dc65a4da98174cd6dd61ed2876ba4f34f2c2eb55c06c34c25b7546c'
$expectedHashes['image/reanim/Zombie_elitedigger_outerarm_upper.png'] = '9d703444efac7649414414037a6b5bb46c6192f0b724317b4cd7823b52d3b860'
$expectedHashes['image/reanim/Zombie_elitedigger_outerarm_upper2.png'] = '4893d5a26355364021c92017073f0bdb7d8d8b1fe9e841cc471e05c47e025a91'
$expectedHashes['image/reanim/Zombie_elitedigger_pickaxe.png'] = 'd68ade73ee25408f24ca279440744071f777db65aeeb28afc929753e22515ab3'
$expectedHashes['image/reanim/Zombie_elitedigger_rise2.png'] = '7aaf6927d66df32db8851bf93bd56c4708a830bf310e52f95430f2ed7f51686b'
$expectedHashes['image/reanim/Zombie_elitedigger_rise3.png'] = 'efb9d21666f31a03f9679e181868fd0f33b20285738cb279316ef7216ad5f26f'
$expectedHashes['image/reanim/Zombie_elitedigger_rise4.png'] = '6d785edf168f7e536e73d21ba9d1cf6eed6e0435e7b292d8968ae88b7ebfcdb7'
$expectedHashes['image/reanim/Zombie_elitedigger_rise5.png'] = '164b7d7bdd3e4dff22f640e89a589aac93d4f048d677b576a4e1f0ff200b9024'
$expectedHashes['image/reanim/Zombie_elitedigger_rise6.png'] = 'b2e726867f1fd45207b4cba87da350ca66a29ee8a1047be263772dac09ac1d4f'
$expectedHashes['particles/ZombieEliteDiggerArm.png'] = '3021f995e09b9236f12745dc2b2ecb0a3af7ff8f00b125afde5554d503f08932'
$expectedHashes['reanim/EliteDigger.reanim'] = '5459d4836dd56b2f789b783f7d36814e34eacca5b211befeae847495c5c336df'

function Convert-HsvToRgb {
    param([double]$Hue, [double]$Saturation, [double]$Value)
    $chroma = $Value * $Saturation
    $sector = $Hue / 60.0
    $x = $chroma * (1.0 - [Math]::Abs(($sector % 2.0) - 1.0))
    $r1 = 0.0; $g1 = 0.0; $b1 = 0.0
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
        [Math]::Min(255, [Math]::Max(0, [int][Math]::Round(($b1 + $m) * 255.0))))
}

function Convert-Image {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][ValidateSet('Clothing', 'Hardhat', 'Pickaxe', 'RiseComposite')][string]$Material
    )
    $source = [System.Drawing.Bitmap]::new($SourcePath)
	$sourceName = [System.IO.Path]::GetFileNameWithoutExtension($SourcePath)
    $result = [System.Drawing.Bitmap]::new(
        $source.Width, $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $color = $source.GetPixel($x, $y)
                if ($color.A -eq 0) { $result.SetPixel($x, $y, $color); continue }
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

                if ($Material -eq 'RiseComposite') {
                    # Composite rise sprites bake shirt and helmet together; use color plus a top-region helmet mask.
                    $isRedClothing = $saturation -ge 0.18 -and $red -gt ($green * 1.08) -and $red -gt ($blue * 1.05)
					$inHelmetRegion = switch ($sourceName) {
						'Zombie_digger_rise2' { ($y -le 27 -and $x -ge 15 -and $x -le 112) -or ($y -le 50 -and ($x -le 35 -or $x -ge 91)) }
						'Zombie_digger_rise3' { $y -le 24 -and $x -ge 5 -and $x -le 82 }
						'Zombie_digger_rise4' { ($y -le 24 -and $x -ge 25 -and $x -le 108) -or ($y -le 31 -and $x -ge 34 -and $x -le 83) }
						'Zombie_digger_rise5' { $y -le 36 -and $x -ge 30 -and $x -le 106 }
						'Zombie_digger_rise6' { ($y -le 24 -and $x -ge 35 -and $x -le 108) -or ($y -le 32 -and $x -ge 42 -and $x -le 90) }
						default { $false }
					}
					$isTopHelmet = $inHelmetRegion -and $max -ge 0.35 -and $saturation -le 0.38
                    if ($isRedClothing) {
                        $replace = $true
                        $targetHue = 37.0
                        $targetSaturation = [Math]::Min(0.92, [Math]::Max(0.62, $saturation * 1.12))
                        $targetValue = [Math]::Min(0.96, [Math]::Max(0.10, $max * 1.03))
                    }
                    elseif ($isTopHelmet) {
                        $replace = $true
                        $targetHue = 34.0
                        $targetSaturation = [Math]::Min(0.92, [Math]::Max(0.70, 0.78 + (1.0 - $max) * 0.08))
                        $targetValue = [Math]::Min(0.98, [Math]::Max(0.12, $max * 0.98))
                    }
                }
                elseif ($Material -eq 'Clothing') {
                    # Only the red plaid/sleeve material changes; skin, denim and outlines remain intact.
                    $replace = $saturation -ge 0.18 -and $red -gt ($green * 1.08) -and $red -gt ($blue * 1.05)
                    $targetHue = 37.0
                    $targetSaturation = [Math]::Min(0.92, [Math]::Max(0.62, $saturation * 1.12))
                    $targetValue = [Math]::Min(0.96, [Math]::Max(0.10, $max * 1.03))
                }
                elseif ($Material -eq 'Hardhat') {
                    # Recolor light helmet material while retaining lamp, cracks, alpha and dark outline.
                    $replace = $max -ge 0.20 -and $saturation -le 0.38
                    $targetHue = 34.0
                    $targetSaturation = [Math]::Min(0.92, [Math]::Max(0.70, 0.78 + (1.0 - $max) * 0.08))
                    $targetValue = [Math]::Min(0.98, [Math]::Max(0.12, $max * 0.98))
                }
                else {
                    # Cyan steel distinguishes the pick head; brown wooden handle stays unchanged.
                    $replace = $max -ge 0.18 -and $saturation -le 0.25
                    $targetHue = 190.0
                    $targetSaturation = [Math]::Min(0.90, [Math]::Max(0.62, 0.68 + (1.0 - $max) * 0.08))
                    $targetValue = [Math]::Min(0.96, [Math]::Max(0.12, $max * 1.02))
                }
                if (-not $replace) { $result.SetPixel($x, $y, $color); continue }
                $rgb = Convert-HsvToRgb -Hue $targetHue -Saturation $targetSaturation -Value $targetValue
                $result.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $color.A, $rgb[0], $rgb[1], $rgb[2]))
            }
        }
        $result.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $result.Dispose(); $source.Dispose() }
}

foreach ($entry in $materials.GetEnumerator()) {
    $sourcePath = Join-Path $imageDirectory "Zombie_digger_$($entry.Key).png"
    $targetPath = Join-Path $imageDirectory "Zombie_elitedigger_$($entry.Key).png"
    Convert-Image -SourcePath $sourcePath -DestinationPath $targetPath -Material $entry.Value
    $generatedFiles.Add($targetPath)
}

$sourceParticle = Join-Path $resourceRoot 'particles\ZombieDiggerArm.png'
$targetParticle = Join-Path $resourceRoot 'particles\ZombieEliteDiggerArm.png'
Convert-Image -SourcePath $sourceParticle -DestinationPath $targetParticle -Material Clothing
$generatedFiles.Add($targetParticle)

$sourceReanim = Join-Path $resourceRoot 'reanim\Zombie_digger.reanim'
$targetReanim = Join-Path $resourceRoot 'reanim\EliteDigger.reanim'
$reanimText = [System.IO.File]::ReadAllText($sourceReanim, [System.Text.Encoding]::UTF8)
foreach ($suffix in $materials.Keys) {
    $upper = $suffix.ToUpperInvariant()
    $reanimText = $reanimText.Replace(
        "IMAGE_REANIM_ZOMBIE_DIGGER_$upper",
        "IMAGE_REANIM_ZOMBIE_ELITEDIGGER_$upper")
}
[System.IO.File]::WriteAllText($targetReanim, $reanimText, [System.Text.UTF8Encoding]::new($false))
$generatedFiles.Add($targetReanim)

$actualHashes = @{}
foreach ($path in $generatedFiles) {
    $relativePath = $path.Substring($resourceRoot.Length + 1).Replace('\', '/')
    $actualHashes[$relativePath] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

if ($expectedHashes.Count -ne $generatedFiles.Count) {
    foreach ($relativePath in ($actualHashes.Keys | Sort-Object)) {
        Write-Output "`$expectedHashes['$relativePath'] = '$($actualHashes[$relativePath])'"
    }
    throw "Expected hash count $($expectedHashes.Count) does not match generated file count $($generatedFiles.Count)."
}
foreach ($relativePath in $actualHashes.Keys) {
    if (-not $expectedHashes.ContainsKey($relativePath)) { throw "Missing expected SHA-256 for $relativePath." }
    if ($actualHashes[$relativePath] -ne $expectedHashes[$relativePath]) {
        throw "SHA-256 mismatch for $relativePath. Expected $($expectedHashes[$relativePath]), got $($actualHashes[$relativePath])."
    }
}
Write-Output "Verified $($generatedFiles.Count) elite digger resource hashes."
