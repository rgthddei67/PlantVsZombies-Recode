param(
    [string]$ResourceRoot = "build/clang-release/resources",
    [string]$ShovelSource = "tools/art_sources/snow_burrow_shovel_source.png"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resourcePath = [IO.Path]::GetFullPath((Join-Path $PWD $ResourceRoot))
$imagePath = Join-Path $resourcePath "image/reanim"
$particlePath = Join-Path $resourcePath "particles"
$reanimPath = Join-Path $resourcePath "reanim"
$shovelSourcePath = [IO.Path]::GetFullPath((Join-Path $PWD $ShovelSource))

function Clamp-Byte([double]$value) {
    return [byte][Math]::Max(0, [Math]::Min(255, [Math]::Round($value)))
}

function Convert-Pixel([Drawing.Color]$color, [string]$mode) {
    if ($color.A -eq 0) { return $color }
    $r = [double]$color.R
    $g = [double]$color.G
    $b = [double]$color.B
    $lum = 0.299 * $r + 0.587 * $g + 0.114 * $b
    $replace = $false
    $snow = $false

    switch ($mode) {
        "clothing" { $replace = $true }
        "red-clothing" {
            $replace = $r -gt 1.16 * $g -and $r -gt 1.12 * $b -and $r -gt 45
        }
        "snow" { $replace = $true; $snow = $true }
        "composite" {
            $redCloth = $r -gt 1.16 * $g -and $r -gt 1.12 * $b -and $r -gt 45
            $brownDirt = $r -gt 1.10 * $g -and $g -gt 1.08 * $b -and $r -gt 55
            $whiteHat = [Math]::Abs($r - $g) -lt 24 -and [Math]::Abs($g - $b) -lt 24 -and $lum -gt 105
            $replace = $redCloth -or $brownDirt -or $whiteHat
            $snow = $brownDirt -or $whiteHat
        }
    }

    if (-not $replace) { return $color }
    if ($snow) {
        $nr = 92 + 0.62 * $lum
        $ng = 112 + 0.66 * $lum
        $nb = 132 + 0.70 * $lum
    }
    else {
        $nr = 0.48 * $lum
        $ng = 0.68 * $lum
        $nb = 0.86 * $lum + 12
    }
    return [Drawing.Color]::FromArgb($color.A,
        (Clamp-Byte $nr), (Clamp-Byte $ng), (Clamp-Byte $nb))
}

function Convert-Image([string]$source, [string]$destination, [string]$mode) {
    $inputImage = [Drawing.Bitmap]::FromFile($source)
    try {
        $outputImage = New-Object Drawing.Bitmap $inputImage.Width, $inputImage.Height,
            ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $inputImage.Height; ++$y) {
                for ($x = 0; $x -lt $inputImage.Width; ++$x) {
                    $outputImage.SetPixel($x, $y,
                        (Convert-Pixel $inputImage.GetPixel($x, $y) $mode))
                }
            }
            $outputImage.Save($destination, [Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $outputImage.Dispose() }
    }
    finally { $inputImage.Dispose() }
}

function Convert-ReanimPart([string]$stem, [string]$mode) {
    $destinationStem = $stem -replace '^Zombie_digger', 'Zombie_snowburrow'
    Convert-Image (Join-Path $imagePath "$stem.png") `
        (Join-Path $imagePath "$destinationStem.png") $mode
    $glowSource = Join-Path $imagePath "${stem}_glow.png"
    if (Test-Path -LiteralPath $glowSource) {
        Convert-Image $glowSource (Join-Path $imagePath "${destinationStem}_glow.png") $mode
    }
}

@(
    @("Zombie_digger_body", "clothing"),
    @("Zombie_digger_hardhat", "snow"),
    @("Zombie_digger_outerarm_upper", "clothing"),
    @("Zombie_digger_outerarm_upper2", "clothing"),
    @("Zombie_digger_outerarm_lower", "red-clothing"),
    @("Zombie_digger_innerarm_upper", "clothing"),
    @("Zombie_digger_outerleg_upper", "clothing"),
    @("Zombie_digger_innerleg_upper", "clothing"),
    @("Zombie_digger_dirt", "snow"),
    @("Zombie_digger_dig0", "snow"),
    @("Zombie_digger_dig1", "snow"),
    @("Zombie_digger_dig2", "snow"),
    @("Zombie_digger_dig3", "snow"),
    @("Zombie_digger_dig4", "snow"),
    @("Zombie_digger_dig5", "snow"),
    @("Zombie_digger_rise2", "composite"),
    @("Zombie_digger_rise3", "composite"),
    @("Zombie_digger_rise4", "composite"),
    @("Zombie_digger_rise5", "composite"),
    @("Zombie_digger_rise6", "composite")
) | ForEach-Object { Convert-ReanimPart $_[0] $_[1] }

# ImageGen 母图只负责新雪铲轮廓；这里把它确定性缩入矿工工具轨的原画布与支点。
$pickaxe = [Drawing.Bitmap]::FromFile((Join-Path $imagePath "Zombie_digger_pickaxe.png"))
$shovel = [Drawing.Bitmap]::FromFile($shovelSourcePath)
try {
    $tool = New-Object Drawing.Bitmap $pickaxe.Width, $pickaxe.Height,
        ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [Drawing.Graphics]::FromImage($tool)
        try {
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            # 母图的 D 形尾柄会挤占矿工工具轨的 48px 画布；裁到铲面和主木柄，让宽铲在实机比例下仍清晰。
            $sourceRect = New-Object Drawing.Rectangle 0, 0,
                ([int][Math]::Round($shovel.Width * 0.82)),
                ([int][Math]::Round($shovel.Height * 0.78))
            $destinationRect = New-Object Drawing.Rectangle 1, 1,
                ($tool.Width - 2), ($tool.Height - 2)
            $graphics.DrawImage($shovel, $destinationRect, $sourceRect,
                [Drawing.GraphicsUnit]::Pixel)
        }
        finally { $graphics.Dispose() }
        $tool.Save((Join-Path $imagePath "Zombie_snowburrow_shovel.png"),
            [Drawing.Imaging.ImageFormat]::Png)

        $glow = New-Object Drawing.Bitmap $tool.Width, $tool.Height,
            ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $tool.Height; ++$y) {
                for ($x = 0; $x -lt $tool.Width; ++$x) {
                    $pixel = $tool.GetPixel($x, $y)
                    $glow.SetPixel($x, $y, [Drawing.Color]::FromArgb(
                        $pixel.A, 255, 255, 255))
                }
            }
            $glow.Save((Join-Path $imagePath "Zombie_snowburrow_shovel_glow.png"),
                [Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $glow.Dispose() }
    }
    finally { $tool.Dispose() }
}
finally {
    $pickaxe.Dispose()
    $shovel.Dispose()
}

Convert-Image (Join-Path $particlePath "ZombieDiggerHead.png") `
    (Join-Path $particlePath "ZombieSnowBurrowHead.png") "composite"
Convert-Image (Join-Path $particlePath "ZombieDiggerArm.png") `
    (Join-Path $particlePath "ZombieSnowBurrowArm.png") "composite"

# 雪帽没有独立耐久，断头时必须与头部预合成成一颗粒子，避免两颗随机物理粒子脱节。
$headPath = Join-Path $particlePath "ZombieSnowBurrowHead.png"
$head = [Drawing.Bitmap]::FromFile($headPath)
$hat = [Drawing.Bitmap]::FromFile((Join-Path $imagePath "Zombie_snowburrow_hardhat.png"))
try {
    $combined = New-Object Drawing.Bitmap 77, 72,
        ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [Drawing.Graphics]::FromImage($combined)
        try {
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.DrawImage($head, 10, 14, 57, 57)
            $graphics.DrawImage($hat, 0, 0, 77, 47)
        }
        finally { $graphics.Dispose() }
        $head.Dispose()
        $head = $null
        $combined.Save($headPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $combined.Dispose() }
}
finally {
    if ($null -ne $head) { $head.Dispose() }
    $hat.Dispose()
}

$sourceReanim = Join-Path $reanimPath "Zombie_digger.reanim"
$destinationReanim = Join-Path $reanimPath "SnowBurrow.reanim"
$reanim = [IO.File]::ReadAllText($sourceReanim)
$customParts = @(
    "BODY", "HARDHAT", "OUTERARM_UPPER", "OUTERARM_UPPER2", "OUTERARM_LOWER",
    "INNERARM_UPPER", "OUTERLEG_UPPER", "INNERLEG_UPPER", "DIRT",
    "DIG0", "DIG1", "DIG2", "DIG3", "DIG4", "DIG5",
    "RISE2", "RISE3", "RISE4", "RISE5", "RISE6"
)
foreach ($part in $customParts) {
    $reanim = $reanim.Replace("IMAGE_REANIM_ZOMBIE_DIGGER_$part",
        "IMAGE_REANIM_ZOMBIE_SNOWBURROW_$part")
}
$reanim = $reanim.Replace("IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE",
    "IMAGE_REANIM_ZOMBIE_SNOWBURROW_SHOVEL")
[IO.File]::WriteAllText($destinationReanim, $reanim,
    (New-Object Text.UTF8Encoding($false)))
