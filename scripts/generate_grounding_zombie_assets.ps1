param(
    [string]$ResourceRoot = (Join-Path $PSScriptRoot "../build/clang-release/resources")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$imageRoot = Join-Path $ResourceRoot "image/reanim"
$sourceReanim = Join-Path $ResourceRoot "reanim/ConeZombie.reanim"
$targetReanim = Join-Path $ResourceRoot "reanim/GroundingZombie.reanim"

function New-GroundingCone {
    param([int]$Stage)

    $sourcePath = Join-Path $imageRoot ("Zombie_cone{0}.png" -f $Stage)
    $source = [System.Drawing.Bitmap]::new($sourcePath)
    $target = [System.Drawing.Bitmap]::new(
        $source.Width, $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $source.Height; ++$y) {
            for ($x = 0; $x -lt $source.Width; ++$x) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.A -eq 0) { continue }
                $luma = (0.30 * $pixel.R + 0.58 * $pixel.G + 0.12 * $pixel.B) / 255.0
                $red = [Math]::Min(255, [int](24 + 74 * $luma))
                $green = [Math]::Min(255, [int](14 + 42 * $luma))
                $blue = [Math]::Min(255, [int](50 + 170 * $luma))
                $target.SetPixel($x, $y,
                    [System.Drawing.Color]::FromArgb($pixel.A, $red, $green, $blue))
            }
        }

        $graphics = [System.Drawing.Graphics]::FromImage($target)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $copperDark = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(255, 82, 40, 28), 2.6)
            $copper = [System.Drawing.Pen]::new(
                [System.Drawing.Color]::FromArgb(255, 220, 128, 55), 1.35)
            $violetGlow = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(175, 171, 91, 255))
            $violetCore = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(255, 239, 215, 255))
            $insulator = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(255, 48, 29, 79))
            try {
                # 贴图顶端空间很小，因此只画一根向左上倾斜的短天线；不会改变原路障的外接尺寸。
                $graphics.FillEllipse($insulator, 28.2, 3.4, 5.2, 3.2)
                $graphics.DrawLine($copperDark, 30.2, 4.5, 25.4, 0.9)
                $graphics.DrawLine($copper, 30.0, 4.1, 25.7, 1.0)
                $graphics.FillEllipse($violetGlow, 22.8, -1.1, 6.0, 5.2)
                $graphics.FillEllipse($violetCore, 24.5, 0.1, 2.5, 2.2)
            }
            finally {
                $copperDark.Dispose()
                $copper.Dispose()
                $violetGlow.Dispose()
                $violetCore.Dispose()
                $insulator.Dispose()
            }
        }
        finally {
            $graphics.Dispose()
        }

        $targetPath = Join-Path $imageRoot ("Zombie_grounding_cone{0}.png" -f $Stage)
        $target.Save($targetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $target.Dispose()
        $source.Dispose()
    }
}

for ($stage = 1; $stage -le 3; ++$stage) {
    New-GroundingCone -Stage $stage
}

$reanimText = [System.IO.File]::ReadAllText($sourceReanim)
$reanimText = $reanimText.Replace(
    "IMAGE_REANIM_ZOMBIE_CONE1", "IMAGE_REANIM_ZOMBIE_GROUNDING_CONE1")
[System.IO.File]::WriteAllText(
    $targetReanim, $reanimText, [System.Text.UTF8Encoding]::new($false))

$expectedHashes = @{
    "Zombie_grounding_cone1.png" = "FEFB05F27687B7616AB4E39589F6B9382DDAAA53F1DB8EDA7B9D7A609693CEF1"
    "Zombie_grounding_cone2.png" = "4CEA924F082BAEE08A378B9C825828E3CF630A22DF171533039CE67A2C18F3B2"
    "Zombie_grounding_cone3.png" = "DF0CA67586C27F6B1FBA5AE4D127E384A5C64B0AB8AB967B9080C7AD3F44C62B"
}
foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = (Get-FileHash (Join-Path $imageRoot $entry.Key) -Algorithm SHA256).Hash
    if ($actual -ne $entry.Value) {
        throw "Unexpected generated hash for $($entry.Key): $actual"
    }
}
$expectedReanimHash = "899C6FCAC8610C8A5DD57E0CE21C9BF561D8790EC2D92D3640A2E4A2CEA01B1F"
$actualReanimHash = (Get-FileHash $targetReanim -Algorithm SHA256).Hash
if ($actualReanimHash -ne $expectedReanimHash) {
    throw "Unexpected generated hash for GroundingZombie.reanim: $actualReanimHash"
}

Write-Output "Generated GroundingZombie cone stages and reanimation in $ResourceRoot"
