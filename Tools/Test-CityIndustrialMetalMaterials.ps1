[CmdletBinding()]
param([ValidateSet('Import','Before','After')][string]$Mode='After',[string]$RunName='First',
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.7')
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Common/CityTools.psm1') -Force
$taskRoot=Get-CityRepositoryRoot -StartPath $PSCommandPath
$taskOut=Join-Path $taskRoot 'Artifacts/D104/IndustrialMetal'
$taskLog="$taskOut/$Mode-$RunName-Engine.log"
if(Test-Path -LiteralPath $taskLog){throw 'Use a fresh RunName'}
$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',"-UserDir=$taskOut/$Mode-$RunName-User","-abslog=$taskLog",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0')
if($Mode -eq 'Import'){$taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityIndustrialMetalMaterials.py",'-AllowCommandletRendering')}
else{$taskArgs+=@("-D104IndustrialMetal$Mode","-ExecCmds=py $PSScriptRoot/Capture-CityLowDensityIntegration.py")}
$taskEditor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProcess=Invoke-CityToolProcess -FilePath $taskEditor -ArgumentList $taskArgs -LogPrefix "$taskOut/$Mode-$RunName" -TimeoutSeconds 360
$taskReport=if($Mode -eq 'Import'){"$taskOut/Import.json"}else{"$taskOut/$Mode/Report.json"}
$null=Read-CityToolReport -Path $taskReport -StartedUtc $taskProcess.StartedUtc
Write-Output "CITYSIM_INDUSTRIAL_METAL_$($Mode.ToUpper())_PASS"
