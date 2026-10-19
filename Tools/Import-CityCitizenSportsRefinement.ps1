param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskPlan=Get-Content -LiteralPath (Join-Path $taskRoot 'docs/DEVELOPMENT_TASKS.md') -Raw
$taskOwners=[regex]::Matches($taskPlan,'\*\*Native window OWNED BY ([A-Z]+\d+)\b')
if ($taskOwners.Count -ne 1 -or $taskOwners[0].Groups[1].Value -ne 'D88') { throw 'D88 exclusive native ownership is required' }
$taskProcesses=Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(UnrealEditor(?:-Cmd)?|UnrealBuildTool|ShaderCompileWorker|AutomationTool|CitySimVR(?:-Win64-Shipping)?)\.exe$' -or
    ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
}
if ($taskProcesses) { throw 'Another native process is active; preserve it and wait' }
$taskOut=Join-Path $taskRoot 'Artifacts/D88'
$taskCache=Join-Path $taskRoot 'Artifacts/D83/Integration/DDC'
$taskPrior=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskCache,'Process')
    & "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$taskRoot/CitySimVR/CitySimVR.uproject" -run=pythonscript "-Script=$PSScriptRoot/Import-CityCitizenSportsRefinement.py" -AllowCommandletRendering '-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0' -ddc=InstalledNoZenLocalFallback -unattended -nop4 -nosplash -stdout -nocef -nohmd -nosound -d3d11 "-UserDir=$taskOut/ImportUser" "-abslog=$taskOut/Import-Engine.log" *> "$taskOut/Import.log"
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$taskOut/Import.json")) { throw 'D88 import failed; inspect Import.log' }
    $taskResult=Get-Content "$taskOut/Import.json" -Raw | ConvertFrom-Json
    if ($taskResult.errors.Count -or $taskResult.assets.Count -ne 11) { throw 'D88 import report incomplete' }
    Write-Output 'CITYSIM_D88_NATIVE_IMPORT_PASS 11 assets'
} finally { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPrior,'Process') }
