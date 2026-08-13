[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$TestName,

    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$Preset = 'clang-release',

    [ValidatePattern('^[A-Za-z0-9.-]*$')]
    [string]$Server = $env:PVZ_ARTIFACT_SERVER,

    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$RemoteUser = 'codex',

    [string]$RemoteRoot = '/srv/pvz-artifacts/runs',

    [ValidateRange(1, 1024)]
    [int]$MaxSourceSizeMB = 250,

    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

# 远端路径会拼入 POSIX shell 命令，必须限制为无空格、无回退段的绝对路径。
if ($RemoteRoot -notmatch '^/[A-Za-z0-9._/-]+$' -or
    $RemoteRoot.Split('/') -contains '..') {
    throw 'RemoteRoot must be an absolute path without spaces or parent-directory segments.'
}
if (-not $DryRun -and [string]::IsNullOrWhiteSpace($Server)) {
    throw 'Specify -Server or set PVZ_ARTIFACT_SERVER before uploading.'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDirectory = Join-Path $repoRoot "build\$Preset\autotest\out\$TestName"
if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
    throw "AutoTest output directory does not exist: $sourceDirectory"
}

$requiredFiles = @('run.log', 'status.json')
foreach ($requiredFile in $requiredFiles) {
    $requiredPath = Join-Path $sourceDirectory $requiredFile
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required AutoTest evidence is missing: $requiredPath"
    }
}

$sourceFiles = @(Get-ChildItem -LiteralPath $sourceDirectory -Recurse -Force -File)
$sourceBytes = ($sourceFiles | Measure-Object -Property Length -Sum).Sum
if ($null -eq $sourceBytes) {
    $sourceBytes = 0
}
$sourceLimitBytes = [int64]$MaxSourceSizeMB * 1MB
if ($sourceBytes -gt $sourceLimitBytes) {
    throw "Evidence is $([math]::Round($sourceBytes / 1MB, 2)) MiB, above the $MaxSourceSizeMB MiB safety limit."
}

$gitCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the current Git commit.'
}
$gitBranch = (& git -C $repoRoot branch --show-current).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the current Git branch.'
}
$gitStatusLines = @(& git -C $repoRoot status --porcelain)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the current Git worktree status.'
}

$statusPath = Join-Path $sourceDirectory 'status.json'
$statusData = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
$fileRecords = foreach ($file in $sourceFiles) {
    $relativePath = $file.FullName.Substring($sourceDirectory.Length).TrimStart([char]'\', [char]'/')
    [ordered]@{
        path = $relativePath.Replace('\', '/')
        bytes = $file.Length
        sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$createdAtUtc = [DateTime]::UtcNow
$shortCommit = $gitCommit.Substring(0, [math]::Min(12, $gitCommit.Length))
$runId = '{0}-{1}-{2}-{3}' -f $createdAtUtc.ToString('yyyyMMddTHHmmssfffZ'), $Preset, $TestName, $shortCommit
$manifest = [ordered]@{
    schemaVersion = 1
    createdAtUtc = $createdAtUtc.ToString('o')
    runId = $runId
    preset = $Preset
    testName = $TestName
    source = "build/$Preset/autotest/out/$TestName"
    git = [ordered]@{
        commit = $gitCommit
        branch = $gitBranch
        dirty = ($gitStatusLines.Count -gt 0)
    }
    status = $statusData
    evidence = [ordered]@{
        fileCount = $sourceFiles.Count
        pngCount = @($sourceFiles | Where-Object Extension -EQ '.png').Count
        totalBytes = $sourceBytes
        files = @($fileRecords)
    }
}

# 先在唯一临时目录内完成复制、清单和压缩，源证据目录始终只读。
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('pvz-autotest-evidence-' + [guid]::NewGuid().ToString('N'))
$payloadDirectory = Join-Path $temporaryRoot 'evidence'
$archivePath = Join-Path $temporaryRoot 'evidence.zip'
$manifestPath = Join-Path $payloadDirectory 'manifest.json'

try {
    New-Item -ItemType Directory -Path $payloadDirectory -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $sourceDirectory -Force) {
        Copy-Item -LiteralPath $item.FullName -Destination $payloadDirectory -Recurse -Force
    }
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Compress-Archive -LiteralPath $payloadDirectory -DestinationPath $archivePath -CompressionLevel Optimal

    $archiveBytes = (Get-Item -LiteralPath $archivePath).Length
    Write-Host "Prepared $runId ($($sourceFiles.Count) source files, $([math]::Round($archiveBytes / 1MB, 2)) MiB archive)."
    if ($manifest.evidence.pngCount -eq 0) {
        Write-Warning 'This AutoTest output has no PNG screenshot; log and status evidence will still be archived.'
    }

    if ($DryRun) {
        Write-Host 'Dry run completed; no remote directory was created and no data was uploaded.'
        return
    }

    $remoteTarget = "$RemoteUser@$Server"
    $remoteDirectory = "$($RemoteRoot.TrimEnd('/'))/$runId"
    $createCommand = "install -d -m 750 -- '$remoteDirectory'"
    Invoke-NativeCommand -FilePath 'ssh' -Arguments @('-o', 'BatchMode=yes', $remoteTarget, $createCommand)

    $destination = "${remoteTarget}:$remoteDirectory/"
    Invoke-NativeCommand -FilePath 'scp' -Arguments @('-q', $archivePath, $manifestPath, $destination)

    $verifyCommand = "set -eu; cd '$remoteDirectory'; test -s evidence.zip; test -s manifest.json; sha256sum evidence.zip manifest.json > SHA256SUMS; chmod 640 evidence.zip manifest.json SHA256SUMS"
    Invoke-NativeCommand -FilePath 'ssh' -Arguments @('-o', 'BatchMode=yes', $remoteTarget, $verifyCommand)
    Write-Host "Uploaded and verified: $remoteTarget`:$remoteDirectory"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
