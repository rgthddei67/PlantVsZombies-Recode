param(
    [string]$ResourceRoot = "build/clang-release/resources"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resourcePath = [IO.Path]::GetFullPath((Join-Path $PWD $ResourceRoot))
$reanimImagePath = Join-Path $resourcePath "image/reanim"
$cardPath = Join-Path $resourcePath "image/PlantImage"
$reanimPath = Join-Path $resourcePath "reanim"
$motherPath = Join-Path $PSScriptRoot "assets/ThermalPulse_mother.png"
$utf8NoBom = [Text.UTF8Encoding]::new($false)

function Clamp-Byte([double]$value) {
    return [byte][Math]::Max(0, [Math]::Min(255, [Math]::Round($value)))
}

function Get-AggregateHash([string[]]$paths) {
    $lines = foreach ($path in ($paths | Sort-Object)) {
        $fullPath = [IO.Path]::GetFullPath($path)
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "Missing asset in hash set: $fullPath"
        }
        "{0}|{1}" -f $fullPath.ToLowerInvariant(),
            (Get-FileHash -Algorithm SHA256 -LiteralPath $fullPath).Hash
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "")
    }
    finally { $sha.Dispose() }
}

function Convert-PalettePixel([Drawing.Color]$color, [string]$palette) {
    if ($color.A -eq 0) { return $color }
    $r = [double]$color.R
    $g = [double]$color.G
    $b = [double]$color.B
    $lum = 0.299 * $r + 0.587 * $g + 0.114 * $b

    # 原版墨线保持深色；中高光分别映射到北极星蓝紫和冰镜青蓝。
    if ($lum -lt 38) {
        return [Drawing.Color]::FromArgb($color.A,
            (Clamp-Byte (0.72 * $r)), (Clamp-Byte (0.82 * $g)),
            (Clamp-Byte ([Math]::Min(255, 0.98 * $b + 8))))
    }
    switch ($palette) {
        "northPetal" {
            return [Drawing.Color]::FromArgb($color.A,
                (Clamp-Byte (72 + 0.54 * $lum)),
                (Clamp-Byte (80 + 0.58 * $lum)),
                (Clamp-Byte (128 + 0.54 * $lum)))
        }
        "northFace" {
            return [Drawing.Color]::FromArgb($color.A,
                (Clamp-Byte (28 + 0.42 * $lum)),
                (Clamp-Byte (82 + 0.55 * $lum)),
                (Clamp-Byte (112 + 0.55 * $lum)))
        }
        "northLeaf" {
            return [Drawing.Color]::FromArgb($color.A,
                (Clamp-Byte (12 + 0.25 * $lum)),
                (Clamp-Byte (65 + 0.64 * $lum)),
                (Clamp-Byte (92 + 0.70 * $lum)))
        }
        "mirrorBody" {
            return [Drawing.Color]::FromArgb($color.A,
                (Clamp-Byte (54 + 0.46 * $lum)),
                (Clamp-Byte (106 + 0.55 * $lum)),
                (Clamp-Byte (138 + 0.50 * $lum)))
        }
        default {
            return [Drawing.Color]::FromArgb($color.A,
                (Clamp-Byte (24 + 0.30 * $lum)),
                (Clamp-Byte (82 + 0.58 * $lum)),
                (Clamp-Byte (116 + 0.62 * $lum)))
        }
    }
}

