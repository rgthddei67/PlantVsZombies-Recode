param(
    [string]$ResourceRoot = (Join-Path $PSScriptRoot "../build/clang-release/resources")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sourcePath = Join-Path $PSScriptRoot "assets/ice_crack_drill_source.png"
$imageRoot = Join-Path $ResourceRoot "image/reanim"
$particleRoot = Join-Path $ResourceRoot "particles"

function New-TransparentBitmap {
    param([int]$Width, [int]$Height)
    return [System.Drawing.Bitmap]::new(
        $Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function New-DrillRigFrame {
    param(
        [System.Drawing.Bitmap]$Base,
        [int]$DamageStage,
        [int]$SpinPhase
    )

    $frame = New-TransparentBitmap -Width $Base.Width -Height $Base.Height
    $graphics = [System.Drawing.Graphics]::FromImage($frame)
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Base, 0, 0)

        # 四相高光沿螺旋钻齿横向错位；慢播是巡航，快播时形成明确旋转感。
        $highlight = [System.Drawing.Pen]::new(
            [System.Drawing.Color]::FromArgb(215, 220, 252, 255), 3.2)
        $shadow = [System.Drawing.Pen]::new(
            [System.Drawing.Color]::FromArgb(150, 25, 92, 145), 2.0)
        try {
            $phaseOffset = @(0, 5, 10, 15)[$SpinPhase]
            foreach ($x in @(20, 42, 66, 90)) {
                $arcX = $x + $phaseOffset
                if ($arcX -gt 101) { $arcX -= 82 }
                $graphics.DrawArc($shadow, $arcX, 35, 21, 50, 78, 190)
                $graphics.DrawArc($highlight, $arcX + 1, 34, 19, 48, 78, 118)
            }
        }
        finally {
            $highlight.Dispose()
            $shadow.Dispose()
        }

        # 蓄力循环同一组帧还会让飞轮中心上下偏摆，低分辨率下仍能读出机械运转。
        $wheelPulse = [System.Drawing.Pen]::new(
            [System.Drawing.Color]::FromArgb(190, 255, 174, 38), 2.2)
        try {
            $pulse = @(0, 2, 0, -2)[$SpinPhase]
            $graphics.DrawEllipse($wheelPulse, 127 + $pulse, 52, 23, 23)
        }
        finally { $wheelPulse.Dispose() }

        if ($DamageStage -ge 2) {
            $crack = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(240, 29, 54, 66), 2.4)
            try {
                $graphics.DrawLines($crack, [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(109, 25),
                    [System.Drawing.PointF]::new(101, 39),
                    [System.Drawing.PointF]::new(111, 48),
                    [System.Drawing.PointF]::new(103, 61)))
                $graphics.DrawLines($crack, [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(174, 30),
                    [System.Drawing.PointF]::new(164, 44),
                    [System.Drawing.PointF]::new(173, 57)))
            }
            finally { $crack.Dispose() }
        }

        if ($DamageStage -ge 3) {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $clear = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::Transparent)
            try {
                $graphics.FillPolygon($clear, [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(74, 24),
                    [System.Drawing.PointF]::new(88, 31),
                    [System.Drawing.PointF]::new(80, 47),
                    [System.Drawing.PointF]::new(69, 39)))
                $graphics.FillPolygon($clear, [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(184, 83),
                    [System.Drawing.PointF]::new(201, 78),
                    [System.Drawing.PointF]::new(205, 97),
                    [System.Drawing.PointF]::new(189, 101)))
            }
            finally { $clear.Dispose() }
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver

            $spark = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(235, 255, 139, 28), 2.2)
            try {
                $graphics.DrawLine($spark, 137, 27, 143, 19)
                $graphics.DrawLine($spark, 143, 28, 152, 25)
                $graphics.DrawLine($spark, 140, 30, 146, 37)
            }
            finally { $spark.Dispose() }
        }
    }
    finally { $graphics.Dispose() }
    return $frame
}

function New-DrillHelmet {
    param([int]$Stage)

    $hatForwardOffsetX = -3 # 左行僵尸的脸侧前移量，单位实机贴图 px
    $suffix = if ($Stage -eq 1) { "" } else { [string]$Stage }
    $source = [System.Drawing.Bitmap]::new(
        (Join-Path $imageRoot ("Zombie_digger_hardhat{0}.png" -f $suffix)))
    # 矿工帽原图服务于向右行走的矿工；本品种向左行走，帽檐必须镜像到脸侧。
    $source.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX)
    $target = New-TransparentBitmap -Width 59 -Height 57
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($target)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

            $matrix = [System.Drawing.Imaging.ColorMatrix]::new()
            $matrix.Matrix00 = 0.48; $matrix.Matrix01 = 0.12; $matrix.Matrix02 = 0.08
            $matrix.Matrix10 = 0.18; $matrix.Matrix11 = 0.72; $matrix.Matrix12 = 0.14
            $matrix.Matrix20 = 0.28; $matrix.Matrix21 = 0.48; $matrix.Matrix22 = 1.05
            $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
            try {
                $attributes.SetColorMatrix($matrix)
                $graphics.DrawImage($source,
                    [System.Drawing.Rectangle]::new($hatForwardOffsetX, 13, 59, 39),
                    0, 0, $source.Width, $source.Height,
                    [System.Drawing.GraphicsUnit]::Pixel, $attributes)
            }
            finally { $attributes.Dispose() }

            $outline = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(255, 64, 33, 17), 4.6)
            $earmuff = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(255, 238, 99, 16))
            $shine = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(230, 255, 203, 76), 1.5)
            try {
                $graphics.DrawArc($outline, 7 + $hatForwardOffsetX, 27, 45, 18, 176, 188)
                $graphics.FillEllipse($earmuff, 5 + $hatForwardOffsetX, 31, 10, 14)
                $graphics.FillEllipse($earmuff, 44 + $hatForwardOffsetX, 31, 10, 14)
                $graphics.DrawArc($shine, 7 + $hatForwardOffsetX, 33, 6, 8, 90, 160)
                $graphics.DrawArc($shine, 46 + $hatForwardOffsetX, 33, 6, 8, 270, 160)
            }
            finally {
                $outline.Dispose(); $earmuff.Dispose(); $shine.Dispose()
            }
        }
        finally { $graphics.Dispose() }
        $target.Save((Join-Path $imageRoot ("Zombie_icecrack_drill_helmet{0}.png" -f $Stage)),
            [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $target.Dispose()
        $source.Dispose()
    }
}

