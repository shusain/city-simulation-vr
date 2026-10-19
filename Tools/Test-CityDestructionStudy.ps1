[CmdletBinding()]
param([string]$RunName='First')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D104/DestructionStudy'
if(Test-Path "$taskOut/$RunName-Engine.log"){throw 'Use a fresh run name'}
$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout',"-UserDir=$taskOut/$RunName-User","-abslog=$taskOut/$RunName-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0',"-ExecCmds=py $PSScriptRoot/Run-CityDestructionStudy.py")
$taskQuoted=$taskArgs|ForEach-Object{'"'+$_+'"'}
$taskProcess=Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskOut/$RunName-Console.log" -RedirectStandardError "$taskOut/$RunName-Stderr.log"
$taskHandle=$taskProcess.Handle
if(-not $taskProcess.WaitForExit(300000)){Stop-Process -Id $taskProcess.Id;throw 'Owned destruction study timed out'}
if($taskProcess.ExitCode -ne 0 -or -not(Test-Path "$taskOut/Simulation.json")){throw 'Destruction simulation report missing; inspect logs'}
Write-Output 'CITYSIM_DESTRUCTION_CHAOS_PASS'
