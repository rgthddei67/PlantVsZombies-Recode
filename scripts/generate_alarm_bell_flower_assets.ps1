param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$systemDrawingAssembly = [System.Drawing.Bitmap].Assembly.Location
$systemDrawingPrimitivesAssembly = [System.Drawing.Rectangle].Assembly.Location
$windowsDrawingAssemblies = [AppDomain]::CurrentDomain.GetAssemblies() | Where-Object {
    $_.GetName().Name -like 'System.Private.Windows*'
} | ForEach-Object { $_.Location }
Add-Type -ReferencedAssemblies @(
    $systemDrawingAssembly, $systemDrawingPrimitivesAssembly
    $windowsDrawingAssemblies
) -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class AlarmBellRaster
{
    private static bool IsConnectedBackground(byte b, byte g, byte r)
    {
        int minimum = Math.Min(r, Math.Min(g, b));
        int maximum = Math.Max(r, Math.Max(g, b));
        return minimum >= 215 && maximum - minimum <= 18;
    }

    public static Bitmap RemoveConnectedCheckerboard(Bitmap source)
    {
        var result = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
        using (Graphics graphics = Graphics.FromImage(result)) {
            graphics.DrawImageUnscaled(source, 0, 0);
        }

        Rectangle bounds = new Rectangle(0, 0, result.Width, result.Height);
        BitmapData data = result.LockBits(bounds, ImageLockMode.ReadWrite,
            PixelFormat.Format32bppArgb);
        try {
            int stride = data.Stride;
            byte[] pixels = new byte[stride * result.Height];
            Marshal.Copy(data.Scan0, pixels, 0, pixels.Length);
            bool[] visited = new bool[result.Width * result.Height];
            int[] queue = new int[visited.Length];
            int head = 0;
            int tail = 0;

            Action<int, int> enqueue = (x, y) => {
                int index = y * result.Width + x;
                if (visited[index]) return;
                int offset = y * stride + x * 4;
                if (!IsConnectedBackground(pixels[offset], pixels[offset + 1],
                        pixels[offset + 2])) return;
                visited[index] = true;
                queue[tail++] = index;
            };

            for (int x = 0; x < result.Width; ++x) {
                enqueue(x, 0);
                enqueue(x, result.Height - 1);
            }
            for (int y = 1; y + 1 < result.Height; ++y) {
                enqueue(0, y);
                enqueue(result.Width - 1, y);
            }

            while (head < tail) {
                int index = queue[head++];
                int x = index % result.Width;
                int y = index / result.Width;
                if (x > 0) enqueue(x - 1, y);
                if (x + 1 < result.Width) enqueue(x + 1, y);
                if (y > 0) enqueue(x, y - 1);
                if (y + 1 < result.Height) enqueue(x, y + 1);
            }

            for (int index = 0; index < visited.Length; ++index) {
                if (!visited[index]) continue;
                int y = index / result.Width;
                int x = index - y * result.Width;
                pixels[y * stride + x * 4 + 3] = 0;
            }
            Marshal.Copy(pixels, 0, data.Scan0, pixels.Length);
        }
        finally {
            result.UnlockBits(data);
        }
        return result;
    }

    public static Rectangle FindAlphaBounds(Bitmap bitmap, Rectangle search)
    {
        int left = search.Right;
        int top = search.Bottom;
        int right = search.Left - 1;
        int bottom = search.Top - 1;
        for (int y = search.Top; y < search.Bottom; ++y) {
            for (int x = search.Left; x < search.Right; ++x) {
                if (bitmap.GetPixel(x, y).A == 0) continue;
                left = Math.Min(left, x);
                top = Math.Min(top, y);
                right = Math.Max(right, x);
                bottom = Math.Max(bottom, y);
            }
        }
        return right < left ? Rectangle.Empty
            : Rectangle.FromLTRB(left, top, right + 1, bottom + 1);
    }
}
'@

