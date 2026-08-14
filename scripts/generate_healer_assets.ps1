param(
    [string]$ResourceRoot = "build/clang-release/resources"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resolvedRoot = Join-Path (Get-Location) $ResourceRoot
$reanimDirectory = Join-Path $resolvedRoot "image/reanim"
$particleDirectory = Join-Path $resolvedRoot "particles"
[System.IO.Directory]::CreateDirectory($reanimDirectory) | Out-Null
[System.IO.Directory]::CreateDirectory($particleDirectory) | Out-Null

function New-Color {
    param([int]$A, [int]$R, [int]$G, [int]$B)
    return [System.Drawing.Color]::FromArgb($A, $R, $G, $B)
}

function Save-ReducedBitmap {
    param(
        [System.Drawing.Bitmap]$Source,
        [int]$Width,
        [int]$Height,
        [string]$Path
    )
    $target = [System.Drawing.Bitmap]::new(
        $Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($target)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.DrawImage($Source, 0, 0, $Width, $Height)
    $target.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $target.Dispose()
}

function New-HealerBody {
    $sourcePath = Join-Path $reanimDirectory "Zombie_body.png"
    $source = [System.Drawing.Bitmap]::new($sourcePath)
    $body = [System.Drawing.Bitmap]::new(
        $source.Width, $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $source.Height; $y++) {
        for ($x = 0; $x -lt $source.Width; $x++) {
            $pixel = $source.GetPixel($x, $y)
            if ($pixel.A -eq 0) {
                $body.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                continue
            }
            # 只把原本棕色外套换为灰白急救背心；白衬衣、红领带和深色轮廓保留原层次。
            if ($pixel.R -gt $pixel.G -and $pixel.G -gt $pixel.B -and $pixel.R -gt 45) {
                $luma = [Math]::Min(232, [Math]::Max(70,
                    [int](0.45 * $pixel.R + 0.40 * $pixel.G + 0.15 * $pixel.B) + 45))
                $body.SetPixel($x, $y, (New-Color $pixel.A $luma ([Math]::Min(238, $luma + 5)) ([Math]::Min(235, $luma + 2))))
            }
            else {
                $body.SetPixel($x, $y, $pixel)
            }
        }
    }
    # 低分辨率绿色肩带与小胸章只覆盖原本不透明的背心区域。
    foreach ($point in @(
        @(12, 15), @(13, 16), @(14, 17), @(15, 18), @(16, 19),
        @(34, 30), @(35, 30), @(36, 30), @(35, 29), @(35, 31))) {
        $x = $point[0]; $y = $point[1]
        if ($x -lt $body.Width -and $y -lt $body.Height -and $body.GetPixel($x, $y).A -gt 0) {
            $body.SetPixel($x, $y, (New-Color 255 33 133 77))
        }
    }
    $body.Save((Join-Path $reanimDirectory "Zombie_healer_body.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
    $body.Dispose()
    $source.Dispose()
}

function New-GearBitmap {
    param([ValidateSet("idle", "area", "focused", "disabled")][string]$State)

    $scale = 4
    $large = [System.Drawing.Bitmap]::new(
        112 * $scale, 112 * $scale,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($large)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.ScaleTransform($scale, $scale)

    $outline = [System.Drawing.Pen]::new((New-Color 255 31 45 35), 3.2)
    $metalPen = [System.Drawing.Pen]::new((New-Color 255 72 83 72), 2.2)
    $seamPen = [System.Drawing.Pen]::new((New-Color 235 171 208 180), 1.6)
    $green = [System.Drawing.SolidBrush]::new((New-Color 255 38 137 77))
    $greenDark = [System.Drawing.SolidBrush]::new((New-Color 255 20 79 49))
    $greenLight = [System.Drawing.SolidBrush]::new((New-Color 255 118 224 145))
    $cloth = [System.Drawing.SolidBrush]::new((New-Color 255 63 153 92))
    $metal = [System.Drawing.SolidBrush]::new((New-Color 255 101 116 104))
    $white = [System.Drawing.SolidBrush]::new((New-Color 255 235 247 235))
    $purple = [System.Drawing.SolidBrush]::new((New-Color 255 108 58 139))
    $purpleDark = [System.Drawing.SolidBrush]::new((New-Color 255 51 28 73))
    $char = [System.Drawing.SolidBrush]::new((New-Color 255 67 62 55))

    # 带厚边、扣带和磨损高光的绿色急救包。
    $graphics.FillRectangle($greenDark, 5, 38, 48, 54)
    $graphics.DrawRectangle($outline, 5, 38, 48, 54)
    $graphics.FillRectangle($cloth, 10, 42, 38, 44)
    $graphics.DrawLine($seamPen, 12, 49, 46, 49)
    $graphics.DrawLine($seamPen, 12, 78, 46, 78)
    $graphics.FillRectangle($metal, 22, 32, 16, 8)
    $graphics.DrawRectangle($metalPen, 22, 32, 16, 8)
    $graphics.FillRectangle($greenDark, 17, 56, 24, 19)
    $graphics.FillRectangle($white, 25, 58, 8, 15)
    $graphics.FillRectangle($white, 20, 63, 18, 6)
    $graphics.FillRectangle($greenLight, 26, 59, 6, 13)
    $graphics.FillRectangle($greenLight, 21, 64, 16, 4)
    $graphics.FillEllipse($metal, 9, 45, 4, 4)
    $graphics.FillEllipse($metal, 45, 45, 4, 4)
    $graphics.FillEllipse($metal, 9, 82, 4, 4)
    $graphics.FillEllipse($metal, 45, 82, 4, 4)

    if ($State -eq "idle") {
        # 折叠状态：灯牌贴着背包横放，绿色窗口熄灭。
        $graphics.FillRectangle($metal, 47, 61, 54, 17)
        $graphics.DrawRectangle($outline, 47, 61, 54, 17)
        $graphics.FillRectangle($greenDark, 54, 65, 35, 9)
        $graphics.DrawLine($seamPen, 91, 65, 97, 73)
    }
    elseif ($State -eq "area") {
        $graphics.FillRectangle($metal, 53, 42, 8, 45)
        $graphics.DrawRectangle($metalPen, 53, 42, 8, 45)
        $graphics.FillEllipse($greenDark, 54, 7, 50, 50)
        $graphics.DrawEllipse($outline, 54, 7, 50, 50)
        $graphics.FillEllipse($green, 59, 12, 40, 40)
        foreach ($cx in @(69, 79, 89)) {
            $graphics.FillRectangle($white, $cx - 2, 24, 5, 17)
            $graphics.FillRectangle($white, $cx - 6, 30, 13, 5)
            $graphics.FillRectangle($greenLight, $cx - 1, 25, 3, 15)
            $graphics.FillRectangle($greenLight, $cx - 5, 31, 11, 3)
        }
    }
    elseif ($State -eq "focused") {
        $graphics.FillRectangle($metal, 53, 42, 8, 45)
        $graphics.DrawRectangle($metalPen, 53, 42, 8, 45)
        $points = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new(78, 4),
            [System.Drawing.PointF]::new(105, 24),
            [System.Drawing.PointF]::new(94, 56),
            [System.Drawing.PointF]::new(62, 56),
            [System.Drawing.PointF]::new(51, 24))
        $graphics.FillPolygon($purpleDark, $points)
        $graphics.DrawPolygon($outline, $points)
        $graphics.FillEllipse($purple, 63, 14, 30, 30)
        $graphics.FillRectangle($greenLight, 75, 18, 7, 23)
        $graphics.FillRectangle($greenLight, 67, 26, 23, 7)
        $graphics.FillRectangle($white, 77, 21, 3, 17)
        $graphics.FillRectangle($white, 70, 28, 17, 3)
        $bolt = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new(94, 36),
            [System.Drawing.PointF]::new(87, 48),
            [System.Drawing.PointF]::new(93, 47),
            [System.Drawing.PointF]::new(87, 58),
            [System.Drawing.PointF]::new(101, 43),
            [System.Drawing.PointF]::new(95, 44))
        $graphics.FillPolygon($greenLight, $bolt)
    }
    else {
        # 禁疗状态：标志烧焦后向下折断，绿色窗口只剩裂纹。
        $graphics.TranslateTransform(62, 57)
        $graphics.RotateTransform(28)
        $graphics.FillRectangle($char, -5, -10, 11, 42)
        $graphics.DrawRectangle($outline, -5, -10, 11, 42)
        $graphics.FillRectangle($char, -20, 19, 53, 25)
        $graphics.DrawRectangle($outline, -20, 19, 53, 25)
        $graphics.DrawLine($seamPen, -12, 24, 4, 38)
        $graphics.DrawLine($seamPen, 4, 38, 17, 25)
        $graphics.ResetTransform()
        $graphics.ScaleTransform($scale, $scale)
    }

    foreach ($resource in @($outline, $metalPen, $seamPen, $green, $greenDark,
        $greenLight, $cloth, $metal, $white, $purple, $purpleDark, $char)) {
        $resource.Dispose()
    }
    $graphics.Dispose()
    $output = Join-Path $reanimDirectory "Zombie_healer_gear_$State.png"
    Save-ReducedBitmap -Source $large -Width 112 -Height 112 -Path $output
    $large.Dispose()
}

function New-ParticleTexture {
    param([ValidateSet("plus", "halo")][string]$Kind)
    $size = if ($Kind -eq "plus") { 40 } else { 56 }
    $scale = 4
    $large = [System.Drawing.Bitmap]::new(
        $size * $scale, $size * $scale,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($large)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($scale, $scale)
    if ($Kind -eq "plus") {
        $dark = [System.Drawing.SolidBrush]::new((New-Color 255 15 74 40))
        $white = [System.Drawing.SolidBrush]::new((New-Color 255 244 255 244))
        $green = [System.Drawing.SolidBrush]::new((New-Color 255 75 224 119))
        $graphics.FillRectangle($dark, 13, 2, 14, 36)
        $graphics.FillRectangle($dark, 2, 13, 36, 14)
        $graphics.FillRectangle($white, 16, 5, 8, 30)
        $graphics.FillRectangle($white, 5, 16, 30, 8)
        $graphics.FillRectangle($green, 18, 7, 4, 26)
        $graphics.FillRectangle($green, 7, 18, 26, 4)
        $dark.Dispose(); $white.Dispose(); $green.Dispose()
    }
    else {
        $penOuter = [System.Drawing.Pen]::new((New-Color 175 22 111 61), 5)
        $penInner = [System.Drawing.Pen]::new((New-Color 210 171 255 193), 3)
        $graphics.DrawEllipse($penOuter, 5, 5, 46, 46)
        $graphics.DrawEllipse($penInner, 8, 8, 40, 40)
        $penOuter.Dispose(); $penInner.Dispose()
    }
    $graphics.Dispose()
    $output = Join-Path $particleDirectory $(if ($Kind -eq "plus") { "HealerPlus.png" } else { "HealerHalo.png" })
    Save-ReducedBitmap -Source $large -Width $size -Height $size -Path $output
    $large.Dispose()
}

New-HealerBody
foreach ($state in @("idle", "area", "focused", "disabled")) {
    New-GearBitmap -State $state
}
New-ParticleTexture -Kind "plus"
New-ParticleTexture -Kind "halo"

$generated = @(
    (Join-Path $reanimDirectory "Zombie_healer_body.png"),
    (Join-Path $reanimDirectory "Zombie_healer_gear_idle.png"),
    (Join-Path $reanimDirectory "Zombie_healer_gear_area.png"),
    (Join-Path $reanimDirectory "Zombie_healer_gear_focused.png"),
    (Join-Path $reanimDirectory "Zombie_healer_gear_disabled.png"),
    (Join-Path $particleDirectory "HealerPlus.png"),
    (Join-Path $particleDirectory "HealerHalo.png"))
$expectedHashes = @{
    "Zombie_healer_body.png" = "3CB8D178CF8E6FA7B92F7BE01EE861A2169C7224850D84D671233EAD132132A3"
    "Zombie_healer_gear_idle.png" = "4693EA7C798BE3C8159AD1FB370F935A729589985745968C906C13EA02142D25"
    "Zombie_healer_gear_area.png" = "4200EA0397377DAAC0D4585DA023AF284411726BE430471A3B14F739EB912FCE"
    "Zombie_healer_gear_focused.png" = "A8DFC5E581C6C99A7B79052071624FAE7E865A01F3C26EF9F3582AD7823AE150"
    "Zombie_healer_gear_disabled.png" = "331EBEE68D15C93033FEFE4CD9081E499A912B47702F73B1FE212B21F16C686C"
    "HealerPlus.png" = "000E6E4BFE2B4D3FC0C5EB106B4A5E15E31513482FB0C8C0A3CB7A31CD4C16D2"
    "HealerHalo.png" = "6DD31C989CEE07D03A895D9EAB6639F63E6198E2C4E1CFFEBFBF08C29A6EF397"
}
foreach ($path in $generated) {
    $hash = (Get-FileHash $path -Algorithm SHA256).Hash
    $name = Split-Path $path -Leaf
    if ($hash -ne $expectedHashes[$name]) {
        throw "Unexpected generated hash for ${name}: $hash"
    }
    Write-Output "$name $hash"
}
