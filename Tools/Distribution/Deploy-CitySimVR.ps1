[CmdletBinding()]
param(
    [ValidateSet('Smoke', 'Sandbox')]
    [string]$Preset = 'Sandbox',
    [string]$Serial,
    [string]$ApkPath
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
$taskAdb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$taskPackage = 'com.shaun.citysimvr.smoketest'
$taskArchiveName = if ($Preset -eq 'Smoke') { 'QuestSmokeTest' } else { 'QuestSandbox' }

if (-not $ApkPath) {
    $taskApks = @(Get-ChildItem (Join-Path $taskRoot "Artifacts\$taskArchiveName") -Recurse -Filter *.apk)
    if ($taskApks.Count -ne 1) { throw "Specify -ApkPath: expected exactly one $Preset APK." }
    $ApkPath = $taskApks[0].FullName
}
if (-not (Test-Path -LiteralPath $ApkPath)) { throw "APK not found: $ApkPath" }
if (-not $Serial) {
    $taskDevices = @(& $taskAdb devices | Where-Object { $_ -match '^\S+\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
    if ($taskDevices.Count -ne 1) { throw 'Connect one authorized headset or specify -Serial.' }
    $Serial = $taskDevices[0]
}
& $taskAdb -s $Serial install -r $ApkPath
if ($LASTEXITCODE -ne 0) { throw 'APK installation failed.' }
$taskActivity = (& $taskAdb -s $Serial shell cmd package resolve-activity --brief $taskPackage | Select-Object -Last 1).Trim()
if ($taskActivity -notlike "$taskPackage/*") { throw "Could not resolve launch activity: $taskActivity" }
& $taskAdb -s $Serial shell am start -W -n $taskActivity
if ($LASTEXITCODE -ne 0) { throw 'Activity launch failed.' }
Write-Output "Installed and launched preset '$Preset'. Verify rendering and input inside the headset."