$sourcePath = Join-Path $RepositoryRoot 'docs/art/alarm-bell-flower/alarm-bell-flower-rig-parts-v1.png'
$bloverReanimPath = Join-Path $RepositoryRoot 'build/clang-release/resources/reanim/Blover.reanim'
$resourceRoot = Join-Path $RepositoryRoot 'build/clang-release/resources'
$reanimImageRoot = Join-Path $resourceRoot 'image/reanim'
$cardRoot = Join-Path $resourceRoot 'image/PlantImage'
$particleRoot = Join-Path $resourceRoot 'particles'
$reanimRoot = Join-Path $resourceRoot 'reanim'
$expectedSourceHash = 'E3FC4BAF664C3817ABAD4EC7B1D03A8DF6B66F524BE4DC26A950110165EA5BE5'
$expectedBloverReanimHash = 'A80928B199711A92D8B856EEE748060CEA79841CFE88D7D093F4F80DA04E8C37'
$expectedOutputHashes = @{
    'REANIM_ALARMBELLFLOWER_HEAD_READY.png' = '14AF58F1AD16AB2D59F6261CEB95FC864C45ADA8FB2EA1509DD4A17E821388EB'
    'REANIM_ALARMBELLFLOWER_HEAD_RINGING.png' = '203CCB32AA8955C921372EC6A0D7B96A71F055A378666625A2ADA212974FF1E7'
    'REANIM_ALARMBELLFLOWER_HEAD_FADING.png' = '1F7E5B86BFB4699877257D40FA75EBFD2A4230D91852F1CADB9D8B48E308B027'
    'REANIM_ALARMBELLFLOWER_CLAPPER.png' = 'E5C442EA967F8F0336D44B2F7BE2B676A5400C2FDD91A719A9C6164FA529C524'
    'REANIM_ALARMBELLFLOWER_BASE.png' = '95E53E83BEE015A207961542BC3E97399EE671D4CE90F455EC4629B3B095C70F'
    'AlarmBellFlower.png' = 'D258DB6D8DD072D47AD215CE02D7902E355D80A42ED56E5B67F0DBE3F547762E'
    'AlarmBellRowPulse.png' = '08B8269711D5A9B422A802A0B3B7D17FDD0E8368D54B3C716894A80A11414254'
    'AlarmBellFlower.reanim' = 'A80928B199711A92D8B856EEE748060CEA79841CFE88D7D093F4F80DA04E8C37'
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

function Save-RigPart(
    [System.Drawing.Bitmap]$Atlas,
    [System.Drawing.Rectangle]$SearchBounds,
    [System.Drawing.Rectangle]$DestinationBounds,
    [string]$DestinationPath
) {
    $sourceBounds = [AlarmBellRaster]::FindAlphaBounds($Atlas, $SearchBounds)
    if ($sourceBounds.IsEmpty) {
        throw "警铃草分件区没有前景像素：$SearchBounds。"
    }

    $canvas = New-ArgbBitmap 120 120
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    try {
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Atlas, $DestinationBounds, $sourceBounds,
            [System.Drawing.GraphicsUnit]::Pixel)
        $canvas.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $canvas.Dispose()
    }
}

if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash -ne $expectedSourceHash) {
    throw '警铃草 ImageGen 源图哈希不匹配，拒绝静默生成漂移资源。'
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $bloverReanimPath).Hash -ne $expectedBloverReanimHash) {
    throw '三叶草 reanim 参考哈希不匹配，拒绝在未知时间轴上生成警铃草动画。'
}

