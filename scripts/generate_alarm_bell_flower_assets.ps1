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
    private static double Clamp01(double value)
    {
        return Math.Max(0.0, Math.Min(1.0, value));
    }

    private static void RgbToHsv(Color color, out double hue,
        out double saturation, out double value)
    {
        double r = color.R / 255.0;
        double g = color.G / 255.0;
        double b = color.B / 255.0;
        double maximum = Math.Max(r, Math.Max(g, b));
        double minimum = Math.Min(r, Math.Min(g, b));
        double delta = maximum - minimum;
        value = maximum;
        saturation = maximum <= 0.0 ? 0.0 : delta / maximum;
        if (delta <= 0.0) {
            hue = 0.0;
        }
        else if (maximum == r) {
            hue = 60.0 * (((g - b) / delta) % 6.0);
        }
        else if (maximum == g) {
            hue = 60.0 * (((b - r) / delta) + 2.0);
        }
        else {
            hue = 60.0 * (((r - g) / delta) + 4.0);
        }
        if (hue < 0.0) hue += 360.0;
    }

    private static Color HsvToColor(int alpha, double hue,
        double saturation, double value)
    {
        hue = (hue % 360.0 + 360.0) % 360.0;
        saturation = Clamp01(saturation);
        value = Clamp01(value);
        double chroma = value * saturation;
        double x = chroma * (1.0 - Math.Abs((hue / 60.0) % 2.0 - 1.0));
        double match = value - chroma;
        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
        if (hue < 60.0) { r = chroma; g = x; }
        else if (hue < 120.0) { r = x; g = chroma; }
        else if (hue < 180.0) { g = chroma; b = x; }
        else if (hue < 240.0) { g = x; b = chroma; }
        else if (hue < 300.0) { r = x; b = chroma; }
        else { r = chroma; b = x; }
        return Color.FromArgb(alpha,
            (int)Math.Round((r + match) * 255.0),
            (int)Math.Round((g + match) * 255.0),
            (int)Math.Round((b + match) * 255.0));
    }

    public static Bitmap RecolorGreenToWinterCyan(Bitmap source)
    {
        var result = new Bitmap(source.Width, source.Height,
            PixelFormat.Format32bppArgb);
        for (int y = 0; y < source.Height; ++y) {
            for (int x = 0; x < source.Width; ++x) {
                Color color = source.GetPixel(x, y);
                if (color.A == 0) {
                    result.SetPixel(x, y, color);
                    continue;
                }
                double hue;
                double saturation;
                double value;
                RgbToHsv(color, out hue, out saturation, out value);
                // 只改原版三叶草的绿色材质；眼白、瞳孔、描边和暖色细节保持原样。
                if (hue >= 65.0 && hue <= 175.0 && saturation >= 0.16
                    && value >= 0.12) {
                    double targetHue = 192.0 + (hue - 120.0) * 0.10;
                    double targetSaturation = Math.Max(0.58, saturation * 0.92);
                    double targetValue = Math.Min(1.0, value * 1.08);
                    color = HsvToColor(color.A, targetHue,
                        targetSaturation, targetValue);
                }
                result.SetPixel(x, y, color);
            }
        }
        return result;
    }

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
$bloverCardPath = Join-Path $RepositoryRoot 'build/clang-release/resources/image/PlantImage/Blover.png'
$resourceRoot = Join-Path $RepositoryRoot 'build/clang-release/resources'
$reanimImageRoot = Join-Path $resourceRoot 'image/reanim'
$cardRoot = Join-Path $resourceRoot 'image/PlantImage'
$particleRoot = Join-Path $resourceRoot 'particles'
$reanimRoot = Join-Path $resourceRoot 'reanim'
$expectedSourceHash = 'E3FC4BAF664C3817ABAD4EC7B1D03A8DF6B66F524BE4DC26A950110165EA5BE5'
$expectedBloverReanimHash = 'A80928B199711A92D8B856EEE748060CEA79841CFE88D7D093F4F80DA04E8C37'
$expectedBloverCardHash = 'B153C6E9D33F73A2A808C0CC88041F65EEDE5FA4596D5B603275F6485962CFF9'
$bloverPartHashes = @{
    'DIRT_BACK' = '34E8B344B4AB5133D1E8FCE862C45534A7C7C97DE86BC3551CD47DB04035B159'
    'DIRT_FRONT' = 'F58458DA1F2B6B60BCAFED46A5045899E768D3C91F05FC7C0E9678F723217E89'
    'STEM1' = '07E0DB604E761F04C4E9C30E54BA2ECC73E88B82FB40E4F5BED73DF293887F09'
    'STEM2' = '63DC77EC9B6C378DE7C05138BB7C936A50D484A09D32B0F00DBD2DA6521476F1'
    'PETAL' = '3FE020328888B35FEACA7F85A02765FF7563805241D671B6F1E015B10607AD24'
    'HEAD' = '87AB938B8BA00944F94CA207A8A094728E3952C42E6B49F073E84725EFBF9DE9'
    'HEAD2' = '01F039787503847975DC7E495F9314BEAAA27BC16ACD373E3A57D40D6E96E9F8'
}
$expectedOutputHashes = @{
    'ALARMBELLFLOWER_DIRT_BACK.png' = 'DEB227178B76D872463FE999F3A9CE2FC186FBC251F9AC5284BCFBB8E49CD4F2'
    'ALARMBELLFLOWER_DIRT_FRONT.png' = '86C1ABE19ADF3516518E0E0E51245FDCE3224A3DF54031A8DF2DA1821FC4F99A'
    'ALARMBELLFLOWER_STEM1.png' = '22356C41365DD778D1D2A1A0E4BCEAC2BBCD5570AC3BA200DC352B9252A605B0'
    'ALARMBELLFLOWER_STEM2.png' = 'D5BF588D41E7F1F37BC295957B1B1966E82D41F35AE3916C187BD50EABAFE486'
    'ALARMBELLFLOWER_PETAL.png' = '155DBE32378DB026B7E95DC240DB4C85E5C0189270CC2FC198748F8807F25B68'
    'ALARMBELLFLOWER_HEAD.png' = '65EE9E5ED00EBB9E2EFB443FAF7A0FEAF4C22D88B820E4FB7513EE6456756B78'
    'ALARMBELLFLOWER_HEAD2.png' = '49619C43F9D1A1F8AD31257C179BEB7C428621E38EF7D644A2A899912AACB86A'
    'REANIM_ALARMBELLFLOWER_CHARM.png' = '2418D3F63DD1C312A66E287D391C2547EAFAACD5EA5F04ACB356E3FDF5EE64DF'
    'AlarmBellFlower.png' = 'C84C86A13F02A03053A3A665FB3295E5BFD0A229E5356B396A36A4F468FF820A'
    'AlarmBellRowPulse.png' = '08B8269711D5A9B422A802A0B3B7D17FDD0E8368D54B3C716894A80A11414254'
    'AlarmBellFlower.reanim' = 'F96EFB74C172259AD48C3580EAF1D887E76207477118EA0D49E0B6AAA596F380'
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

function Save-ScaledPart(
    [System.Drawing.Bitmap]$Atlas,
    [System.Drawing.Rectangle]$SearchBounds,
    [int]$Width,
    [int]$Height,
    [string]$DestinationPath
) {
    $sourceBounds = [AlarmBellRaster]::FindAlphaBounds($Atlas, $SearchBounds)
    if ($sourceBounds.IsEmpty) {
        throw "警铃草分件区没有前景像素：$SearchBounds。"
    }

    $canvas = New-ArgbBitmap $Width $Height
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    try {
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Atlas, [System.Drawing.Rectangle]::new(0, 0, $Width, $Height), $sourceBounds,
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
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $bloverCardPath).Hash -ne $expectedBloverCardHash) {
    throw '三叶草卡图参考哈希不匹配，拒绝静默生成漂移资源。'
}
foreach ($part in $bloverPartHashes.GetEnumerator()) {
    $sourcePartPath = Join-Path $reanimImageRoot ("Blover_{0}.png" -f $part.Key.ToLowerInvariant())
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePartPath).Hash -ne $part.Value) {
        throw "三叶草分件 $($part.Key) 哈希不匹配，拒绝静默生成漂移资源。"
    }
}

