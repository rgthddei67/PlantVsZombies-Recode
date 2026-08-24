param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$resourceRoot = Join-Path $RepositoryRoot 'build/clang-release/resources'
$sourceImageRoot = Join-Path $resourceRoot 'image/reanim'
$targetImageRoot = $sourceImageRoot
$cardRoot = Join-Path $resourceRoot 'image/PlantImage'
$reanimRoot = Join-Path $resourceRoot 'reanim'
$scale = 4

function New-ArgbBitmap([int]$width, [int]$height) {
    return [System.Drawing.Bitmap]::new(
        $width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function New-RecoloredNut([string]$sourcePath) {
    $source = [System.Drawing.Bitmap]::new($sourcePath)
    try {
        $large = New-ArgbBitmap ($source.Width * $scale) ($source.Height * $scale)
        $graphics = [System.Drawing.Graphics]::FromImage($large)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($source, 0, 0, $large.Width, $large.Height)
        }
        finally {
            $graphics.Dispose()
        }

        for ($y = 0; $y -lt $large.Height; ++$y) {
            for ($x = 0; $x -lt $large.Width; ++$x) {
                $pixel = $large.GetPixel($x, $y)
                if ($pixel.A -eq 0) { continue }

                $maximum = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                $minimum = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $luminance = (0.299 * $pixel.R) + (0.587 * $pixel.G) + (0.114 * $pixel.B)

                # 保留眼白、高光与黑色五官；只把坚果棕色材质映成冷青蓝。
                if (($maximum - $minimum) -lt 28 -and $luminance -gt 145) { continue }
                if ($luminance -lt 34) {
                    $r = [int][Math]::Round($luminance * 0.58)
                    $g = [int][Math]::Round($luminance * 0.78)
                    $b = [int][Math]::Round($luminance * 0.92)
                }
                else {
                    $r = [int][Math]::Min(255, 20 + $luminance * 0.46)
                    $g = [int][Math]::Min(255, 48 + $luminance * 0.66)
                    $b = [int][Math]::Min(255, 64 + $luminance * 0.78)
                }
                $large.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $pixel.A, $r, $g, $b))
            }
        }
        return $large
    }
    finally {
        $source.Dispose()
    }
}

function Add-AnchorHardware(
    [System.Drawing.Bitmap]$bitmap,
    [bool]$braced
) {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $s = [float]$scale

        if ($braced) {
            $outline = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(235, 18, 44, 60), 3.2 * $s)
            $fill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(245, 112, 205, 226))
            try {
                $leftSpike = [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(28 * $s, 73 * $s),
                    [System.Drawing.PointF]::new(4 * $s, 96 * $s),
                    [System.Drawing.PointF]::new(37 * $s, 87 * $s))
                $rightSpike = [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(72 * $s, 73 * $s),
                    [System.Drawing.PointF]::new(96 * $s, 96 * $s),
                    [System.Drawing.PointF]::new(63 * $s, 87 * $s))
                $graphics.FillPolygon($fill, $leftSpike)
                $graphics.DrawPolygon($outline, $leftSpike)
                $graphics.FillPolygon($fill, $rightSpike)
                $graphics.DrawPolygon($outline, $rightSpike)
                $graphics.DrawLine($outline, 26 * $s, 82 * $s, 12 * $s, 99 * $s)
                $graphics.DrawLine($outline, 74 * $s, 82 * $s, 88 * $s, 99 * $s)
            }
            finally {
                $outline.Dispose()
                $fill.Dispose()
            }
        }

        # 深色轮廓配浅蓝钢件，让最终 100px 贴图仍能辨认“锚”而不是普通雪帽。
        $dark = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(245, 17, 39, 52), 5.0 * $s)
        $steel = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 132, 210, 226), 2.4 * $s)
        $ringFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 104, 187, 207))
        try {
            $graphics.DrawEllipse($dark, 43 * $s, 59 * $s, 14 * $s, 14 * $s)
            $graphics.FillEllipse($ringFill, 44.5 * $s, 60.5 * $s, 11 * $s, 11 * $s)
            $graphics.DrawLine($dark, 50 * $s, 70 * $s, 50 * $s, 88 * $s)
            $graphics.DrawLine($dark, 39 * $s, 77 * $s, 61 * $s, 77 * $s)
            $graphics.DrawArc($dark, 35 * $s, 72 * $s, 30 * $s, 22 * $s, 5, 170)
            $graphics.DrawLine($dark, 36 * $s, 83 * $s, 31 * $s, 78 * $s)
            $graphics.DrawLine($dark, 64 * $s, 83 * $s, 69 * $s, 78 * $s)

            $graphics.DrawEllipse($steel, 43 * $s, 59 * $s, 14 * $s, 14 * $s)
            $graphics.DrawLine($steel, 50 * $s, 70 * $s, 50 * $s, 88 * $s)
            $graphics.DrawLine($steel, 39 * $s, 77 * $s, 61 * $s, 77 * $s)
            $graphics.DrawArc($steel, 35 * $s, 72 * $s, 30 * $s, 22 * $s, 5, 170)
        }
        finally {
            $dark.Dispose()
            $steel.Dispose()
            $ringFill.Dispose()
        }

        if ($braced) {
            $icePen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(220, 218, 250, 255), 1.8 * $s)
            try {
                $graphics.DrawLine($icePen, 19 * $s, 28 * $s, 34 * $s, 20 * $s)
                $graphics.DrawLine($icePen, 66 * $s, 21 * $s, 82 * $s, 32 * $s)
                $graphics.DrawLine($icePen, 22 * $s, 41 * $s, 31 * $s, 47 * $s)
            }
            finally {
                $icePen.Dispose()
            }
        }
    }
    finally {
        $graphics.Dispose()
    }
}

