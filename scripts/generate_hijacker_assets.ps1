param(
    [string]$ResourceRoot = (Join-Path $PSScriptRoot "../build/clang-release/resources")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$imageRoot = Join-Path $ResourceRoot "image/reanim"
$sourceReanim = Join-Path $ResourceRoot "reanim/Zombie_JackBox.reanim"
$targetReanim = Join-Path $ResourceRoot "reanim/Zombie_Hijacker.reanim"
$scale = 4

function New-Pen([int]$a, [int]$r, [int]$g, [int]$b, [float]$width) {
    return [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb($a, $r, $g, $b), $width)
}

function New-Brush([int]$a, [int]$r, [int]$g, [int]$b) {
    return [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb($a, $r, $g, $b))
}

function Add-MechanicalReceiver([System.Drawing.Graphics]$g, [string]$part) {
    $outline = New-Pen 255 31 17 43 2.2
    $copper = New-Pen 255 174 91 42 1.5
    $highlight = New-Pen 230 246 163 73 0.75
    $glass = New-Brush 235 69 16 92
    $glassCore = New-Brush 235 166 65 218
    $shell = New-Brush 245 50 25 69
    $shellLight = New-Brush 245 87 42 105
    $arc = New-Pen 245 229 139 255 1.1
    $wear = New-Pen 190 187 133 158 0.65
    try {
        if ($part -eq "box") {
            $shellPoints = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(4, 14), [System.Drawing.PointF]::new(11, 5),
                [System.Drawing.PointF]::new(45, 5), [System.Drawing.PointF]::new(54, 15),
                [System.Drawing.PointF]::new(52, 44), [System.Drawing.PointF]::new(43, 52),
                [System.Drawing.PointF]::new(10, 50), [System.Drawing.PointF]::new(4, 41))
            $g.FillPolygon($shell, $shellPoints)
            $g.DrawPolygon($outline, $shellPoints)
            $platePoints = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(10, 15), [System.Drawing.PointF]::new(44, 11),
                [System.Drawing.PointF]::new(48, 39), [System.Drawing.PointF]::new(40, 46),
                [System.Drawing.PointF]::new(12, 43))
            $g.FillPolygon($shellLight, $platePoints)
            $g.DrawPolygon($copper, $platePoints)
            $g.FillEllipse($glass, 15, 17, 28, 24)
            $g.DrawEllipse($copper, 15, 17, 28, 24)
            $g.FillEllipse($glassCore, 22, 22, 14, 14)
            $g.DrawArc($arc, 18, 19, 22, 19, 205, 120)
            $g.DrawBezier($arc, 20, 32, 25, 22, 31, 37, 38, 25)
            $g.DrawBezier($highlight, 17, 16, 24, 13, 34, 13, 41, 15)
            foreach ($p in @(@(10,13), @(47,14), @(48,42), @(10,44))) {
                $g.FillEllipse((New-Brush 255 225 137 58), $p[0] - 1.2, $p[1] - 1.2, 2.4, 2.4)
            }
            $g.DrawLine($wear, 8, 31, 13, 29)
            $g.DrawLine($wear, 42, 8, 47, 10)
            $g.DrawLine($wear, 36, 47, 40, 43)
        }
        elseif ($part -eq "box2") {
            $backPoints = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(5, 19), [System.Drawing.PointF]::new(14, 5),
                [System.Drawing.PointF]::new(55, 4), [System.Drawing.PointF]::new(63, 15),
                [System.Drawing.PointF]::new(58, 38), [System.Drawing.PointF]::new(12, 40))
            $g.FillPolygon($shell, $backPoints)
            $g.DrawPolygon($outline, $backPoints)
            $g.FillEllipse($glass, 18, 9, 34, 28)
            $g.DrawEllipse($copper, 18, 9, 34, 28)
            $g.FillEllipse($glassCore, 27, 15, 16, 15)
            $g.DrawBezier($arc, 20, 27, 27, 10, 37, 34, 50, 14)
            $g.DrawBezier($highlight, 17, 11, 29, 6, 43, 7, 53, 12)
            $lidPoints = [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(13, 6), [System.Drawing.PointF]::new(54, 4),
                [System.Drawing.PointF]::new(48, 0), [System.Drawing.PointF]::new(18, 1))
            $g.FillPolygon($shellLight, $lidPoints)
            $g.DrawPolygon($copper, $lidPoints)
            foreach ($x in @(12, 55)) { $g.FillEllipse((New-Brush 255 225 137 58), $x, 20, 2.5, 2.5) }
            $g.DrawLine($wear, 9, 31, 17, 28)
            $g.DrawLine($wear, 49, 35, 56, 31)
        }
    }
    finally {
        foreach ($item in @($outline, $copper, $highlight, $glass, $glassCore, $shell, $shellLight, $arc, $wear)) {
            $item.Dispose()
        }
    }
}