$sourceAtlas = [System.Drawing.Bitmap]::new($sourcePath)
try {
    if ($sourceAtlas.Width -ne 1817 -or $sourceAtlas.Height -ne 866) {
        throw "警铃草分件源图尺寸应为 1817x866，实际为 $($sourceAtlas.Width)x$($sourceAtlas.Height)。"
    }
    $cleanAtlas = [AlarmBellRaster]::RemoveConnectedCheckerboard($sourceAtlas)
    try {
        # 大幅生成图不再替换原版头部；只裁成一个低分辨率小铃挂件。
        $charmPath = Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_CHARM.png'
        Save-ScaledPart $cleanAtlas ([System.Drawing.Rectangle]::new(0, 0, 362, 866)) `
            24 24 $charmPath

        # 保留三叶草全部原版分件轮廓，只把绿色材质确定性转成冬日青蓝。
        foreach ($part in $bloverPartHashes.Keys) {
            $sourcePartPath = Join-Path $reanimImageRoot ("Blover_{0}.png" -f $part.ToLowerInvariant())
            $destinationPartPath = Join-Path $reanimImageRoot ("ALARMBELLFLOWER_{0}.png" -f $part)
            $sourcePart = [System.Drawing.Bitmap]::new($sourcePartPath)
            try {
                $recoloredPart = [AlarmBellRaster]::RecolorGreenToWinterCyan($sourcePart)
                try {
                    $recoloredPart.Save($destinationPartPath,
                        [System.Drawing.Imaging.ImageFormat]::Png)
                }
                finally {
                    $recoloredPart.Dispose()
                }
            }
            finally {
                $sourcePart.Dispose()
            }
        }

        # 卡图与场上对象共享同一原版三叶草轮廓、青蓝配色和小铃身份件。
        $bloverCard = [System.Drawing.Bitmap]::new($bloverCardPath)
        try {
            $card = [AlarmBellRaster]::RecolorGreenToWinterCyan($bloverCard)
        }
        finally {
            $bloverCard.Dispose()
        }
        $cardGraphics = [System.Drawing.Graphics]::FromImage($card)
        $charm = [System.Drawing.Bitmap]::new($charmPath)
        try {
            $cardGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
            $cardGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $cardGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $cardGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $cardGraphics.DrawImage($charm, [System.Drawing.Rectangle]::new(62, 72, 22, 22))
            $card.Save((Join-Path $cardRoot 'AlarmBellFlower.png'),
                [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $charm.Dispose()
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

# 逐字保留原版三叶草的 idle/blow/loop 时间轴，只把图片键换成独立派生配色。
$alarmBellReanim = Get-Content -Raw -LiteralPath $bloverReanimPath
foreach ($part in $bloverPartHashes.Keys) {
    $alarmBellReanim = $alarmBellReanim.Replace(
        "IMAGE_REANIM_BLOVER_$part", "IMAGE_REANIM_ALARMBELLFLOWER_$part")
}
Set-Content -LiteralPath (Join-Path $reanimRoot 'AlarmBellFlower.reanim') `
    -Value $alarmBellReanim -NoNewline -Encoding utf8NoBOM

$outputs = @(
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_DIRT_BACK.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_DIRT_FRONT.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_STEM1.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_STEM2.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_PETAL.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_HEAD.png'),
    (Join-Path $reanimImageRoot 'ALARMBELLFLOWER_HEAD2.png'),
    (Join-Path $reanimImageRoot 'REANIM_ALARMBELLFLOWER_CHARM.png'),
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
