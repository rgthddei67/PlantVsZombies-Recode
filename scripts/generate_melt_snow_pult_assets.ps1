param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$resourceRoot = Join-Path $RepositoryRoot 'build/clang-release/resources'
$reanimImageRoot = Join-Path $resourceRoot 'image/reanim'
$cardRoot = Join-Path $resourceRoot 'image/PlantImage'
$particleRoot = Join-Path $resourceRoot 'particles'
$reanimRoot = Join-Path $resourceRoot 'reanim'

function New-RecoloredBitmap(
    [string]$sourcePath,
    [int]$blueBias,
    [int]$greenBias
) {
    $source = [System.Drawing.Bitmap]::new($sourcePath)
    $target = [System.Drawing.Bitmap]::new(
        $source.Width, $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $source.Height; ++$y) {
            for ($x = 0; $x -lt $source.Width; ++$x) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.A -eq 0) { continue }
                $maximum = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                $minimum = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $luminance = (0.299 * $pixel.R) + (0.587 * $pixel.G) + (0.114 * $pixel.B)

                # 保留眼白与近黑五官；绿色植株材质统一映成容易识别的冰蓝色。
                if (($maximum - $minimum) -lt 22 -and ($luminance -gt 155 -or $luminance -lt 34)) {
                    $color = $pixel
                }
                else {
                    $r = [int][Math]::Min(255, 15 + $luminance * 0.45)
                    $g = [int][Math]::Min(255, $greenBias + $luminance * 0.62)
                    $b = [int][Math]::Min(255, $blueBias + $luminance * 0.78)
                    $color = [System.Drawing.Color]::FromArgb($pixel.A, $r, $g, $b)
                }
                $target.SetPixel($x, $y, $color)
            }
        }
        return $target
    }
    finally {
        $source.Dispose()
    }
}

function Save-Recolored(
    [string]$sourcePath,
    [string]$targetPath,
    [int]$blueBias = 54,
    [int]$greenBias = 42
) {
    $bitmap = New-RecoloredBitmap $sourcePath $blueBias $greenBias
    try {
        $bitmap.Save($targetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

# 逐部件派生而不是运行时整体染色，确保卡图、默认实例与 -NoInstance 使用同一套最终像素。
Get-ChildItem -LiteralPath $reanimImageRoot -Filter 'Cabbagepult_*.png' | ForEach-Object {
    $suffix = $_.Name.Substring('Cabbagepult_'.Length)
    if ($suffix -eq 'cabbage.png') { $suffix = 'snowclod.png' }
    Save-Recolored $_.FullName (Join-Path $reanimImageRoot ("MeltSnowPult_" + $suffix))
}
$frontLeafSources = @(
    'Peashooter_frontleaf.png',
    'Peashooter_frontleaf_lefttip.png',
    'Peashooter_frontleaf_righttip.png'
)
foreach ($sourceName in $frontLeafSources) {
    $suffix = $sourceName.Substring('Peashooter_'.Length)
    Save-Recolored (Join-Path $reanimImageRoot $sourceName) `
        (Join-Path $reanimImageRoot ("MeltSnowPult_" + $suffix))
}
Save-Recolored (Join-Path $cardRoot 'Cabbagepult.png') `
    (Join-Path $cardRoot 'MeltSnowPult.png')

# 盐晶与原雪团保持 30x27 画布，运行时换图不会改变动画锚点。
$scale = 4
$large = [System.Drawing.Bitmap]::new(
    30 * $scale, 27 * $scale,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($large)
try {
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $s = [float]$scale
    $outline = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(245, 27, 65, 92), 2.2 * $s)
    $core = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 190, 239, 255))
    $shine = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(240, 248, 255, 255), 1.2 * $s)
    try {
        $points = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new(15 * $s, 1 * $s),
            [System.Drawing.PointF]::new(27 * $s, 9 * $s),
            [System.Drawing.PointF]::new(23 * $s, 24 * $s),
            [System.Drawing.PointF]::new(8 * $s, 26 * $s),
            [System.Drawing.PointF]::new(2 * $s, 12 * $s))
        $graphics.FillPolygon($core, $points)
        $graphics.DrawPolygon($outline, $points)
        $graphics.DrawLine($shine, 15 * $s, 4 * $s, 11 * $s, 21 * $s)
        $graphics.DrawLine($shine, 15 * $s, 4 * $s, 23 * $s, 10 * $s)
    }
    finally {
        $outline.Dispose()
        $core.Dispose()
        $shine.Dispose()
    }

    $salt = [System.Drawing.Bitmap]::new(
        30, 27, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $smallGraphics = [System.Drawing.Graphics]::FromImage($salt)
    try {
        $smallGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $smallGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $smallGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $smallGraphics.DrawImage($large, 0, 0, 30, 27)
        $salt.Save((Join-Path $reanimImageRoot 'MeltSnowPult_saltcrystal.png'),
            [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $smallGraphics.Dispose()
        $salt.Dispose()
    }
}
finally {
    $graphics.Dispose()
    $large.Dispose()
}

# 两种命中特效沿用雪豌豆分片几何，只改变配色以保持低分辨率可读性。
Save-Recolored (Join-Path $particleRoot 'snowpea_splats.png') `
    (Join-Path $particleRoot 'melt_snow_splats.png') 42 36
Save-Recolored (Join-Path $particleRoot 'snowpea_particles.png') `
    (Join-Path $particleRoot 'melt_snow_particles.png') 42 36
Save-Recolored (Join-Path $particleRoot 'snowpea_splats.png') `
    (Join-Path $particleRoot 'salt_crystal_splats.png') 92 70
Save-Recolored (Join-Path $particleRoot 'snowpea_particles.png') `
    (Join-Path $particleRoot 'salt_crystal_particles.png') 92 70

$sourceReanim = Get-Content -LiteralPath (Join-Path $reanimRoot 'Cabbagepult.reanim') -Raw
$targetReanim = $sourceReanim.Replace(
    'IMAGE_REANIM_CABBAGEPULT_CABBAGE', 'IMAGE_REANIM_MELTSNOWPULT_SNOWCLOD')
$targetReanim = $targetReanim.Replace('Cabbagepult_cabbage', 'MeltSnowPult_snowclod')
$targetReanim = $targetReanim.Replace(
    'IMAGE_REANIM_CABBAGEPULT_', 'IMAGE_REANIM_MELTSNOWPULT_')
$targetReanim = $targetReanim.Replace(
    'IMAGE_REANIM_PEASHOOTER_FRONTLEAF', 'IMAGE_REANIM_MELTSNOWPULT_FRONTLEAF')
$targetReanim = $targetReanim.Replace('Cabbagepult_', 'MeltSnowPult_')
Set-Content -LiteralPath (Join-Path $reanimRoot 'MeltSnowPult.reanim') `
    -Value $targetReanim -Encoding utf8NoBOM -NoNewline

Get-FileHash -Algorithm SHA256 -LiteralPath @(
    (Join-Path $cardRoot 'MeltSnowPult.png'),
    (Join-Path $reanimImageRoot 'MeltSnowPult_head.png'),
    (Join-Path $reanimImageRoot 'MeltSnowPult_snowclod.png'),
    (Join-Path $reanimImageRoot 'MeltSnowPult_saltcrystal.png'),
    (Join-Path $reanimRoot 'MeltSnowPult.reanim')
) | ForEach-Object { "{0}  {1}" -f $_.Hash, $_.Path }
