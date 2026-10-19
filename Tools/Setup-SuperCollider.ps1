[CmdletBinding()]
param([switch]$Download)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskDir = Join-Path $taskRoot 'Artifacts/Toolchain/SuperCollider'
$taskZip = Join-Path $taskDir 'SuperCollider-3.14.1-win64.zip'
$taskRuntime = Join-Path $taskDir 'SuperCollider'
$taskExpectedHash = 'a5f95416307d35c039ca53a9f9c6151c26585064d3229521093a60dddb9458cd'
$taskUrl = 'https://github.com/supercollider/supercollider/releases/download/Version-3.14.1/SuperCollider-3.14.1-win64.zip'
New-Item -ItemType Directory -Path $taskDir -Force | Out-Null
if (-not (Test-Path -LiteralPath $taskZip)) {
    if (-not $Download) { throw 'Portable archive missing. Run Tools/Setup-SuperCollider.ps1 -Download once.' }
    Invoke-WebRequest -Uri $taskUrl -OutFile $taskZip
}
$taskHash = (Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($taskHash -ne $taskExpectedHash) { throw 'SuperCollider archive SHA256 mismatch; refusing to unpack or execute.' }
if (-not (Test-Path -LiteralPath $taskRuntime)) {
    Expand-Archive -LiteralPath $taskZip -DestinationPath $taskDir
}
$taskSclang = Join-Path $taskRuntime 'sclang.exe'
$taskScsynth = Join-Path $taskRuntime 'scsynth.exe'
$taskVersions = @()
foreach ($taskBinary in @($taskSclang, $taskScsynth)) {
    if (-not (Test-Path -LiteralPath $taskBinary)) { throw "Missing runtime binary: $taskBinary" }
    $taskVersion = (& $taskBinary -v | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $taskVersion -notmatch '3\.14\.1') { throw "Unexpected runtime: $taskVersion" }
    $taskVersions += $taskVersion
}
[ordered]@{
    version = '3.14.1'
    source = $taskUrl
    archiveSha256 = $taskHash
    sclang = $taskSclang
    scsynth = $taskScsynth
    versions = $taskVersions
    binaries = @($taskSclang, $taskScsynth) | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant() }
    }
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskDir 'Setup.json') -Encoding utf8
Write-Output "SUPERCOLLIDER_READY $taskSclang"
