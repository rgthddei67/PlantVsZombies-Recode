param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sourcePath = Join-Path $RepositoryRoot 'docs/art/frost-mine/frost-mine-concept-v1.png'
$resourceRoot = Join-Path $RepositoryRoot 'build/clang-release/resources'
$reanimImageRoot = Join-Path $resourceRoot 'image/reanim'
$cardRoot = Join-Path $resourceRoot 'image/PlantImage'
$particleRoot = Join-Path $resourceRoot 'particles'
$reanimRoot = Join-Path $resourceRoot 'reanim'
$expectedSourceHash = '2EF0001B381B7B1CDBC2445A0F7B15FC8827D1A33C6955A4D06F18D13BB4ADD5'
$expectedOutputHashes = @{
    'REANIM_FROSTMINE_DORMANT.png' = '536C3B02CD2229DC707AB0DBB279385435739E56DF46E67AD23C6315E843C7AF'
    'REANIM_FROSTMINE_CALIBRATED.png' = 'F26A1BC33A87A3320CF03CBEABEBEDC1CAC1453C67808497632484C0FC2AB20A'
    'REANIM_FROSTMINE_ARMED.png' = '0902D8F933F69F7D6BB3B7F61BE9D8D7AEE543BAF862549DE81F60DE47F756A5'
    'FrostMine.png' = 'C6D27DF498C4E744CDD4F3612528A301FDDC23B4AF3E28082DD0581BD2234BAA'
    'FrostMineShards.png' = 'BEA07CCF461594186FE8A3F99DE04AF6BBA754C7F776A647E030B743DAA6A854'
    'FrostMinePulse.png' = 'DB3F34EC6EA2B4B265038547A07A7ED7FB466D644334E6AEDB6A6200FD4AC499'
    'FrostMine.reanim' = 'C621597B5D3B7FA16AF49612667409AB4CF71D584B464D8F3D21F083ED11A372'
}

function New-ArgbBitmap([int]$Width, [int]$Height) {
    return [System.Drawing.Bitmap]::new(
        $Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Save-Downsampled(
    [System.Drawing.Bitmap]$Source,
    [int]$Width,
    [int]$Height,
    [string]$DestinationPath
) {
    $target = New-ArgbBitmap $Width $Height
    $graphics = [System.Drawing.Graphics]::FromImage($target)
    try {
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Source, 0, 0, $Width, $Height)
        $target.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $target.Dispose()
    }
}

if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash -ne $expectedSourceHash) {
    throw '伏霜雷 ImageGen 源图哈希不匹配，拒绝静默生成漂移资源。'
}

$concept = [System.Drawing.Bitmap]::new($sourcePath)
try {
    if ($concept.Width -ne 2172 -or $concept.Height -ne 724) {
        throw "伏霜雷源图尺寸应为 2172x724，实际为 $($concept.Width)x$($concept.Height)。"
    }

    $states = @(
        @{ Name = 'dormant'; Index = 0 },
        @{ Name = 'calibrated'; Index = 1 },
        @{ Name = 'armed'; Index = 2 }
    )
    foreach ($state in $states) {
        # ImageGen 源图恰好由三个 724x724 正方形状态组成；统一裁框保证本体尺度和脚底基线不跳。
        $frame = New-ArgbBitmap 724 724
        $graphics = [System.Drawing.Graphics]::FromImage($frame)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $sourceRect = [System.Drawing.Rectangle]::new($state.Index * 724, 0, 724, 724)
            $graphics.DrawImage($concept, [System.Drawing.Rectangle]::new(0, 0, 724, 724),
                $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
            Save-Downsampled $frame 100 100 (Join-Path $reanimImageRoot (
                'REANIM_FROSTMINE_' + $state.Name.ToUpperInvariant() + '.png'))
        }
        finally {
            $graphics.Dispose()
            $frame.Dispose()
        }
    }
}
finally {
    $concept.Dispose()
}

# 卡图使用校准态，发光雪花芯在 120px 卡片里仍能直接说明其预报联动身份。
$card = New-ArgbBitmap 120 120
$calibrated = [System.Drawing.Bitmap]::new((Join-Path $reanimImageRoot 'REANIM_FROSTMINE_CALIBRATED.png'))
$cardGraphics = [System.Drawing.Graphics]::FromImage($card)
try {
    $cardGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $cardGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $cardGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $cardGraphics.DrawImage($calibrated, 10, 10, 100, 100)
    $card.Save((Join-Path $cardRoot 'FrostMine.png'), [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $cardGraphics.Dispose()
    $calibrated.Dispose()
    $card.Dispose()
}

# 四分片短冰晶：深蓝描边、青白核心和不同倾角，低分辨率仍保留清晰材质层次。
$particleScale = 4
$largeShards = New-ArgbBitmap (96 * $particleScale) (24 * $particleScale)
$shardGraphics = [System.Drawing.Graphics]::FromImage($largeShards)
try {
    $shardGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $shardGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    for ($index = 0; $index -lt 4; ++$index) {
        $s = [float]$particleScale
        $left = [float]($index * 24 * $particleScale)
        $tilt = [float](($index - 1.5) * 1.6 * $particleScale)
        $points = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new($left + 12 * $s + $tilt, 1 * $s),
            [System.Drawing.PointF]::new($left + 21 * $s, 9 * $s),
            [System.Drawing.PointF]::new($left + 15 * $s, 22 * $s),
            [System.Drawing.PointF]::new($left + 4 * $s, 16 * $s),
            [System.Drawing.PointF]::new($left + 5 * $s, 6 * $s))
        $fill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(
            245, 75 + $index * 12, 191 + $index * 8, 245))
        $outline = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(
            255, 15, 44, 92), 2.0 * $s)
        $highlight = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(
            235, 225, 253, 255), 1.0 * $s)
        try {
            $shardGraphics.FillPolygon($fill, $points)
            $shardGraphics.DrawPolygon($outline, $points)
            $shardGraphics.DrawLine($highlight,
                $left + 11 * $s + $tilt * 0.4, 4 * $s,
                $left + 8 * $s, 15 * $s)
        }
        finally {
            $fill.Dispose()
            $outline.Dispose()
            $highlight.Dispose()
        }
    }
    Save-Downsampled $largeShards 96 24 (Join-Path $particleRoot 'FrostMineShards.png')
}
finally {
    $shardGraphics.Dispose()
    $largeShards.Dispose()
}