function Add-CoilAssembly([System.Drawing.Graphics]$g, [string]$part) {
    $dark = New-Pen 255 45 22 38 2.0
    $copper = New-Pen 255 204 106 45 1.5
    $shine = New-Pen 230 255 190 92 0.55
    $wire = New-Pen 245 197 84 247 1.0
    $insulator = New-Brush 250 45 24 63
    try {
        if ($part -eq "handle") {
            $g.FillRectangle($insulator, 1, 5, 20, 7)
            $g.DrawLine($dark, 1, 5, 21, 5)
            $g.DrawLine($dark, 1, 12, 21, 12)
            foreach ($x in 3, 6, 9, 12, 15, 18) {
                $g.DrawArc($copper, $x, 3, 4, 11, 75, 210)
                $g.DrawArc($shine, $x + 0.5, 3.5, 3, 9, 80, 85)
            }
            $g.DrawBezier($wire, 1, 9, -2, 11, 0, 15, 4, 15)
        }
        elseif ($part -eq "clownneck") {
            for ($i = 0; $i -lt 4; ++$i) {
                $y = 2 + $i * 4
                $g.FillEllipse($insulator, 4, $y, 12, 5)
                $g.DrawEllipse($copper, 4, $y, 12, 5)
                $g.DrawArc($shine, 5, $y + 0.5, 10, 3.5, 190, 120)
            }
            $g.DrawLine($wire, 9, 1, 11, 19)
        }
        elseif ($part -eq "clownhead") {
            $g.FillEllipse($insulator, 14, 27, 22, 18)
            $g.DrawEllipse($dark, 14, 27, 22, 18)
            for ($i = 0; $i -lt 3; ++$i) {
                $g.DrawArc($copper, 17, 29 + $i * 4, 16, 5, 185, 170)
            }
            $g.DrawLine($dark, 19, 30, 14, 7)
            $g.DrawLine($dark, 31, 30, 37, 7)
            $g.DrawLine($copper, 19, 30, 14, 7)
            $g.DrawLine($copper, 31, 30, 37, 7)
            $g.DrawArc($wire, 9, 1, 11, 12, 155, 210)
            $g.DrawArc($wire, 31, 1, 11, 12, 175, 210)
            $g.FillEllipse((New-Brush 255 234 166 255), 11, 3, 5, 5)
            $g.FillEllipse((New-Brush 255 234 166 255), 35, 3, 5, 5)
        }
    }
    finally {
        foreach ($item in @($dark, $copper, $shine, $wire, $insulator)) { $item.Dispose() }
    }
}

function Add-Goggles([System.Drawing.Graphics]$g, [string]$part) {
    if ($part -ne "head" -and $part -ne "head2") { return }
    $frame = New-Pen 255 106 54 33 2.4
    $rim = New-Pen 255 222 131 55 1.0
    $strap = New-Pen 230 43 25 49 3.0
    $lens = New-Brush 165 80 30 119
    $glint = New-Pen 245 226 190 255 1.0
    $meter = New-Brush 245 211 194 139
    $needle = New-Pen 255 137 29 47 0.75
    try {
        $g.DrawLine($strap, 9, 21, 50, 19)
        $g.FillEllipse($lens, 14, 14, 15, 13)
        $g.FillEllipse($lens, 31, 13, 15, 13)
        $g.DrawEllipse($frame, 14, 14, 15, 13)
        $g.DrawEllipse($frame, 31, 13, 15, 13)
        $g.DrawEllipse($rim, 15.5, 15.5, 12, 10)
        $g.DrawEllipse($rim, 32.5, 14.5, 12, 10)
        $g.DrawLine($frame, 28, 19, 32, 18)
        $g.DrawArc($glint, 17, 16, 8, 6, 195, 85)
        $g.DrawArc($glint, 35, 15, 7, 6, 195, 85)
        $g.FillEllipse($meter, 45, 23, 9, 9)
        $g.DrawEllipse($frame, 45, 23, 9, 9)
        $g.DrawLine($needle, 49.5, 27.5, 52, 25.5)
    }
    finally {
        foreach ($item in @($frame, $rim, $strap, $lens, $glint, $meter, $needle)) { $item.Dispose() }
    }
}

