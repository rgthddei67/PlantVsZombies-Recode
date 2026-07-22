param(
    [Parameter(Mandatory = $true)]
    [string]$CanonicalRoot,

    [Parameter(Mandatory = $true)]
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'

$canonicalRootPath = [System.IO.Path]::GetFullPath($CanonicalRoot)
$buildRootPath = [System.IO.Path]::GetFullPath($BuildRoot)

if (-not (Test-Path -LiteralPath $canonicalRootPath -PathType Container)) {
    throw "Canonical runtime directory does not exist: $canonicalRootPath"
}

New-Item -ItemType Directory -Path $buildRootPath -Force | Out-Null

foreach ($assetName in @('resources', 'font')) {
    $targetPath = Join-Path $canonicalRootPath $assetName
    $linkPath = Join-Path $buildRootPath $assetName

    if (-not (Test-Path -LiteralPath $targetPath -PathType Container)) {
        throw "Canonical runtime asset directory does not exist: $targetPath"
    }

    if (Test-Path -LiteralPath $linkPath) {
        $existing = Get-Item -LiteralPath $linkPath -Force
        if (($existing.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            $targetMatches = $false
            foreach ($existingTarget in @($existing.Target)) {
                if ([System.IO.Path]::GetFullPath($existingTarget) -eq $targetPath) {
                    $targetMatches = $true
                    break
                }
            }
            if ($targetMatches) {
                continue
            }

            throw "Directory junction points to the wrong target: $linkPath"
        }

        throw "Destination exists and is not a directory junction: $linkPath"
    }

    New-Item -ItemType Junction -Path $linkPath -Target $targetPath | Out-Null
    Write-Output "Shared runtime asset: $assetName -> $targetPath"
}