function New-GroundRiftTexture {
    $scale = 4
    $large = New-TransparentBitmap -Width (118 * $scale) -Height (32 * $scale)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($large)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.ScaleTransform($scale, $scale)
            $dark = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(245, 30, 52, 73), 8.0)
            $ice = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(245, 65, 171, 223), 4.5)
            $shine = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(235, 218, 251, 255), 1.7)
            try {
                $points = [System.Drawing.PointF[]]@(
                    [System.Drawing.PointF]::new(3, 22),
                    [System.Drawing.PointF]::new(16, 17),
                    [System.Drawing.PointF]::new(27, 24),
                    [System.Drawing.PointF]::new(39, 9),
                    [System.Drawing.PointF]::new(52, 19),
                    [System.Drawing.PointF]::new(66, 8),
                    [System.Drawing.PointF]::new(78, 23),
                    [System.Drawing.PointF]::new(91, 13),
                    [System.Drawing.PointF]::new(115, 19))
                $graphics.DrawLines($dark, $points)
                $graphics.DrawLines($ice, $points)
                $graphics.DrawLines($shine, $points)
                $graphics.DrawLine($ice, 39, 9, 34, 2)
                $graphics.DrawLine($ice, 66, 8, 70, 1)
                $graphics.DrawLine($ice, 91, 13, 98, 5)
            }
            finally { $dark.Dispose(); $ice.Dispose(); $shine.Dispose() }
        }
        finally { $graphics.Dispose() }

        $small = New-TransparentBitmap -Width 118 -Height 32
        try {
            $down = [System.Drawing.Graphics]::FromImage($small)
            try {
                $down.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $down.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $down.DrawImage($large, 0, 0, 118, 32)
            }
            finally { $down.Dispose() }
            $small.Save((Join-Path $imageRoot "IceCrackDrillRift.png"),
                [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $small.Dispose() }
    }
    finally { $large.Dispose() }
}

function New-DrillShardAtlas {
    $atlas = New-TransparentBitmap -Width 128 -Height 32
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($atlas)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $edge = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(245, 28, 91, 145))
            $ice = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(240, 99, 205, 241))
            $shine = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(240, 225, 253, 255), 1.4)
            try {
                $shapes = @(
                    @(@(4,27),@(12,3),@(25,20),@(20,29)),
                    @(@(38,27),@(46,8),@(58,4),@(52,29)),
                    @(@(68,24),@(78,2),@(91,19),@(84,30)),
                    @(@(100,29),@(107,5),@(124,13),@(116,27)))
                foreach ($shape in $shapes) {
                    $points = [System.Drawing.PointF[]]@($shape | ForEach-Object {
                        [System.Drawing.PointF]::new($_[0], $_[1])
                    })
                    $graphics.FillPolygon($edge, $points)
                    $inner = [System.Drawing.PointF[]]@($points | ForEach-Object {
                        [System.Drawing.PointF]::new($_.X + 1.5, $_.Y + 1.5)
                    })
                    $graphics.FillPolygon($ice, $inner)
                    $graphics.DrawLine($shine, $inner[0], $inner[1])
                }
            }
            finally { $edge.Dispose(); $ice.Dispose(); $shine.Dispose() }
        }
        finally { $graphics.Dispose() }
        $atlas.Save((Join-Path $particleRoot "IceCrackDrillShards.png"),
            [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $atlas.Dispose() }
}

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "缺少冰裂钻机源图: $sourcePath"
}
New-Item -ItemType Directory -Force -Path $imageRoot, $particleRoot | Out-Null

$source = [System.Drawing.Bitmap]::new($sourcePath)
$base = New-TransparentBitmap -Width 220 -Height 124
try {
    $graphics = [System.Drawing.Graphics]::FromImage($base)
    try {
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source,
            [System.Drawing.Rectangle]::new(0, 0, 220, 124),
            33, 10, 1626, 916, [System.Drawing.GraphicsUnit]::Pixel)
    }
    finally { $graphics.Dispose() }

    foreach ($stage in 1..3) {
        foreach ($phase in 0..3) {
            $frame = New-DrillRigFrame -Base $base -DamageStage $stage -SpinPhase $phase
            try {
                $frame.Save((Join-Path $imageRoot (
                    "Zombie_icecrack_drill_rig{0}_spin{1}.png" -f $stage, $phase)),
                    [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally { $frame.Dispose() }
        }
    }
}
finally {
    $base.Dispose()
    $source.Dispose()
}

foreach ($stage in 1..3) { New-DrillHelmet -Stage $stage }
New-GroundRiftTexture
New-DrillShardAtlas

$generated = Get-ChildItem -LiteralPath $imageRoot -Filter "*icecrack_drill*" |
    Sort-Object Name
$generated += Get-Item -LiteralPath (Join-Path $particleRoot "IceCrackDrillShards.png")
$generated | Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "{0}  {1}" -f $_.Hash, $_.Path }