function Convert-ClassicImage(
    [string]$sourcePath,
    [string]$destinationPath,
    [string]$palette
) {
    $source = [Drawing.Bitmap]::FromFile($sourcePath)
    try {
        $output = [Drawing.Bitmap]::new($source.Width, $source.Height,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; ++$y) {
                for ($x = 0; $x -lt $source.Width; ++$x) {
                    $output.SetPixel($x, $y,
                        (Convert-PalettePixel $source.GetPixel($x, $y) $palette))
                }
            }
            $output.Save($destinationPath, [Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $output.Dispose() }
    }
    finally { $source.Dispose() }
}

function Get-ReanimImageIndex {
    $index = @{}
    foreach ($file in Get-ChildItem -LiteralPath $reanimImagePath -Filter "*.png") {
        $key = "IMAGE_REANIM_" + $file.BaseName.ToUpperInvariant()
        $index[$key] = $file.FullName
    }
    return $index
}

function Generate-RecoloredReanim(
    [string]$sourceName,
    [string]$destinationName,
    [scriptblock]$paletteSelector,
    [hashtable]$imageIndex,
    [System.Collections.Generic.List[string]]$outputs
) {
    $sourceReanim = Join-Path $reanimPath "$sourceName.reanim"
    $xml = [IO.File]::ReadAllText($sourceReanim)
    $keys = [regex]::Matches($xml, '<i>(IMAGE_REANIM_[^<]+)</i>') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    foreach ($key in $keys) {
        if (-not $imageIndex.ContainsKey($key)) {
            throw "Cannot resolve classic reanim image key: $key"
        }
        $source = $imageIndex[$key]
        $sourceStem = [IO.Path]::GetFileNameWithoutExtension($source)
        $separator = $sourceStem.IndexOf('_')
        $suffix = if ($separator -ge 0) {
            $sourceStem.Substring($separator + 1)
        } else { $sourceStem }
        $destinationStem = "${destinationName}_$suffix"
        $destination = Join-Path $reanimImagePath "$destinationStem.png"
        $palette = & $paletteSelector $key
        Convert-ClassicImage $source $destination $palette
        $outputs.Add($destination)
        $xml = $xml.Replace($key,
            "IMAGE_REANIM_" + $destinationStem.ToUpperInvariant())
    }
    $destinationReanim = Join-Path $reanimPath "$destinationName.reanim"
    [IO.File]::WriteAllText($destinationReanim, $xml, $utf8NoBom)
    $outputs.Add($destinationReanim)
}

function Generate-Card(
    [string]$sourceName,
    [string]$destinationName,
    [string]$palette,
    [string]$emblem,
    [System.Collections.Generic.List[string]]$outputs
) {
    $destination = Join-Path $cardPath "$destinationName.png"
    Convert-ClassicImage (Join-Path $cardPath "$sourceName.png") $destination $palette
    $bitmap = [Drawing.Bitmap]::FromFile($destination)
    try {
        $copy = [Drawing.Bitmap]::new($bitmap)
    }
    finally { $bitmap.Dispose() }
    try {
        $temporary = "$destination.tmp.png"
        try {
            $graphics = [Drawing.Graphics]::FromImage($copy)
            try {
                $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
                if ($emblem -eq "star") {
                    $points = [Drawing.PointF[]]@(
                        [Drawing.PointF]::new(64, 5), [Drawing.PointF]::new(68, 15),
                        [Drawing.PointF]::new(79, 15), [Drawing.PointF]::new(70, 22),
                        [Drawing.PointF]::new(73, 33), [Drawing.PointF]::new(64, 27),
                        [Drawing.PointF]::new(55, 33), [Drawing.PointF]::new(58, 22),
                        [Drawing.PointF]::new(49, 15), [Drawing.PointF]::new(60, 15))
                    $graphics.FillPolygon([Drawing.Brushes]::White, $points)
                    $graphics.DrawPolygon([Drawing.Pen]::new(
                        [Drawing.Color]::FromArgb(255, 74, 116, 215), 2.0), $points)
                }
                else {
                    $brush = [Drawing.SolidBrush]::new(
                        [Drawing.Color]::FromArgb(155, 168, 239, 255))
                    $pen = [Drawing.Pen]::new(
                        [Drawing.Color]::FromArgb(245, 235, 254, 255), 2.5)
                    try {
                        $graphics.FillEllipse($brush, 49, 5, 30, 38)
                        $graphics.DrawEllipse($pen, 49, 5, 30, 38)
                        $graphics.DrawLine($pen, 55, 33, 73, 13)
                    }
                    finally { $brush.Dispose(); $pen.Dispose() }
                }
            }
            finally { $graphics.Dispose() }
            $copy.Save($temporary, [Drawing.Imaging.ImageFormat]::Png)
            Move-Item -LiteralPath $temporary -Destination $destination -Force
        }
        finally {
            if (Test-Path -LiteralPath $temporary) {
                Remove-Item -LiteralPath $temporary -Force
            }
        }
    }
    finally { $copy.Dispose() }
    $outputs.Add($destination)
}

function Generate-ThermalPulse(
    [string]$sourcePath,
    [string]$destinationPath,
    [System.Collections.Generic.List[string]]$outputs
) {
    $source = [Drawing.Bitmap]::FromFile($sourcePath)
    try {
        $left = $source.Width
        $top = $source.Height
        $right = -1
        $bottom = -1
        for ($y = 0; $y -lt $source.Height; ++$y) {
            for ($x = 0; $x -lt $source.Width; ++$x) {
                $pixel = $source.GetPixel($x, $y)
                $strength = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                if ($strength -gt 18) {
                    $left = [Math]::Min($left, $x)
                    $top = [Math]::Min($top, $y)
                    $right = [Math]::Max($right, $x)
                    $bottom = [Math]::Max($bottom, $y)
                }
            }
        }
        if ($right -lt $left -or $bottom -lt $top) { throw "Thermal pulse mother art is empty" }
        $crop = [Drawing.Bitmap]::new($right - $left + 1, $bottom - $top + 1,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $crop.Height; ++$y) {
                for ($x = 0; $x -lt $crop.Width; ++$x) {
                    $pixel = $source.GetPixel($left + $x, $top + $y)
                    $strength = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                    $alpha = Clamp-Byte ([Math]::Max(0, ($strength - 8) * 1.45))
                    $crop.SetPixel($x, $y, [Drawing.Color]::FromArgb(
                        $alpha, $pixel.R, $pixel.G, $pixel.B))
                }
            }
            $output = [Drawing.Bitmap]::new(80, 42,
                [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $graphics = [Drawing.Graphics]::FromImage($output)
                try {
                    $graphics.InterpolationMode =
                        [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                    $scale = [Math]::Min(76.0 / $crop.Width, 38.0 / $crop.Height)
                    $drawWidth = [int][Math]::Round($crop.Width * $scale)
                    $drawHeight = [int][Math]::Round($crop.Height * $scale)
                    $graphics.DrawImage($crop, [int]((80 - $drawWidth) / 2),
                        [int]((42 - $drawHeight) / 2), $drawWidth, $drawHeight)
                }
                finally { $graphics.Dispose() }
                $output.Save($destinationPath, [Drawing.Imaging.ImageFormat]::Png)
            }
            finally { $output.Dispose() }
        }
        finally { $crop.Dispose() }
    }
    finally { $source.Dispose() }
    $outputs.Add($destinationPath)
}

function Generate-ThermalEquipment(
    [string]$destinationPath,
    [string]$kind,
    [System.Collections.Generic.List[string]]$outputs
) {
    $size = if ($kind -eq "rifle") { [Drawing.Size]::new(120, 44) }
        elseif ($kind -eq "pack") { [Drawing.Size]::new(58, 76) }
        else { [Drawing.Size]::new(74, 38) }
    $bitmap = [Drawing.Bitmap]::new($size.Width, $size.Height,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $g = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $outline = [Drawing.Pen]::new([Drawing.Color]::FromArgb(255, 38, 32, 38), 4.0)
            $metal = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 78, 96, 110))
            $dark = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 42, 55, 67))
            $heat = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 255, 102, 28))
            $core = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 255, 238, 108))
            try {
                if ($kind -eq "rifle") {
                    $g.FillRectangle($dark, 22, 12, 78, 18)
                    $g.DrawRectangle($outline, 22, 12, 78, 18)
                    $g.FillPolygon($metal, [Drawing.Point[]]@(
                        [Drawing.Point]::new(0, 16), [Drawing.Point]::new(24, 10),
                        [Drawing.Point]::new(24, 31), [Drawing.Point]::new(0, 27)))
                    $g.DrawPolygon($outline, [Drawing.Point[]]@(
                        [Drawing.Point]::new(0, 16), [Drawing.Point]::new(24, 10),
                        [Drawing.Point]::new(24, 31), [Drawing.Point]::new(0, 27)))
                    $g.FillRectangle($metal, 93, 7, 23, 28)
                    $g.DrawRectangle($outline, 93, 7, 23, 28)
                    foreach ($x in 38, 52, 66, 80) {
                        $g.FillEllipse($heat, $x, 14, 10, 14)
                        $g.FillEllipse($core, $x + 3, 17, 4, 8)
                    }
                }
                elseif ($kind -eq "pack") {
                    $g.FillRoundedRectangle($dark, 8, 5, 42, 66, 10)
                    $g.DrawRoundedRectangle($outline, 8, 5, 42, 66, 10)
                    $g.FillRectangle($metal, 13, 12, 32, 20)
                    $g.FillEllipse($heat, 17, 39, 24, 24)
                    $g.FillEllipse($core, 23, 45, 12, 12)
                }
                else {
                    $g.FillRectangle($dark, 4, 11, 66, 14)
                    $g.DrawRectangle($outline, 4, 11, 66, 14)
                    $g.FillEllipse($heat, 8, 4, 25, 28)
                    $g.FillEllipse($heat, 41, 4, 25, 28)
                    $g.FillEllipse($core, 14, 10, 13, 16)
                    $g.FillEllipse($core, 47, 10, 13, 16)
                }
            }
            finally {
                $outline.Dispose(); $metal.Dispose(); $dark.Dispose()
                $heat.Dispose(); $core.Dispose()
            }
        }
        finally { $g.Dispose() }
        $bitmap.Save($destinationPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bitmap.Dispose() }
    $outputs.Add($destinationPath)
}

