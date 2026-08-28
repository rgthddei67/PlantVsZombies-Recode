param(
    [string]$ResourceRoot = "build/clang-release/resources"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$resourcePath = [IO.Path]::GetFullPath((Join-Path $PWD $ResourceRoot))
$imagePath = Join-Path $resourcePath "image/reanim"
$cardPath = Join-Path $resourcePath "image/PlantImage"
$reanimPath = Join-Path $resourcePath "reanim"

$sources = [ordered]@{
    "image/PlantImage/Umbrellaleaf.png" = "7FC2196CAB14BF3BD15EDED1204D1E171EEF7178AF7EF1F198123F730A16E110"
    "reanim/Umbrellaleaf.reanim" = "C02E5C0C428E08FD7AC7BF4EBDB7FB94896465F80840F98FB2A4FA54AE8750CC"
    "image/reanim/Umbrellaleaf_body.png" = "0ECF941BD28C9429C8B7F15FC6628909635A32DAADBB67DCFD306A86E6EE9E64"
    "image/reanim/Umbrellaleaf_leaf1.png" = "93AC48C763C8263E80D9DA4040F2148208C21AD31A2CAE4408DEDDCFEE8103D6"
    "image/reanim/Umbrellaleaf_leaf2.png" = "A5D940A7BD8C81AD8D751D009ED07A75BBE12DA367ED283CCA9E2B01AEB5037D"
    "image/reanim/Umbrellaleaf_leaf3.png" = "ED831317C68AAC1B5F82067FBC58D4F07C31CE955F67B54779FE1CAE823FF377"
    "image/reanim/Umbrellaleaf_leaf4.png" = "BBC4C3201709C92DE4CB1873F41B9EE06E92E3A38E71381A25349ED1EB7D1ABF"
    "image/reanim/Umbrellaleaf_leaf5.png" = "1CC1090A04CA16B18A072614D245BDA0B0351290256B7465A757EB0304EEB07F"
    "image/reanim/Umbrellaleaf_leaf6.png" = "D43AB265E354F3E3F7CEDB5E1E62AF185EB9AE0B7B41596D974BEF9E4B726B9B"
    "image/reanim/Umbrellaleaf_leaf7.png" = "E889FE3729E714A72C470C737830019C8A99294DE1E08DCC559B7941649803F4"
}

$outputs = [ordered]@{
    "image/PlantImage/ListeningGrass.png" = "FE4E3224E1865E24527E9D7D2F143D27C11C45AF641A635FB66703E7B3B02DBB"
    "image/reanim/ListeningGrass_body.png" = "0473F180A230FD6A7410C820EABC40DD3D0169748AC5122BF7548D9FC59E5534"
    "image/reanim/ListeningGrass_leaf1.png" = "F7900226DC58EDB0068AED73099A175D1FDA69711A5465F9BE735AA3AA74C01D"
    "image/reanim/ListeningGrass_leaf2.png" = "E7663A9C930FDF1549D1450909B1AC0881F24A92F5207D21D309273488BAC92F"
    "image/reanim/ListeningGrass_leaf3.png" = "6C34A3B3FD1875D2651210244972E5C12D0F7B59C6678B24C3ADEEA23F2094D7"
    "image/reanim/ListeningGrass_leaf4.png" = "6BA4897CEFCB05A828EC681D46C75E4BD918B381256450BAEBC442C5F2BAB022"
    "image/reanim/ListeningGrass_leaf5.png" = "EF87CF79EB96465286F8AFD267D52F86FD474619ACB74CA19DC1E24FAFA9630F"
    "image/reanim/ListeningGrass_leaf6.png" = "DC472800C530D96CA9D9D1CA2A128494EA4D122317B7392310802B426A712507"
    "image/reanim/ListeningGrass_leaf7.png" = "E84D46AD4E1F2C6BBDEE755E4C004008D051D42EB5F10F3A886809B7846BF2D2"
    "reanim/ListeningGrass.reanim" = "2C2A86C50077E50AE83BAF7F473AAE6E5F80FDE74991786D1BE4CCB7149773CF"
}

function Assert-SourceHash([string]$relativePath, [string]$expectedHash) {
    $path = Join-Path $resourcePath $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing classic source asset: $relativePath"
    }
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
    if ($actualHash -ne $expectedHash) {
        throw "Classic source asset drifted: $relativePath expected=$expectedHash actual=$actualHash"
    }
}

function Clamp-Byte([double]$value) {
    return [byte][Math]::Max(0, [Math]::Min(255, [Math]::Round($value)))
}

