param(
    [string]$ResourceRoot = (Join-Path $PSScriptRoot "../build/clang-release/resources")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$artRoot = Join-Path $PSScriptRoot "../docs/art/ice-statue-executioner"
$maulSource = Join-Path $artRoot "ice-execution-maul-source-v1.png"
$shellSource = Join-Path $artRoot "ice-statue-shell-source-v1.png"
$imageRoot = Join-Path $ResourceRoot "image/reanim"
$expectedMaulSha256 = "D20FC9EA583F281063AF6E934FFAECDDF61D1DC0DE108E17A55FF148294C85F0"
$expectedShellSha256 = "20A331E18B821C8349A364E4C14EAAE34AA3BF82943021F49CB0A119234B90C5"

function Assert-SourceHash {
    param([string]$Path, [string]$Expected)
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actual -ne $Expected) {
        throw "Source hash mismatch: $Path`nexpected=$Expected`nactual=$actual"
    }
}

function Export-ScaledPng {
    param(
        [string]$Source,
        [string]$Destination,
        [int]$Width,
        [int]$Height
    )
    $inputBitmap = [System.Drawing.Bitmap]::new($Source)
    $outputBitmap = [System.Drawing.Bitmap]::new(
        $Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($outputBitmap)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.DrawImage($inputBitmap, 0, 0, $Width, $Height)
        }
        finally { $graphics.Dispose() }
        $outputBitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $outputBitmap.Dispose()
        $inputBitmap.Dispose()
    }
}

Assert-SourceHash -Path $maulSource -Expected $expectedMaulSha256
Assert-SourceHash -Path $shellSource -Expected $expectedShellSha256
New-Item -ItemType Directory -Force -Path $imageRoot | Out-Null

# 扶梯锤轨以贴图中心为旋转轴；方形母图保持双头冰锤的握柄中心，不做运行时坐标补偿。
Export-ScaledPng -Source $maulSource `
    -Destination (Join-Path $imageRoot "Zombie_ice_executioner_maul.png") `
    -Width 86 -Height 84

# 冰壳由 Plant::Draw 按固定 112x120 画面尺寸缩放；源文件只保留透明像素与清晰轮廓。
Export-ScaledPng -Source $shellSource `
    -Destination (Join-Path $imageRoot "Ice_statue_shell.png") `
    -Width 112 -Height 120

Write-Host "Generated Ice Statue Executioner assets in $imageRoot"
