param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Add-Type -AssemblyName System.Drawing

$resourceRoot = Join-Path $RepositoryRoot "build/clang-release/resources"
$reanimImageRoot = Join-Path $resourceRoot "image/reanim"
$plantImageRoot = Join-Path $resourceRoot "image/PlantImage"
$particleRoot = Join-Path $resourceRoot "particles"

function Convert-TintedImage {
    param(
        [string]$SourcePath,
        [string]$DestinationPath,
        [scriptblock]$Palette
    )

    $source = [System.Drawing.Bitmap]::new($SourcePath)
    try {
        $output = [System.Drawing.Bitmap]::new(
            $source.Width,
            $source.Height,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; $y++) {
                for ($x = 0; $x -lt $source.Width; $x++) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $output.SetPixel($x, $y, $pixel)
                        continue
                    }

                    $maxChannel = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                    $minChannel = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                    $luminance = 0.299 * $pixel.R + 0.587 * $pixel.G + 0.114 * $pixel.B
                    # 眼睛、高光和黑色描边保持原色，只重染原素材的有色组织。
                    if (($maxChannel - $minChannel) -lt 14 -or $luminance -lt 20.0) {
                        $output.SetPixel($x, $y, $pixel)
                        continue
                    }

                    $target = & $Palette $x $y $source.Width $source.Height
                    $targetLuminance = [Math]::Max(
                        1.0,
                        0.299 * $target[0] + 0.587 * $target[1] + 0.114 * $target[2])
                    $scale = $luminance / $targetLuminance
                    $targetRed = [Math]::Min(255.0, $target[0] * $scale)
                    $targetGreen = [Math]::Min(255.0, $target[1] * $scale)
                    $targetBlue = [Math]::Min(255.0, $target[2] * $scale)
                    $blend = 0.9
                    $red = [int][Math]::Round($pixel.R * (1.0 - $blend) + $targetRed * $blend)
                    $green = [int][Math]::Round($pixel.G * (1.0 - $blend) + $targetGreen * $blend)
                    $blue = [int][Math]::Round($pixel.B * (1.0 - $blend) + $targetBlue * $blend)
                    $output.SetPixel($x, $y,
                        [System.Drawing.Color]::FromArgb($pixel.A, $red, $green, $blue))
                }
            }

            $output.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $output.Dispose()
        }
    }
    finally {
        $source.Dispose()
    }
}

$purple = { param($x, $y, $width, $height) @(165.0, 55.0, 215.0) }
$teal = { param($x, $y, $width, $height) @(25.0, 180.0, 145.0) }
$cardPalette = {
    param($x, $y, $width, $height)
    if ($y -ge 70) { return @(25.0, 180.0, 145.0) }
    return @(165.0, 55.0, 215.0)
}

$tealParts = @(
    "backleaf",
    "backleaf_lefttip",
    "backleaf_righttip",
    "frontleaf",
    "frontleaf_lefttip",
    "frontleaf_righttip",
    "stalk_bottom",
    "stalk_top"
)
foreach ($part in $tealParts) {
    Convert-TintedImage `
        (Join-Path $reanimImageRoot "PeaShooter_$part.png") `
        (Join-Path $reanimImageRoot "ToxicPeaShooter_$part.png") `
        $teal
}
Convert-TintedImage `
    (Join-Path $reanimImageRoot "PeaShooter_Head.png") `
    (Join-Path $reanimImageRoot "ToxicPeaShooter_Head.png") `
    $purple
Convert-TintedImage `
    (Join-Path $reanimImageRoot "PeaShooter_mouth.png") `
    (Join-Path $reanimImageRoot "ToxicPeaShooter_mouth.png") `
    $purple
# 头后小叶使用的是全局 anim_sprout，不属于地面 backleaf 轨道；为新植物派生独立紫色纹理。
Convert-TintedImage `
    (Join-Path $reanimImageRoot "anim_sprout.png") `
    (Join-Path $reanimImageRoot "ToxicPeaShooter_sprout.png") `
    $purple
Convert-TintedImage `
    (Join-Path $plantImageRoot "PeaShooter.png") `
    (Join-Path $plantImageRoot "ToxicPeaShooter.png") `
    $cardPalette
Convert-TintedImage `
    (Join-Path $resourceRoot "image/ProjectilePea.png") `
    (Join-Path $resourceRoot "image/ProjectileToxicPea.png") `
    $purple
Convert-TintedImage `
    (Join-Path $particleRoot "pea_splats.png") `
    (Join-Path $particleRoot "toxicpea_splats.png") `
    $purple
Convert-TintedImage `
    (Join-Path $particleRoot "pea_particles.png") `
    (Join-Path $particleRoot "toxicpea_particles.png") `
    $purple

$sourceReanim = Join-Path $resourceRoot "reanim/PeaShooter.reanim"
$destinationReanim = Join-Path $resourceRoot "reanim/ToxicPeaShooter.reanim"
$reanimText = [System.IO.File]::ReadAllText($sourceReanim)
$reanimText = $reanimText.Replace(
    "IMAGE_REANIM_PEASHOOTER_",
    "IMAGE_REANIM_TOXICPEASHOOTER_")
$reanimText = $reanimText.Replace(
    "IMAGE_REANIM_ANIM_SPROUT",
    "IMAGE_REANIM_TOXICPEASHOOTER_SPROUT")
[System.IO.File]::WriteAllText(
    $destinationReanim,
    $reanimText,
    [System.Text.UTF8Encoding]::new($false))

Write-Output "Generated ToxicPeaShooter card, projectile, hit particles, reanimation and 11 recolored track images."
