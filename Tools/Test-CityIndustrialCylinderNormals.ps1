[CmdletBinding()]
param([ValidateSet('Import','Before','After')][string]$Mode='Import',[string]$RunName,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.7')
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Common/CityTools.psm1') -Force
$taskRoot=Get-CityRepositoryRoot -StartPath $PSCommandPath
$taskOut=Join-Path $taskRoot 'Artifacts/D104/CylinderShading'
$taskEditor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if(-not $RunName){$RunName=$Mode}
if(Test-Path "$taskOut/$RunName-Engine.log"){throw 'Preserve existing run evidence; use a fresh RunName'}
$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',"-UserDir=$taskOut/$RunName-User","-abslog=$taskOut/$RunName-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0')
if($Mode -eq 'Import'){$taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityIndustrialCylinderNormals.py",'-AllowCommandletRendering')}
else{$taskArgs+=@("-D104Cylinder$Mode","-ExecCmds=py $PSScriptRoot/Capture-CityLowDensityIntegration.py")}
$taskProcess=Invoke-CityToolProcess -FilePath $taskEditor -ArgumentList $taskArgs -LogPrefix "$taskOut/$RunName" -TimeoutSeconds 180
$taskReport=if($Mode -eq 'Import'){"$taskOut/Import.json"}else{"$taskOut/$($Mode)ChimneysFinal/Report.json"}
$null=Read-CityToolReport -Path $taskReport -StartedUtc $taskProcess.StartedUtc
Write-Output "CITYSIM_D104_CYLINDER_$($Mode.ToUpper())_PASS"
