param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Add-Type -AssemblyName System.Drawing

$sourcePath = Join-Path $RepositoryRoot "build/clang-release/resources/image/PlantImage/ScaredyShroom.png"
$destinationPath = Join-Path $RepositoryRoot "build/clang-release/resources/image/PlantImage/EliteScaredyShroom.png"

$source = [System.Drawing.Bitmap]::new($sourcePath)
try {
    $output = [System.Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.A -eq 0) {
                    $output.SetPixel($x, $y, $pixel)
                    continue
                }

                # 用原亮度重建紫色材质，再与原色混合，保留阴影、高光、描边和透明度。
                $luminance = 0.299 * $pixel.R + 0.587 * $pixel.G + 0.114 * $pixel.B
                $targetRed = [Math]::Min(255.0, $luminance * 1.32 + 38.0)
                $targetGreen = [Math]::Min(255.0, $luminance * 0.24)
                $targetBlue = [Math]::Min(255.0, $luminance * 1.62 + 55.0)
                $blend = if ($luminance -lt 35.0) { 0.36 } else { 0.82 }

                $red = [int][Math]::Round($pixel.R * (1.0 - $blend) + $targetRed * $blend)
                $green = [int][Math]::Round($pixel.G * (1.0 - $blend) + $targetGreen * $blend)
                $blue = [int][Math]::Round($pixel.B * (1.0 - $blend) + $targetBlue * $blend)
                $output.SetPixel($x, $y,
                    [System.Drawing.Color]::FromArgb($pixel.A, $red, $green, $blue))
            }
        }

        $output.Save($destinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $output.Dispose()
    }
}
finally {
    $source.Dispose()
}

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash
$destinationHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destinationPath).Hash
Write-Output "source=$sourceHash"
Write-Output "elite=$destinationHash"
