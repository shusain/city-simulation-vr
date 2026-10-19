$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D88/Scale'
if (Test-Path "$taskOut/Import.json") { throw 'Preserve prior import evidence; verify the saved assets instead of rerunning import' }
$taskOwners=[regex]::Matches((Get-Content "$taskRoot/docs/DEVELOPMENT_TASKS.md" -Raw),'\*\*Native window OWNED BY ([A-Z]+\d+)\b')
if ($taskOwners.Count -ne 1 -or $taskOwners[0].Groups[1].Value -ne 'D88') { throw 'D88 exclusive native ownership is required' }
if (Get-Process UnrealEditor*,UnrealBuildTool*,AutomationTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Native process active; preserve it and wait' }
$taskPrior=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',"$taskRoot/Artifacts/D83/Integration/DDC",'Process')
    & 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$taskRoot/CitySimVR/CitySimVR.uproject" -run=pythonscript "-Script=$PSScriptRoot/Import-CitySportsParkScale.py" -AllowCommandletRendering '-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0' -ddc=InstalledNoZenLocalFallback -unattended -nop4 -nosplash -stdout -nocef -nohmd -nosound -d3d11 "-UserDir=$taskOut/ImportUser" "-abslog=$taskOut/Import-Engine.log" *> "$taskOut/Import.log"
} finally { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPrior,'Process') }
if ($LASTEXITCODE -ne 0 -or !(Test-Path "$taskOut/Import.json")) { throw 'Scale import failed; inspect evidence' }
$taskReport=Get-Content "$taskOut/Import.json" -Raw | ConvertFrom-Json
if ($taskReport.errors.Count -or $taskReport.assets.Count -ne 4) { throw 'Incomplete scale import' }
Write-Output 'CITYSIM_D88_SCALE_IMPORT_PASS 4'
