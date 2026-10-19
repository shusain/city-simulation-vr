param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process owns the window' }
$taskCache=Join-Path $taskRoot 'Artifacts/D83/Integration/DDC'
$taskShaders=Join-Path $taskRoot 'Artifacts/D83/Integration/Shaders'
New-Item -ItemType Directory -Path $taskCache,$taskShaders -Force | Out-Null
$taskPrior=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskCache,'Process')
    & "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$taskRoot/CitySimVR/CitySimVR.uproject" -run=pythonscript "-Script=$PSScriptRoot/Import-CityParkIntegration.py" -AllowCommandletRendering '-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0' "-ShaderWorkingDir=$taskShaders" -ddc=InstalledNoZenLocalFallback -unattended -nop4 -nosplash -stdout -nocef -nohmd -nosound -d3d11 "-abslog=$taskRoot/Artifacts/D83/Integration/Import-Engine.log" *> "$taskRoot/Artifacts/D83/Integration/Import.log"
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$taskRoot/Artifacts/D83/Integration/Import.json")) { throw "D83 native import failed ($LASTEXITCODE); inspect Integration/Import.log" }
    $taskResult=Get-Content "$taskRoot/Artifacts/D83/Integration/Import.json" -Raw | ConvertFrom-Json
    if ($taskResult.errors.Count) { throw 'D83 importer reported errors' }
    Write-Output 'CITYSIM_D83_NATIVE_IMPORT_PASS'
} finally { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPrior,'Process') }
