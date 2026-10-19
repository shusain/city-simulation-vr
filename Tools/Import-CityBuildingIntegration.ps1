[CmdletBinding()]
param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close Unreal processes before the focused integration.'}
$taskEditor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject=Join-Path $taskRoot 'CitySimVR/CitySimVR.uproject'
$taskPreviousCache=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try{
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $taskRoot 'Artifacts/D79/SurfaceDDC'),'Process')
    foreach($taskImport in @(
        @{Script='Import-CityArtKit.py';Flag='-BuildingRefinementOnly';Report='Artifacts/AssetImport/D79-building-refinement-import.json'},
        @{Script='Update-CityStorefrontCatalog.py';Flag='-D79Integration';Report='Artifacts/D79/Integration/storefront-catalog.json'},
        @{Script='Import-CitySeasonalArt.py';Flag='-ResidentialGroundOnly';Report='Artifacts/AssetImport/D66-ResidentialGround-HomeImport.json'})){
        $taskReport=Join-Path $taskRoot $taskImport.Report
        if(Test-Path -LiteralPath $taskReport){Remove-Item -LiteralPath $taskReport}
        $taskPythonArgs=@('-run=pythonscript',"-Script=$(Join-Path $PSScriptRoot $taskImport.Script)",'-AllowCommandletRendering')
        if($taskImport.Script -eq 'Import-CitySeasonalArt.py'){
            # Section collision editing needs the initialized Editor subsystem.
            $taskPythonArgs=@("-ExecutePythonScript=$(Join-Path $PSScriptRoot $taskImport.Script)",'-RenderOffscreen',"-UserDir=$(Join-Path $taskRoot 'Artifacts/D79/Integration/ImportUser')")
        }
        & $taskEditor $taskProject @taskPythonArgs "-ShaderWorkingDir=$(Join-Path $taskRoot 'Artifacts/D79/SurfaceShaders')" '-ddc=InstalledNoZenLocalFallback' '-unattended' '-nop4' '-nosplash' '-stdout' '-nohmd' '-nosound' '-d3d11' $taskImport.Flag
        if($LASTEXITCODE -ne 0 -or -not(Test-Path -LiteralPath $taskReport)){throw "Focused import failed: $($taskImport.Script)"}
        $taskResult=Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
        if($taskResult.errors.Count){throw "Focused import reported errors: $($taskImport.Script)"}
    }
    # Apply the source paint/metal masks after any retained legacy/Interchange
    # settings on the earlier base-mesh pass. Materials and other meshes are reused.
    & (Join-Path $PSScriptRoot 'Import-CityBuildingSurfacePrototype.ps1') -MeshesOnly -Production -MechanicalOnly -EngineRoot $EngineRoot
    Write-Output 'CITYSIM_D79_BUILDING_INTEGRATION_PASS'
}finally{[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$taskPreviousCache,'Process')}