# System.Drawing 没有内建圆角矩形，给 Graphics 补两个局部脚本方法。
Update-TypeData -TypeName System.Drawing.Graphics -MemberType ScriptMethod `
    -MemberName FillRoundedRectangle -Force -Value {
        param($brush, $x, $y, $width, $height, $radius)
        $path = [Drawing.Drawing2D.GraphicsPath]::new()
        try {
            $path.AddArc($x, $y, $radius, $radius, 180, 90)
            $path.AddArc($x + $width - $radius, $y, $radius, $radius, 270, 90)
            $path.AddArc($x + $width - $radius, $y + $height - $radius,
                $radius, $radius, 0, 90)
            $path.AddArc($x, $y + $height - $radius, $radius, $radius, 90, 90)
            $path.CloseFigure()
            $this.FillPath($brush, $path)
        }
        finally { $path.Dispose() }
    }
Update-TypeData -TypeName System.Drawing.Graphics -MemberType ScriptMethod `
    -MemberName DrawRoundedRectangle -Force -Value {
        param($pen, $x, $y, $width, $height, $radius)
        $path = [Drawing.Drawing2D.GraphicsPath]::new()
        try {
            $path.AddArc($x, $y, $radius, $radius, 180, 90)
            $path.AddArc($x + $width - $radius, $y, $radius, $radius, 270, 90)
            $path.AddArc($x + $width - $radius, $y + $height - $radius,
                $radius, $radius, 0, 90)
            $path.AddArc($x, $y + $height - $radius, $radius, $radius, 90, 90)
            $path.CloseFigure()
            $this.DrawPath($pen, $path)
        }
        finally { $path.Dispose() }
    }

