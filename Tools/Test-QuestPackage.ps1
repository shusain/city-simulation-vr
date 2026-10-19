[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ApkPath,
    [string]$SdkPath = (Join-Path $env:LOCALAPPDATA 'Android/Sdk'),
    [string]$JavaPath = 'C:/Program Files/Eclipse Adoptium/jdk-21.0.12.101-hotspot/bin/java.exe'
)
$ErrorActionPreference = 'Stop'
$taskApk = (Resolve-Path -LiteralPath $ApkPath).Path
$taskBuildTools = Join-Path $SdkPath 'build-tools/36.1.0'
. (Join-Path $PSScriptRoot 'Distribution/Repository/tools/PackageRules.ps1')
Test-CityQuestPayload $taskApk
$taskBadging = & (Join-Path $taskBuildTools 'aapt.exe') dump badging $taskApk 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $taskBadging -notmatch "package: name='com.shaun.citysimvr.smoketest'" -or
    $taskBadging -notmatch "(?m)^sdkVersion:'32'" -or $taskBadging -notmatch "(?m)^targetSdkVersion:'35'" -or
    $taskBadging -notmatch "(?m)^native-code: 'arm64-v8a'\s*$") { throw 'Quest package identity, API level or ARM64 ABI is incorrect.' }
$taskSignature = & $JavaPath -jar (Join-Path $taskBuildTools 'lib/apksigner.jar') verify --verbose --print-certs $taskApk 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $taskSignature -notmatch 'Verified using v2 scheme.*true') { throw 'Quest APK signature verification failed.' }
Write-Output "Quest APK verified: com.shaun.citysimvr.smoketest, ARM64, min32/target35, signed, embedded cooked payload; SHA256 $((Get-FileHash -LiteralPath $taskApk).Hash). Headset behavior remains a separate check."
