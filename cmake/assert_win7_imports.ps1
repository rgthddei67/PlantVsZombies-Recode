param(
    [Parameter(Mandatory = $true)]
    [string]$LlvmReadObj,

    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [Parameter(Mandatory = $true)]
    [string]$Win7ExportMap
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($requiredFile in @($LlvmReadObj, $Binary, $Win7ExportMap)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required Win7 import-audit file is missing: $requiredFile"
    }
}

# YY-Thunks 随包导出表按 INI 分节记录 DLL 与序号/名称；同时保存名称和序号，
# 以覆盖普通 name import 及 OLEAUT32 等 ordinal import。
$win7Exports = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$currentDll = $null
foreach ($line in Get-Content -LiteralPath $Win7ExportMap) {
    if ($line -match '^\[([^\]]+)\]$') {
        $currentDll = $matches[1]
        continue
    }
    if ($null -ne $currentDll -and $line -match '^(\d+)=(.*)$') {
        $ordinal = $matches[1]
        $symbol = $matches[2]
        [void]$win7Exports.Add("$currentDll|#$ordinal")
        if (-not [string]::IsNullOrWhiteSpace($symbol)) {
            [void]$win7Exports.Add("$currentDll|$symbol")
        }
    }
}

$importDump = & $LlvmReadObj --coff-imports $Binary 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "llvm-readobj failed for '$Binary':`n$($importDump -join [Environment]::NewLine)"
}

$missingImports = [System.Collections.Generic.List[string]]::new()
$currentImportDll = $null
$importCount = 0
foreach ($line in $importDump) {
    if ($line -match '^\s*Name:\s*(\S+)\s*$') {
        $currentImportDll = $matches[1]
        continue
    }
    if ($null -ne $currentImportDll -and
        $line -match '^\s*Symbol:\s*(.*?)\s*\((\d+)\)\s*$') {
        ++$importCount
        $symbol = $matches[1].Trim()
        $ordinal = $matches[2]
        $lookup = if ([string]::IsNullOrEmpty($symbol)) {
            "$currentImportDll|#$ordinal"
        }
        else {
            "$currentImportDll|$symbol"
        }

        if (-not $win7Exports.Contains($lookup)) {
            $displaySymbol = if ([string]::IsNullOrEmpty($symbol)) { "ordinal $ordinal" } else { $symbol }
            $missingImports.Add("$currentImportDll!$displaySymbol")
        }
    }
}

if ($importCount -eq 0) {
    throw "No PE imports were parsed from '$Binary'; the Win7 audit cannot prove compatibility."
}
if ($missingImports.Count -gt 0) {
    $details = ($missingImports | Sort-Object -Unique) -join [Environment]::NewLine
    throw "'$Binary' imports APIs absent from Windows 7 x64:`n$details"
}

Write-Host "Win7 import audit passed: $Binary ($importCount imports checked)"