function Add-WorkwearDetail([System.Drawing.Graphics]$g, [string]$part, [int]$w, [int]$h) {
    if ($part -notmatch "body|leg|arm|belt") { return }
    $seam = New-Pen 175 155 105 169 0.8
    $shadow = New-Pen 170 36 20 48 1.1
    $copper = New-Brush 230 202 111 48
    try {
        $g.DrawLine($seam, $w * 0.22, $h * 0.35, $w * 0.72, $h * 0.68)
        $g.DrawLine($shadow, $w * 0.26, $h * 0.39, $w * 0.70, $h * 0.72)
        $g.DrawLine($seam, $w * 0.18, $h * 0.78, $w * 0.44, $h * 0.72)
        $g.FillEllipse($copper, [Math]::Max(1, $w * 0.25), [Math]::Max(1, $h * 0.55), 2.2, 2.2)
        $g.FillEllipse($copper, [Math]::Max(3, $w * 0.62), [Math]::Max(2, $h * 0.39), 1.8, 1.8)
        if ($part -eq "body1") {
            $patch = New-Brush 190 70 38 82
            $g.FillPolygon($patch, [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(12, 48), [System.Drawing.PointF]::new(30, 45),
                [System.Drawing.PointF]::new(33, 59), [System.Drawing.PointF]::new(16, 63)))
            $g.DrawPolygon($seam, [System.Drawing.PointF[]]@(
                [System.Drawing.PointF]::new(12, 48), [System.Drawing.PointF]::new(30, 45),
                [System.Drawing.PointF]::new(33, 59), [System.Drawing.PointF]::new(16, 63)))
            $patch.Dispose()
        }
        if ($part -eq "outerarm_lower2") {
            $g.DrawBezier($seam, 19, 35, 27, 28, 35, 43, 43, 33)
            $g.DrawBezier((New-Pen 245 222 111 249 0.9), 20, 37, 28, 31, 34, 46, 42, 36)
        }
    }
    finally {
        foreach ($item in @($seam, $shadow, $copper)) { $item.Dispose() }
    }
}

