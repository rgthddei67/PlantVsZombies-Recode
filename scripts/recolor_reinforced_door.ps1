param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Add-Type -AssemblyName System.Drawing

function Convert-ToReinforcedDoorColor {
    param(
        [Parameter(Mandatory)]
        [string]$SourcePath,

        [Parameter(Mandatory)]
        [string]$DestinationPath
    )

    $source = [System.Drawing.Bitmap]::new($SourcePath)
    try {
        $output = [System.Drawing.Bitmap]::new($source.Width, $source.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; $y++) {
                for ($x = 0; $x -lt $source.Width; $x++) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $output.SetPixel($x, $y, $pixel)
                        continue
                    }

                    $maximum = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                    $minimum = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                    $saturation = if ($maximum -eq 0) { 0.0 } else { ($maximum - $minimum) / [double]$maximum }
                    $value = $maximum / 255.0

                    # Recolor only neutral metal highlights; preserve the dark mesh, yellow handle, and rust details.
                    if ($saturation -le 0.32 -and $value -ge 0.22) {
                        $tealValue = [Math]::Min(0.94, 0.12 + 0.82 * $value)
                        $tealSaturation = 0.58
                        $chroma = $tealValue * $tealSaturation
                        $secondary = $chroma * (1.0 - [Math]::Abs((184.0 / 60.0) % 2.0 - 1.0))
                        $offset = $tealValue - $chroma

                        $red = [int][Math]::Round(255.0 * $offset)
                        $green = [int][Math]::Round(255.0 * ($secondary + $offset))
                        $blue = [int][Math]::Round(255.0 * ($chroma + $offset))
                        $pixel = [System.Drawing.Color]::FromArgb($pixel.A, $red, $green, $blue)
                    }

                    $output.SetPixel($x, $y, $pixel)
                }
            }

            $output.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $output.Dispose()
        }
    }
    finally {
        $source.Dispose()
    }
}

foreach ($preset in @("clang-release", "msvc-debug")) {
    $imageDirectory = Join-Path $RepositoryRoot "build/$preset/resources/image/reanim"
    foreach ($stage in 1..3) {
        Convert-ToReinforcedDoorColor `
            -SourcePath (Join-Path $imageDirectory "Zombie_screendoor$stage.png") `
            -DestinationPath (Join-Path $imageDirectory "Zombie_reinforced_screendoor$stage.png")
    }
}