$imageIndex = Get-ReanimImageIndex
$outputs = [System.Collections.Generic.List[string]]::new()
$northSelector = {
    param($key)
    if ($key -match 'SUNFLOWER_.*PETAL') { return "northPetal" }
    if ($key -match 'SUNFLOWER_(HEAD|BLINK)') { return "northFace" }
    return "northLeaf"
}
$mirrorSelector = {
    param($key)
    if ($key -match 'PLANTERN_(BODY|EYES)') { return "mirrorBody" }
    return "mirrorLeaf"
}

Generate-RecoloredReanim "SunFlower" "NorthStarFlower" $northSelector $imageIndex $outputs
Generate-RecoloredReanim "Plantern" "IceMirrorGrass" $mirrorSelector $imageIndex $outputs
Generate-Card "SunFlower" "NorthStarFlower" "northPetal" "star" $outputs
Generate-Card "Plantern" "IceMirrorGrass" "mirrorBody" "mirror" $outputs
Generate-ThermalPulse $motherPath (Join-Path $resourcePath "image/ProjectileThermalPulse.png") $outputs
Generate-ThermalEquipment (Join-Path $reanimImagePath "Zombie_thermal_goggles.png") "goggles" $outputs
Generate-ThermalEquipment (Join-Path $reanimImagePath "Zombie_thermal_pack.png") "pack" $outputs
Generate-ThermalEquipment (Join-Path $reanimImagePath "Zombie_thermal_rifle.png") "rifle" $outputs

$sourceReanims = @(
    (Join-Path $reanimPath "SunFlower.reanim"),
    (Join-Path $reanimPath "Plantern.reanim"),
    (Join-Path $cardPath "SunFlower.png"),
    (Join-Path $cardPath "Plantern.png"),
    $motherPath
)
$sourceAggregate = Get-AggregateHash $sourceReanims
$outputAggregate = Get-AggregateHash $outputs.ToArray()
$expectedSourceAggregate = "DE26626721FB8F0B714483DEE5529735E4CA4547690D64AD2BD35D494477AD92"
$expectedOutputAggregate = "016A2E2467E615A5FE42E100D934FFA7F98055274CE5710A3C692FCE49842A4B"
if ($expectedSourceAggregate -ne "TO_BE_FILLED" -and
    $sourceAggregate -ne $expectedSourceAggregate) {
    throw "Polar midgame source assets drifted: expected=$expectedSourceAggregate actual=$sourceAggregate"
}
if ($expectedOutputAggregate -ne "TO_BE_FILLED" -and
    $outputAggregate -ne $expectedOutputAggregate) {
    throw "Polar midgame generated assets drifted: expected=$expectedOutputAggregate actual=$outputAggregate"
}

Write-Output "sourceAggregate=$sourceAggregate"
Write-Output "outputAggregate=$outputAggregate"
Write-Output "Generated animated plant recolors, distinct cards, thermal equipment, and thermal pulse."
