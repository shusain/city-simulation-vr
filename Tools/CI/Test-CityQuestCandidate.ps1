[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ApkPath,
    [Parameter(Mandatory)][string]$StagedPakDirectory,
    [Parameter(Mandatory)][string]$ReportPath,
    [Parameter(Mandatory)][string]$AndroidSdkPath,
    [string]$JavaPath = 'C:/Program Files/Eclipse Adoptium/jdk-21.0.12.101-hotspot/bin/java.exe'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
& (Join-Path $PSScriptRoot '../Test-QuestPackage.ps1') -ApkPath $ApkPath -SdkPath $AndroidSdkPath -JavaPath $JavaPath
. (Join-Path $PSScriptRoot '../Distribution/Repository/tools/PackageRules.ps1')
$taskApkPath = (Resolve-Path -LiteralPath $ApkPath).Path
$taskStaged = @(Get-ChildItem -LiteralPath $StagedPakDirectory -File | Where-Object { $_.Extension -in @('.pak', '.utoc', '.ucas') })
if (-not $taskStaged.Count) { throw 'No staged Quest containers to compare.' }
$taskApk = [IO.Compression.ZipFile]::OpenRead($taskApkPath)
$taskMemory = [IO.MemoryStream]::new()
$taskPayloads = @()
try {
    $taskStream = $taskApk.GetEntry('assets/main.obb.png').Open()
    try { $taskStream.CopyTo($taskMemory) } finally { $taskStream.Dispose() }
    $taskMemory.Position = 0
    $taskObb = [IO.Compression.ZipArchive]::new($taskMemory, [IO.Compression.ZipArchiveMode]::Read, $true)
    try {
        $taskEmbedded = @($taskObb.Entries | Where-Object { $_.FullName -like 'CitySimVR/Content/Paks/*' })
        if ($taskEmbedded.Count -ne $taskStaged.Count) { throw 'Embedded/staged Quest container inventory differs.' }
        foreach ($taskFile in $taskStaged) {
            $taskEntry = $taskObb.GetEntry("CitySimVR/Content/Paks/$($taskFile.Name)")
            if (-not $taskEntry -or $taskEntry.Length -ne $taskFile.Length) { throw "Embedded Quest payload missing/different size: $($taskFile.Name)" }
            $taskStream = $taskEntry.Open()
            try { $taskEmbeddedHash = Get-CityStreamHash $taskStream } finally { $taskStream.Dispose() }
            $taskExpectedHash = (Get-FileHash -LiteralPath $taskFile.FullName -Algorithm SHA256).Hash
            if ($taskEmbeddedHash -ne $taskExpectedHash) { throw "Embedded Quest bytes differ from this cook: $($taskFile.Name)" }
            $taskPayloads += [ordered]@{ name=$taskFile.Name; bytes=$taskFile.Length; sha256=$taskExpectedHash.ToLowerInvariant() }
        }
    } finally { $taskObb.Dispose() }
} finally { $taskMemory.Dispose(); $taskApk.Dispose() }
$taskSigner = & $JavaPath -jar (Join-Path $AndroidSdkPath 'build-tools/36.1.0/lib/apksigner.jar') verify --print-certs $taskApkPath 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $taskSigner -notmatch 'Signer #1 certificate SHA-256 digest: ([a-fA-F0-9]{64})') { throw 'Quest signer fingerprint unavailable.' }
$taskSignerSha = $Matches[1].ToLowerInvariant()
[ordered]@{ status='passed'; apkSha256=(Get-FileHash -LiteralPath $taskApkPath -Algorithm SHA256).Hash.ToLowerInvariant();
    package='com.shaun.citysimvr.smoketest'; abi='arm64-v8a'; minSdk=32; targetSdk=35; signed=$true;
    signerSha256=$taskSignerSha; stagedPayloads=$taskPayloads; headset='unconfirmed-not-installed';
    verifiedUtc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ReportPath -Encoding utf8
Write-Output "Quest candidate verified against $($taskPayloads.Count) staged containers; receipt: $ReportPath"