$sourceAtlas = [System.Drawing.Bitmap]::new($sourcePath)
try {
    if ($sourceAtlas.Width -ne 1817 -or $sourceAtlas.Height -ne 866) {
        throw "警铃草分件源图尺寸应为 1817x866，实际为 $($sourceAtlas.Width)x$($sourceAtlas.Height)。"
    }
    $cleanAtlas = [AlarmBellRaster]::RemoveConnectedCheckerboard($sourceAtlas)
    try {
        # 三种头图按铃身底部的茎秆插口对齐，而不是按不规则外轮廓对齐；右倾的
        # 响铃/疲惫头比正面头左移 10px，令语义连接点保持在同一骨骼轴心。
        Save-RigPart $cleanAtlas ([System.Drawing.Rectangle]::new(0, 0, 362, 866)) `
            ([System.Drawing.Rectangle]::new(25, 3, 70, 61)) `
            (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_READY.png')
        Save-RigPart $cleanAtlas ([System.Drawing.Rectangle]::new(362, 0, 308, 866)) `
            ([System.Drawing.Rectangle]::new(15, 3, 70, 61)) `
            (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_RINGING.png')
        Save-RigPart $cleanAtlas ([System.Drawing.Rectangle]::new(670, 0, 300, 866)) `
            ([System.Drawing.Rectangle]::new(15, 3, 70, 61)) `
            (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_FADING.png')
        Save-RigPart $cleanAtlas ([System.Drawing.Rectangle]::new(970, 0, 120, 866)) `
            ([System.Drawing.Rectangle]::new(52, 48, 16, 38)) `
            (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_CLAPPER.png')
        $basePath = Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_BASE.png'
        Save-RigPart $cleanAtlas ([System.Drawing.Rectangle]::new(1090, 0, 355, 866)) `
            ([System.Drawing.Rectangle]::new(20, 48, 80, 65)) $basePath
        # 地面轨只保留叶座；上方茎段交回 Blover_stem2/stem1 原版骨骼，避免头部摆动时断颈。
        $fullBase = [System.Drawing.Bitmap]::new($basePath)
        $leafBase = New-ArgbBitmap 120 120
        $leafGraphics = [System.Drawing.Graphics]::FromImage($leafBase)
        try {
            $leafGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $leafGraphics.DrawImage($fullBase, [System.Drawing.Rectangle]::new(0, 66, 120, 54),
                [System.Drawing.Rectangle]::new(0, 66, 120, 54),
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $leafGraphics.Dispose()
            $fullBase.Dispose()
        }
        try {
            $leafBase.Save($basePath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $leafBase.Dispose()
        }

        # 卡图仍是 120px，角色保持主人确认的 0.8 构图（108→86px），但改用分件图集的完整参考态。
        $referenceBounds = [AlarmBellRaster]::FindAlphaBounds($cleanAtlas,
            [System.Drawing.Rectangle]::new(1445, 0, 372, 866))
        if ($referenceBounds.IsEmpty) {
            throw '警铃草完整参考态没有前景像素。'
        }
        $card = New-ArgbBitmap 120 120
        $cardGraphics = [System.Drawing.Graphics]::FromImage($card)
        try {
            $cardGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $cardGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $cardGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $cardGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $cardGraphics.DrawImage($cleanAtlas, [System.Drawing.Rectangle]::new(17, 17, 86, 86),
                $referenceBounds, [System.Drawing.GraphicsUnit]::Pixel)
            $card.Save((Join-Path $cardRoot 'AlarmBellFlower.png'),
                [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $cardGraphics.Dispose()
            $card.Dispose()
        }
    }
    finally {
        $cleanAtlas.Dispose()
    }
}
finally {
    $sourceAtlas.Dispose()
}

# 金青双色整行声波：高分辨率绘制后缩小，保留柔和光晕、波峰和等距铃音火花。
$particleScale = 4
$largePulse = New-ArgbBitmap (720 * $particleScale) (96 * $particleScale)
$pulseGraphics = [System.Drawing.Graphics]::FromImage($largePulse)
try {
    $pulseGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $pulseGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $s = [float]$particleScale
    $centerY = 48.0 * $s

    foreach ($wave in @(
        @{ Amplitude = 22.0; Cycles = 7.0; Color = [System.Drawing.Color]::FromArgb(200, 73, 232, 255); Width = 2.8 },
        @{ Amplitude = 13.0; Cycles = 9.0; Color = [System.Drawing.Color]::FromArgb(180, 255, 205, 66); Width = 2.2 }
    )) {
        $points = [System.Collections.Generic.List[System.Drawing.PointF]]::new()
        for ($x = 18; $x -le 702; $x += 3) {
            $phase = (($x - 18.0) / 684.0) * $wave.Cycles * 2.0 * [Math]::PI
            $envelope = [Math]::Sin((($x - 18.0) / 684.0) * [Math]::PI)
            $y = $centerY + [Math]::Sin($phase) * $wave.Amplitude * $envelope * $s
            $points.Add([System.Drawing.PointF]::new([float]($x * $s), [float]$y))
        }
        $glowPen = [System.Drawing.Pen]::new(
            [System.Drawing.Color]::FromArgb(58, $wave.Color.R, $wave.Color.G, $wave.Color.B),
            [float]($wave.Width * 3.2 * $s))
        $corePen = [System.Drawing.Pen]::new($wave.Color, [float]($wave.Width * $s))
        try {
            $pulseGraphics.DrawLines($glowPen, $points.ToArray())
            $pulseGraphics.DrawLines($corePen, $points.ToArray())
        }
        finally {
            $glowPen.Dispose()
            $corePen.Dispose()
        }
    }

    $centerGlow = [System.Drawing.Pen]::new(
        [System.Drawing.Color]::FromArgb(82, 225, 252, 255), 5.0 * $s)
    $centerCore = [System.Drawing.Pen]::new(
        [System.Drawing.Color]::FromArgb(210, 255, 244, 174), 1.2 * $s)
    try {
        $pulseGraphics.DrawLine($centerGlow, 12 * $s, $centerY, 708 * $s, $centerY)
        $pulseGraphics.DrawLine($centerCore, 12 * $s, $centerY, 708 * $s, $centerY)
    }
    finally {
        $centerGlow.Dispose()
        $centerCore.Dispose()
    }

    for ($x = 72; $x -le 648; $x += 72) {
        $size = (($x / 72) % 2 -eq 0) ? 5.0 : 3.8
        $points = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new([float]($x * $s), [float](($centerY / $s - $size) * $s)),
            [System.Drawing.PointF]::new([float](($x + $size) * $s), [float]$centerY),
            [System.Drawing.PointF]::new([float]($x * $s), [float](($centerY / $s + $size) * $s)),
            [System.Drawing.PointF]::new([float](($x - $size) * $s), [float]$centerY))
        $fill = [System.Drawing.SolidBrush]::new(
            [System.Drawing.Color]::FromArgb(220, 218, 253, 255))
        $outline = [System.Drawing.Pen]::new(
            [System.Drawing.Color]::FromArgb(220, 32, 105, 157), 1.1 * $s)
        try {
            $pulseGraphics.FillPolygon($fill, $points)
            $pulseGraphics.DrawPolygon($outline, $points)
        }
        finally {
            $fill.Dispose()
            $outline.Dispose()
        }
    }

    Save-Downsampled $largePulse 720 96 (Join-Path $particleRoot 'AlarmBellRowPulse.png')
}
finally {
    $pulseGraphics.Dispose()
    $largePulse.Dispose()
}

# 直接复用原版三叶草的 idle/blow/loop 时间轴；原版双段茎负责连接，地面轨与头轨换成警铃草分件。
Copy-Item -LiteralPath $bloverReanimPath `
    -Destination (Join-Path $reanimRoot 'AlarmBellFlower.reanim') -Force

$outputs = @(
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_READY.png'),
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_RINGING.png'),
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_HEAD_FADING.png'),
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_CLAPPER.png'),
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_BASE.png'),
    (Join-Path $cardRoot 'AlarmBellFlower.png'),
    (Join-Path $particleRoot 'AlarmBellRowPulse.png'),
    (Join-Path $reanimRoot 'AlarmBellFlower.reanim')
)
$outputHashes = Get-FileHash -Algorithm SHA256 -LiteralPath $outputs
$driftedOutputs = [System.Collections.Generic.List[string]]::new()
foreach ($outputHash in $outputHashes) {
    $fileName = Split-Path -Leaf $outputHash.Path
    "{0}  {1}" -f $outputHash.Hash, $outputHash.Path
    if ($outputHash.Hash -ne $expectedOutputHashes[$fileName]) {
        $driftedOutputs.Add($fileName)
    }
}
if ($driftedOutputs.Count -gt 0) {
    throw "警铃草生成资源哈希漂移：$($driftedOutputs -join ', ')。"
}
