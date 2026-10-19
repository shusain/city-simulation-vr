[CmdletBinding()]
param([switch]$MeshesOnly,[switch]$ReuseSurfaces,[switch]$Production,[switch]$MechanicalOnly,[string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskReport = Join-Path $taskRoot $(if($Production){'Artifacts\AssetImport\D79-surfaces-production.json'}else{'Artifacts\AssetImport\D79-surfaces-import.json'})
$taskCache = Join-Path $taskRoot 'Artifacts\D79\SurfaceDDC'
$taskShaders = Join-Path $taskRoot 'Artifacts\D79\SurfaceShaders'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskProject = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskScript = Join-Path $PSScriptRoot 'Import-CityBuildingSurfacePrototype.py'
if (Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue) { throw 'Close the other Unreal editor process before importing the prototype.' }
if (Test-Path -LiteralPath $taskReport) { Remove-Item -LiteralPath $taskReport }
New-Item -ItemType Directory -Path $taskCache -Force | Out-Null
New-Item -ItemType Directory -Path $taskShaders -Force | Out-Null
# Installed UE includes this supported file-cache fallback. The process-only
# override avoids requiring writes to AppData/Zen in a restricted session.
$taskPreviousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskCache,'Process')
    $taskExtraArgs = @()
    if ($MeshesOnly) { $taskExtraArgs += '-SurfaceMeshesOnly' }
    if ($ReuseSurfaces) { $taskExtraArgs += '-ReuseSurfaceAssets' }
    if ($Production) { $taskExtraArgs += '-PromoteSurfaceAssets' }
    if ($MechanicalOnly) { $taskExtraArgs += '-MechanicalMeshesOnly' }
    & $taskEditor $taskProject '-run=pythonscript' "-Script=$taskScript" '-AllowCommandletRendering' '-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0' "-ShaderWorkingDir=$taskShaders" '-ddc=InstalledNoZenLocalFallback' '-unattended' '-nop4' '-nosplash' '-stdout' '-nohmd' '-nosound' '-d3d11' @taskExtraArgs
    if ($LASTEXITCODE -ne 0) { throw "Surface prototype import exited $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath $taskReport)) { throw 'Surface prototype import did not produce its success report; inspect the Python log.' }
    $taskResult = Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
    Write-Output "CITYSIM_BUILDING_SURFACE_IMPORT_PASS $($taskResult.meshes.Count) meshes"
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPreviousCache,'Process')
}
