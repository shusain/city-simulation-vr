[CmdletBinding()]
param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7',
    [string]$Python='C:\Users\shaun\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close Unreal before the focused reflection bake.'}
$taskEditor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject=Join-Path $taskRoot 'CitySimVR/CitySimVR.uproject'
& $Python (Join-Path $PSScriptRoot 'Prepare-CityRoomReflection.py')
if($LASTEXITCODE -ne 0){throw 'Room reflection source generation failed.'}
$taskPreviousCache=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try{
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $taskRoot 'Artifacts/D79/SurfaceDDC'),'Process')
    $taskCommon=@($taskProject,'-AllowCommandletRendering','-unattended','-nop4','-nosplash','-stdout','-nohmd','-nocef','-d3d11',
        '-ddc=InstalledNoZenLocalFallback',"-ShaderWorkingDir=$(Join-Path $taskRoot 'Artifacts/D79/SurfaceShaders')")
    & $taskEditor @taskCommon '-run=pythonscript' "-Script=$(Join-Path $PSScriptRoot 'Import-CityRoomReflection.py')"
    if($LASTEXITCODE -ne 0){throw 'Native room cube import failed.'}
    # Compile the Editor first. Only CitySandbox and its existing build-data registry
    # are saved; this adds one reflection, without a Lightmass lighting rebuild.
    $taskBakeReport=Join-Path $taskRoot 'Artifacts/D79-MetalD71/reflection-bake.json'
    if(Test-Path -LiteralPath $taskBakeReport){Remove-Item -LiteralPath $taskBakeReport}
    & $taskEditor @taskCommon "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'Bake-CityRoomReflection.py')" '-RenderOffscreen'
    if($LASTEXITCODE -ne 0 -or -not(Test-Path -LiteralPath (Join-Path $taskRoot 'Artifacts/D79-MetalD71/reflection-bake.json'))){throw 'CitySandbox room reflection bake failed.'}
}finally{[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPreviousCache,'Process')}