function Convert-ToHijackerPart([System.IO.FileInfo]$source) {
    $part = $source.BaseName.Substring("Zombie_jackbox_".Length).ToLowerInvariant()
    $target = Join-Path $imageRoot ("Zombie_hijacker_{0}.png" -f $part)
    $src = [System.Drawing.Bitmap]::new($source.FullName)
    try {
        $tinted = [System.Drawing.Bitmap]::new($src.Width, $src.Height,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        for ($y = 0; $y -lt $src.Height; ++$y) {
            for ($x = 0; $x -lt $src.Width; ++$x) {
                $p = $src.GetPixel($x, $y)
                if ($p.A -eq 0) { continue }
                if ($p.G -gt ($p.R * 1.08) -and $p.G -gt ($p.B * 1.05)) {
                    $r = [Math]::Min(255, [int]($p.R * 0.92 + 4))
                    $gg = [Math]::Min(255, [int]($p.G * 0.96 + 3))
                    $b = [Math]::Min(255, [int]($p.B * 1.02 + 4))
                }
                else {
                    $lum = (0.26 * $p.R + 0.58 * $p.G + 0.16 * $p.B) / 255.0
                    $r = [Math]::Min(255, [int](29 + 103 * $lum))
                    $gg = [Math]::Min(255, [int](18 + 51 * $lum))
                    $b = [Math]::Min(255, [int](42 + 126 * $lum))
                }
                $tinted.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($p.A, $r, $gg, $b))
            }
        }

        $large = [System.Drawing.Bitmap]::new($src.Width * $scale, $src.Height * $scale,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($large)
        try {
            $g.Clear([System.Drawing.Color]::Transparent)
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $g.DrawImage($tinted, 0, 0, $large.Width, $large.Height)
            $g.ScaleTransform($scale, $scale)
            Add-MechanicalReceiver $g $part
            Add-CoilAssembly $g $part
            Add-Goggles $g $part
            Add-WorkwearDetail $g $part $src.Width $src.Height
            $g.ResetTransform()
        }
        finally { $g.Dispose() }

        $final = [System.Drawing.Bitmap]::new($src.Width, $src.Height,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $fg = [System.Drawing.Graphics]::FromImage($final)
        try {
            $fg.Clear([System.Drawing.Color]::Transparent)
            $fg.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $fg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $fg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $fg.DrawImage($large, 0, 0, $final.Width, $final.Height)
        }
        finally { $fg.Dispose() }
        try { $final.Save($target, [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $final.Dispose() }
        $large.Dispose()
        $tinted.Dispose()
    }
    finally { $src.Dispose() }
}

Get-ChildItem $imageRoot -Filter "Zombie_jackbox_*.png" |
    Where-Object { $_.BaseName -notlike "*_glow" } |
    ForEach-Object { Convert-ToHijackerPart $_ }

# 断头粒子必须包含已经烘焙进头部贴图的护目镜；断臂粒子把上下臂合成一块独立碎片。
$particleRoot = Join-Path $ResourceRoot "particles"
[System.IO.File]::Copy(
    (Join-Path $imageRoot "Zombie_hijacker_head.png"),
    (Join-Path $particleRoot "ZombieHijackerHead.png"), $true)
$upperArm = [System.Drawing.Bitmap]::new((Join-Path $imageRoot "Zombie_hijacker_outerarm_upper.png"))
$lowerArm = [System.Drawing.Bitmap]::new((Join-Path $imageRoot "Zombie_hijacker_outerarm_lower.png"))
$armParticle = [System.Drawing.Bitmap]::new(74, 96,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$armGraphics = [System.Drawing.Graphics]::FromImage($armParticle)
try {
    $armGraphics.Clear([System.Drawing.Color]::Transparent)
    $armGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $armGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $armGraphics.DrawImage($upperArm, 30, 2, $upperArm.Width, $upperArm.Height)
    $armGraphics.DrawImage($lowerArm, 5, 28, $lowerArm.Width, $lowerArm.Height)
    $cable = New-Pen 245 226 121 250 1.4
    $copper = New-Pen 245 213 111 51 1.2
    try {
        $armGraphics.DrawBezier($cable, 31, 37, 43, 49, 24, 62, 42, 76)
        $armGraphics.DrawBezier($copper, 34, 39, 47, 52, 28, 65, 45, 79)
    }
    finally {
        $cable.Dispose()
        $copper.Dispose()
    }
}
finally {
    $armGraphics.Dispose()
    $upperArm.Dispose()
    $lowerArm.Dispose()
}
try {
    $armParticle.Save((Join-Path $particleRoot "ZombieHijackerArm.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
}
finally { $armParticle.Dispose() }

# 复制到独立资源键，避免停止劫持者低鸣时误停磁力菇或小丑自己的循环声道。
[System.IO.File]::Copy(
    (Join-Path $ResourceRoot "sounds/Plant/magnetshroom.ogg"),
    (Join-Path $ResourceRoot "sounds/Zombie/hijacker_hum.ogg"), $true)
# 处决与同帧雷声叠加；冰晶碎裂层使用独立键，后续可单独替换而不影响寒冰菇。
[System.IO.File]::Copy(
    (Join-Path $ResourceRoot "sounds/Plant/frozen.ogg"),
    (Join-Path $ResourceRoot "sounds/Zombie/hijacker_execute.ogg"), $true)

$reanim = [System.IO.File]::ReadAllText($sourceReanim)
$reanim = $reanim.Replace("IMAGE_REANIM_ZOMBIE_JACKBOX_", "IMAGE_REANIM_ZOMBIE_HIJACKER_")
$reanim = $reanim.Replace("<name>anim_pop</name>", "<name>anim_hijack</name>")
[System.IO.File]::WriteAllText($targetReanim, $reanim,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Generated detailed Hijacker reanimation assets in $imageRoot"