function Save-Downsampled(
    [System.Drawing.Bitmap]$large,
    [string]$targetPath
) {
    $small = New-ArgbBitmap 100 100
    $graphics = [System.Drawing.Graphics]::FromImage($small)
    try {
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($large, 0, 0, 100, 100)
        $small.Save($targetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $small.Dispose()
    }
}

$sourceVariants = @(
    @{ Source = 'Wallnut_body.png'; Suffix = 'body' },
    @{ Source = 'Wallnut_cracked1.png'; Suffix = 'cracked1' },
    @{ Source = 'Wallnut_cracked2.png'; Suffix = 'cracked2' }
)

foreach ($variant in $sourceVariants) {
    foreach ($braced in @($false, $true)) {
        $large = New-RecoloredNut (Join-Path $sourceImageRoot $variant.Source)
        try {
            Add-AnchorHardware $large $braced
            $prefix = if ($braced) { 'SnowAnchorNut_braced_' } else { 'SnowAnchorNut_' }
            Save-Downsampled $large (Join-Path $targetImageRoot ($prefix + $variant.Suffix + '.png'))
        }
        finally {
            $large.Dispose()
        }
    }
}

# 卡图沿用原版 120×120 透明画布和主体尺度，使用展开姿态明确传达玩法身份。
$card = New-ArgbBitmap 120 120
$bracedBody = [System.Drawing.Bitmap]::new((Join-Path $targetImageRoot 'SnowAnchorNut_braced_body.png'))
$cardGraphics = [System.Drawing.Graphics]::FromImage($card)
try {
    $cardGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $cardGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $cardGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $cardGraphics.DrawImage($bracedBody, 25, 22, 70, 70)
    $card.Save((Join-Path $cardRoot 'SnowAnchorNut.png'), [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $cardGraphics.Dispose()
    $bracedBody.Dispose()
    $card.Dispose()
}

$sourceReanim = Get-Content -LiteralPath (Join-Path $reanimRoot 'WallNut.reanim') -Raw
$snowAnchorReanim = $sourceReanim.Replace(
    'IMAGE_REANIM_WALLNUT_BODY', 'IMAGE_REANIM_SNOWANCHORNUT_BODY')
Set-Content -LiteralPath (Join-Path $reanimRoot 'SnowAnchorNut.reanim') `
    -Value $snowAnchorReanim -Encoding utf8NoBOM -NoNewline

Get-FileHash -Algorithm SHA256 -LiteralPath @(
    (Join-Path $cardRoot 'SnowAnchorNut.png'),
    (Join-Path $targetImageRoot 'SnowAnchorNut_body.png'),
    (Join-Path $targetImageRoot 'SnowAnchorNut_braced_body.png'),
    (Join-Path $reanimRoot 'SnowAnchorNut.reanim')
) | ForEach-Object { "{0}  {1}" -f $_.Hash, $_.Path }
