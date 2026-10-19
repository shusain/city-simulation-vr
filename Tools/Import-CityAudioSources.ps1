[CmdletBinding()]
param(
    [switch]$Import,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [string]$Python = 'C:\Users\shaun\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
# Default mode is source validation only and does not launch Unreal.
& $Python (Join-Path $PSScriptRoot 'Stage-CityAudioSources.py') --check
if ($LASTEXITCODE -ne 0) { throw 'D58 source staging validation failed' }
if (-not $Import) { Write-Output 'D58_NATIVE_IMPORT_NOT_STARTED_VALIDATE_ONLY'; return }
$taskPlan = Get-Content -LiteralPath (Join-Path $taskRoot 'docs/DEVELOPMENT_TASKS.md') -Raw
$taskOwners = [regex]::Matches($taskPlan, '\*\*Native window OWNED BY ([A-Z]+\d+)\b')
if ($taskOwners.Count -ne 1 -or $taskOwners[0].Groups[1].Value -ne 'D58') {
    $taskNames = ($taskOwners | ForEach-Object { $_.Groups[1].Value }) -join ', '
    throw "D58 native import blocked: recorded owner is '$taskNames'. Wait for explicit release and record a D58 claim."
}
$taskProcesses = Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor(?:-Cmd)?|UnrealBuildTool|ShaderCompileWorker|AutomationTool|CitySimVR(?:-Win64-Shipping)?)\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
}
if ($taskProcesses) { throw "D58 native import blocked: competing native process IDs: $(($taskProcesses.ProcessId) -join ', ')" }
$taskEditor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject = Join-Path $taskRoot 'CitySimVR/CitySimVR.uproject'
$taskReport = Join-Path $taskRoot 'Artifacts/D58/UnrealImport/ImportReport.json'
if (Test-Path -LiteralPath $taskReport) { throw 'An import report already exists; verify/retain it before running another native import' }
& $taskEditor $taskProject '-run=pythonscript' "-Script=$(Join-Path $PSScriptRoot 'Import-CityAudioSources.py')" `
    "-UserDir=$(Join-Path $taskRoot 'Artifacts/D58/UnrealImport/User')" `
    "-ShaderWorkingDir=$(Join-Path $taskRoot 'Artifacts/D58/UnrealImport/Shaders')" `
    '-ddc=InstalledNoZenLocalFallback' '-unattended' '-nop4' '-nosplash' '-stdout' '-nohmd' '-AllowCommandletAudio' '-AudioMixer' `
    "-abslog=$(Join-Path $taskRoot 'Artifacts/D58/UnrealImport/Import.log')"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $taskReport)) { throw 'Native D58 import failed; inspect the dedicated log without modifying other sessions' }
$taskResult = Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
if ($taskResult.status -ne 'Pass' -or $taskResult.assets.Count -ne 50) { throw 'Incomplete native D58 import report' }
Write-Output 'D58_NATIVE_SOUNDWAVE_IMPORT_PASS assets=50'
