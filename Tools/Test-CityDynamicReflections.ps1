[CmdletBinding()]
param([ValidateSet('Import','Proof')][string]$Mode='Proof',[string]$RunName='First',[string]$ExecutablePath,[switch]$SequentialCubeFaces)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/LowDensity/DynamicReflections'
$taskLog="$taskOut/$Mode-$RunName-Engine.log"
if(Test-Path $taskLog){throw 'Use a fresh RunName'}
$taskExe=if($Mode -eq 'Import'){'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'}elseif($ExecutablePath){$ExecutablePath}else{"$taskRoot/Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe"}
$taskArgs=@('-CityDesktop','-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout',"-UserDir=$taskOut/$Mode-$RunName-User","-abslog=$taskLog",'-ForceRes','-windowed','-ResX=1280','-ResY=800')
if($Mode -eq 'Import'){$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject")+$taskArgs+@('-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0','-run=pythonscript',"-Script=$PSScriptRoot/Import-CityOriginalFactoryChimneys.py",'-AllowCommandletRendering')}
else{
 $taskFixture="$taskOut/$Mode-$RunName-User/Saved/Automation/DynamicReflections"
 New-Item -ItemType Directory -Path $taskFixture -Force | Out-Null
 Copy-Item -LiteralPath "$taskRoot/Artifacts/LowDensity/Integration/Desktop-Cylinders-User/Saved/Automation/LowDensity/Fixture.json" -Destination "$taskFixture/FixtureInput.json"
 if($taskExe -like '*UnrealEditor*'){$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'/Game/CitySim/Maps/CitySandbox','-game')+$taskArgs}
 $taskCommands=if($SequentialCubeFaces){'r.SceneCapture.CubeSinglePass 0,Automation RunTests CitySim.Desktop.DynamicReflections'}else{'Automation RunTests CitySim.Desktop.DynamicReflections'}
 $taskArgs+=@("-ExecCmds=$taskCommands",'-TestExit=Automation Test Queue Empty')
}
$taskQuoted=$taskArgs|ForEach-Object{'"'+$_+'"'}
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskOut/$Mode-$RunName-Console.log" -RedirectStandardError "$taskOut/$Mode-$RunName-Stderr.log"
$taskHandle=$taskProcess.Handle
if(-not $taskProcess.WaitForExit(180000)){Stop-Process -Id $taskProcess.Id;throw 'Owned reflection check timed out'}
if($taskProcess.ExitCode -ne 0){throw 'Reflection check failed; inspect log'}
if($Mode -eq 'Proof'){
 $taskText=Get-Content $taskLog -Raw
 if($taskText -match 'Result=\{Fail\}|Fatal error|Assertion failed|LogD3D11RHI: Error' -or $taskText -notmatch 'CITYSIM_DYNAMIC_REFLECTION_PROOF_COMPLETE'){throw 'Reflection proof failed'}
}
Write-Output "CITYSIM_D104_DYNAMIC_$($Mode.ToUpper())_PASS"
