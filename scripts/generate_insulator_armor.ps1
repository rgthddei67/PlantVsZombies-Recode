param(
    [string]$OutputDirectory = "build/clang-release/resources/image/reanim"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resolvedOutput = Join-Path (Get-Location) $OutputDirectory
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

function New-ArmorBitmap {
    param([int]$Stage)

    $bitmap = [System.Drawing.Bitmap]::new(
        72, 72, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $outline = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 55, 43, 34)), 4
    $seam = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 106, 91, 69)), 2
    $crack = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 76, 58, 45)), 2.6
    $highlight = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(210, 255, 251, 211)), 2
    $ceramic = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 222, 214, 163))
    $ceramicShadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 174, 160, 113))
    $rubber = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 45, 48, 42))
    $bolt = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 113, 131, 111))

    # 宽肩、窄腰的大块轮廓，控制在原版 Zombie_body 的低分辨率尺度。
    $points = [System.Drawing.Point[]]@(
        (New-Object System.Drawing.Point 11, 13),
        (New-Object System.Drawing.Point 28, 7),
        (New-Object System.Drawing.Point 50, 10),
        (New-Object System.Drawing.Point 62, 22),
        (New-Object System.Drawing.Point 57, 57),
        (New-Object System.Drawing.Point 39, 66),
        (New-Object System.Drawing.Point 17, 59),
        (New-Object System.Drawing.Point 7, 30)
    )
    $graphics.FillPolygon($ceramicShadow, $points)
    $graphics.DrawPolygon($outline, $points)

    $plate = [System.Drawing.Point[]]@(
        (New-Object System.Drawing.Point 15, 16),
        (New-Object System.Drawing.Point 31, 11),
        (New-Object System.Drawing.Point 47, 14),
        (New-Object System.Drawing.Point 56, 24),
        (New-Object System.Drawing.Point 52, 52),
        (New-Object System.Drawing.Point 38, 60),
        (New-Object System.Drawing.Point 21, 55),
        (New-Object System.Drawing.Point 12, 30)
    )
    $graphics.FillPolygon($ceramic, $plate)

    # 两条深色绝缘带与三块陶瓷拼片，保持远景轮廓易读。
    $graphics.FillRectangle($rubber, 8, 25, 49, 7)
    $graphics.FillRectangle($rubber, 13, 46, 41, 7)
    $graphics.DrawLine($seam, 34, 13, 35, 25)
    $graphics.DrawLine($seam, 18, 35, 52, 36)
    $graphics.DrawLine($highlight, 18, 18, 42, 14)
    $graphics.FillEllipse($bolt, 11, 26, 5, 5)
    $graphics.FillEllipse($bolt, 50, 27, 5, 5)
    $graphics.FillEllipse($bolt, 16, 47, 5, 5)
    $graphics.FillEllipse($bolt, 47, 48, 5, 5)

    if ($Stage -ge 2) {
        $graphics.DrawLines($crack, [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 42, 14),
            (New-Object System.Drawing.Point 37, 22),
            (New-Object System.Drawing.Point 41, 28),
            (New-Object System.Drawing.Point 34, 36),
            (New-Object System.Drawing.Point 38, 45)
        ))
        $graphics.DrawLines($crack, [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 34, 36),
            (New-Object System.Drawing.Point 27, 32),
            (New-Object System.Drawing.Point 23, 38)
        ))
    }
    if ($Stage -ge 3) {
        $graphics.DrawLines($crack, [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 20, 16),
            (New-Object System.Drawing.Point 25, 24),
            (New-Object System.Drawing.Point 19, 31),
            (New-Object System.Drawing.Point 24, 42),
            (New-Object System.Drawing.Point 18, 53)
        ))
        $graphics.DrawLines($crack, [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 48, 37),
            (New-Object System.Drawing.Point 42, 43),
            (New-Object System.Drawing.Point 46, 51),
            (New-Object System.Drawing.Point 39, 59)
        ))
        # 重伤阶段只缺一个清晰小角，仍然是一整层甲而不是第二件装备。
        $missing = [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 52, 18),
            (New-Object System.Drawing.Point 62, 22),
            (New-Object System.Drawing.Point 58, 33),
            (New-Object System.Drawing.Point 50, 28)
        )
        $graphics.FillPolygon([System.Drawing.Brushes]::Transparent, $missing)
        $graphics.DrawLines($crack, [System.Drawing.Point[]]@(
            (New-Object System.Drawing.Point 51, 17),
            (New-Object System.Drawing.Point 47, 24),
            (New-Object System.Drawing.Point 52, 31)
        ))
    }

    $outline.Dispose()
    $seam.Dispose()
    $crack.Dispose()
    $highlight.Dispose()
    $ceramic.Dispose()
    $ceramicShadow.Dispose()
    $rubber.Dispose()
    $bolt.Dispose()
    $graphics.Dispose()
    return $bitmap
}

for ($stage = 1; $stage -le 3; $stage++) {
    $bitmap = New-ArmorBitmap -Stage $stage
    $path = Join-Path $resolvedOutput "Zombie_insulator_armor$stage.png"
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

$expectedHashes = @{
    "Zombie_insulator_armor1.png" = "DFD1B287C7946DE80361E4F7705BC2F9D17B5E0A8FE0390488A74D1A6F2A6851"
    "Zombie_insulator_armor2.png" = "7B6E75DC367D1474291A39E5787898F69B5C8015D73AC1EBE042680B7C749CE3"
    "Zombie_insulator_armor3.png" = "4C80529F3AB2F617A6F06CDBD53BD11F7C92606EC6681FE4E73A9B0226DD2531"
}
foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = (Get-FileHash (Join-Path $resolvedOutput $entry.Key) -Algorithm SHA256).Hash
    if ($actual -ne $entry.Value) {
        throw "Unexpected generated hash for $($entry.Key): $actual"
    }
}

Write-Output "Generated low-detail insulator armor stages in $resolvedOutput"
