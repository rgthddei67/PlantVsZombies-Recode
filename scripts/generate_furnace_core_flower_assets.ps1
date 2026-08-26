param(
	[string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$cardSource = Join-Path $RepositoryRoot 'docs/art/furnace-core-flower/FurnaceCoreFlower_card_source.png'
$coreSource = Join-Path $RepositoryRoot 'docs/art/furnace-core-flower/FurnaceCoreFlower_core_source.png'
$cardOutput = Join-Path $RepositoryRoot 'build/clang-release/resources/image/PlantImage/FurnaceCoreFlower.png'
$coreOutput = Join-Path $RepositoryRoot 'build/clang-release/resources/image/reanim/REANIM_FURNACECOREFLOWER_CORE.png'

$expectedCardSourceSha256 = 'AF03C1008A6C800544522500A6947A47395564AAA0D305581815EEAE2242F910' # ImageGen 原始炉芯花卡图的锁定 SHA-256
$expectedCoreSourceSha256 = 'CEEFA250270E36923671218B5A88773A09D8E95C415394B30BBAA86539CF1E87' # ImageGen 无火焰炉膛脸原图的锁定 SHA-256

function Assert-SourceHash([string]$Path, [string]$ExpectedHash) {
	$actualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
	if ($actualHash -ne $ExpectedHash) {
		throw "Asset source hash mismatch: $Path expected=$ExpectedHash actual=$actualHash"
	}
}

function Write-ScaledPng(
	[string]$SourcePath,
	[string]$OutputPath,
	[int]$Width,
	[int]$Height,
	[int]$DrawX,
	[int]$DrawY,
	[int]$DrawWidth,
	[int]$DrawHeight
) {
	$source = [System.Drawing.Image]::FromFile($SourcePath)
	try {
		$bitmap = New-Object System.Drawing.Bitmap $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
		try {
			$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
			try {
				$graphics.Clear([System.Drawing.Color]::Transparent)
				$graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
				$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
				$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
				$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
				$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
				$graphics.DrawImage($source, $DrawX, $DrawY, $DrawWidth, $DrawHeight)
			}
			finally {
				$graphics.Dispose()
			}
			$bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
		}
		finally {
			$bitmap.Dispose()
		}
	}
	finally {
		$source.Dispose()
	}
}

function Write-CroppedScaledPng(
	[string]$SourcePath,
	[string]$OutputPath,
	[int]$Width,
	[int]$Height,
	[int]$SourceX,
	[int]$SourceY,
	[int]$SourceWidth,
	[int]$SourceHeight
) {
	$source = [System.Drawing.Image]::FromFile($SourcePath)
	try {
		$bitmap = New-Object System.Drawing.Bitmap $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
		try {
			$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
			try {
				$graphics.Clear([System.Drawing.Color]::Transparent)
				$graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
				$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
				$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
				$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
				$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
				$destination = New-Object System.Drawing.Rectangle 0, 0, $Width, $Height
				$sourceRect = New-Object System.Drawing.Rectangle $SourceX, $SourceY, $SourceWidth, $SourceHeight
				$graphics.DrawImage($source, $destination, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
			}
			finally {
				$graphics.Dispose()
			}
			$bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
		}
		finally {
			$bitmap.Dispose()
		}
	}
	finally {
		$source.Dispose()
	}
}

Assert-SourceHash $cardSource $expectedCardSourceSha256
Assert-SourceHash $coreSource $expectedCoreSourceSha256

# 卡片原图已经按 6:5 构图；统一缩至经典 120x100 卡图尺寸。
Write-ScaledPng $cardSource $cardOutput 120 100 0 0 120 100
# 面部轨原图为 57x43；裁去生成原图透明边缘后缩入同尺寸，保持卡图炉膛脸身份。
Write-CroppedScaledPng $coreSource $coreOutput 57 43 80 120 1090 950

Write-Output "Generated $cardOutput"
Write-Output "Generated $coreOutput"