# 单张雪花冲击环与碎晶分层播放，避免爆炸只像普通蓝色烟雾。
$largePulse = New-ArgbBitmap (64 * $particleScale) (64 * $particleScale)
$pulseGraphics = [System.Drawing.Graphics]::FromImage($largePulse)
try {
    $pulseGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $s = [float]$particleScale
    $ring = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(190, 93, 224, 255), 2.4 * $s)
    $snowflake = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(235, 224, 252, 255), 2.1 * $s)
    try {
        $pulseGraphics.DrawEllipse($ring, 5 * $s, 5 * $s, 54 * $s, 54 * $s)
        foreach ($angle in @(0.0, 60.0, 120.0)) {
            $radians = $angle * [Math]::PI / 180.0
            $dx = [float]([Math]::Cos($radians) * 24 * $s)
            $dy = [float]([Math]::Sin($radians) * 24 * $s)
            $pulseGraphics.DrawLine($snowflake, 32 * $s - $dx, 32 * $s - $dy,
                32 * $s + $dx, 32 * $s + $dy)
        }
    }
    finally {
        $ring.Dispose()
        $snowflake.Dispose()
    }
    Save-Downsampled $largePulse 64 64 (Join-Path $particleRoot 'FrostMinePulse.png')
}
finally {
    $pulseGraphics.Dispose()
    $largePulse.Dispose()
}

$reanim = @'
<fps>12</fps>
<track>
<name>anim_idle</name>
<t></t><t></t><t></t><t></t><t></t><t></t><t></t><t></t>
<t></t><t></t><t></t><t></t><t></t><t></t><t></t><t></t>
</track>
<track>
<name>FrostMine_body</name>
<t><x>8</x><y>8</y><sx>0.840</sx><sy>0.840</sy><i>IMAGE_REANIM_FROSTMINE_DORMANT</i></t>
<t><y>7.7</y><sy>0.842</sy></t><t><y>7.4</y><sy>0.844</sy></t><t><y>7.1</y><sy>0.846</sy></t>
<t><y>6.8</y><sy>0.848</sy></t><t><y>7.1</y><sy>0.846</sy></t><t><y>7.4</y><sy>0.844</sy></t>
<t><y>7.7</y><sy>0.842</sy></t><t><y>8</y><sy>0.840</sy></t><t><y>8.2</y><sy>0.839</sy></t>
<t><y>8.4</y><sy>0.838</sy></t><t><y>8.6</y><sy>0.837</sy></t><t><y>8.8</y><sy>0.836</sy></t>
<t><y>8.6</y><sy>0.837</sy></t><t><y>8.4</y><sy>0.838</sy></t><t><y>8.2</y><sy>0.839</sy></t>
</track>
'@
Set-Content -LiteralPath (Join-Path $reanimRoot 'FrostMine.reanim') `
    -Value $reanim -Encoding utf8NoBOM -NoNewline

$outputHashes = Get-FileHash -Algorithm SHA256 -LiteralPath @(
    (Join-Path $reanimImageRoot 'REANIM_FROSTMINE_DORMANT.png'),
    (Join-Path $reanimImageRoot 'REANIM_FROSTMINE_CALIBRATED.png'),
    (Join-Path $reanimImageRoot 'REANIM_FROSTMINE_ARMED.png'),
    (Join-Path $cardRoot 'FrostMine.png'),
    (Join-Path $particleRoot 'FrostMineShards.png'),
    (Join-Path $particleRoot 'FrostMinePulse.png'),
    (Join-Path $reanimRoot 'FrostMine.reanim')
)
foreach ($outputHash in $outputHashes) {
    $fileName = Split-Path -Leaf $outputHash.Path
    if ($outputHash.Hash -ne $expectedOutputHashes[$fileName]) {
        throw "伏霜雷生成资源哈希漂移：$fileName。"
    }
    "{0}  {1}" -f $outputHash.Hash, $outputHash.Path
}