function Convert-Pixel([Drawing.Color]$color, [string]$mode) {
    if ($color.A -eq 0) { return $color }
    $r = [double]$color.R
    $g = [double]$color.G
    $b = [double]$color.B
    $lum = 0.299 * $r + 0.587 * $g + 0.114 * $b

    # 保留原版墨线和五官对比；其余色阶映射到冷青叶片或薄荷色面部。
    if ($lum -lt 42) {
        return [Drawing.Color]::FromArgb($color.A,
            (Clamp-Byte (0.72 * $r)), (Clamp-Byte (0.82 * $g)),
            (Clamp-Byte ([Math]::Min(255, 0.98 * $b + 8))))
    }
    if ($mode -eq "body") {
        return [Drawing.Color]::FromArgb($color.A,
            (Clamp-Byte (40 + 0.58 * $lum)),
            (Clamp-Byte (70 + 0.67 * $lum)),
            (Clamp-Byte (82 + 0.68 * $lum)))
    }
    return [Drawing.Color]::FromArgb($color.A,
        (Clamp-Byte (8 + 0.28 * $lum)),
        (Clamp-Byte (48 + 0.64 * $lum)),
        (Clamp-Byte (70 + 0.72 * $lum)))
}

function Convert-ClassicPart(
    [string]$sourcePath,
    [string]$destinationPath,
    [string]$mode,
    [bool]$addFrost
) {
    $source = [Drawing.Bitmap]::FromFile($sourcePath)
    try {
        $output = [Drawing.Bitmap]::new($source.Width, $source.Height,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $top = $source.Height
            $bottom = -1
            for ($y = 0; $y -lt $source.Height; ++$y) {
                for ($x = 0; $x -lt $source.Width; ++$x) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -gt 0) {
                        $top = [Math]::Min($top, $y)
                        $bottom = [Math]::Max($bottom, $y)
                    }
                    $output.SetPixel($x, $y, (Convert-Pixel $pixel $mode))
                }
            }

            if ($addFrost -and $bottom -ge $top) {
                $frostLimit = $top + [Math]::Max(2, [int](($bottom - $top + 1) * 0.58))
                for ($y = $top; $y -le $frostLimit; ++$y) {
                    for ($x = 0; $x -lt $source.Width; ++$x) {
                        $original = $source.GetPixel($x, $y)
                        if ($original.A -eq 0) { continue }
                        $edge = $y -eq 0 -or $source.GetPixel($x, $y - 1).A -eq 0
                        if (-not $edge -and $x -gt 0) {
                            $edge = $source.GetPixel($x - 1, $y).A -eq 0
                        }
                        if (-not $edge -and $x + 1 -lt $source.Width) {
                            $edge = $source.GetPixel($x + 1, $y).A -eq 0
                        }
                        if (-not $edge) { continue }
                        $cold = $output.GetPixel($x, $y)
                        $output.SetPixel($x, $y, [Drawing.Color]::FromArgb(
                            $cold.A,
                            (Clamp-Byte (0.32 * $cold.R + 0.68 * 207)),
                            (Clamp-Byte (0.32 * $cold.G + 0.68 * 239)),
                            (Clamp-Byte (0.32 * $cold.B + 0.68 * 250))))
                    }
                }
            }
            $output.Save($destinationPath, [Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $output.Dispose() }
    }
    finally { $source.Dispose() }
}

foreach ($entry in $sources.GetEnumerator()) {
    Assert-SourceHash $entry.Key $entry.Value
}

Convert-ClassicPart (Join-Path $cardPath "Umbrellaleaf.png") `
    (Join-Path $cardPath "ListeningGrass.png") "leaf" $true
Convert-ClassicPart (Join-Path $imagePath "Umbrellaleaf_body.png") `
    (Join-Path $imagePath "ListeningGrass_body.png") "body" $false
foreach ($index in 1..7) {
    Convert-ClassicPart (Join-Path $imagePath "Umbrellaleaf_leaf$index.png") `
        (Join-Path $imagePath "ListeningGrass_leaf$index.png") "leaf" $true
}

$reanim = [IO.File]::ReadAllText((Join-Path $reanimPath "Umbrellaleaf.reanim"))
$reanim = $reanim.Replace("IMAGE_REANIM_UMBRELLALEAF_BODY", "IMAGE_REANIM_LISTENINGGRASS_BODY")
foreach ($index in 1..7) {
    $reanim = $reanim.Replace("IMAGE_REANIM_UMBRELLALEAF_LEAF$index",
        "IMAGE_REANIM_LISTENINGGRASS_LEAF$index")
}
$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $reanimPath "ListeningGrass.reanim"), $reanim, $utf8NoBom)

foreach ($entry in $outputs.GetEnumerator()) {
    $path = Join-Path $resourcePath $entry.Key
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
    if ($actualHash -ne $entry.Value) {
        throw "Generated output drifted: $($entry.Key) expected=$($entry.Value) actual=$actualHash"
    }
}

Write-Output "Generated ListeningGrass from the hash-locked classic Umbrellaleaf timeline and parts."
