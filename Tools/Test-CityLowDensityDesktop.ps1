[CmdletBinding()]
param([string]$RunName='First',[string]$ExecutablePath,[int]$Width=1280,[int]$Height=800)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/LowDensity/Integration'
$taskUser=Join-Path $taskOut "Desktop-$RunName-User"
$taskLog=Join-Path $taskOut "Desktop-$RunName-Engine.log"
if(Test-Path -LiteralPath $taskLog){throw 'Use a fresh run name to preserve evidence'}
$taskExe=Join-Path $taskRoot 'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe'
if($ExecutablePath){$taskExe=(Resolve-Path -LiteralPath $ExecutablePath).Path}
$taskArgs=@('-CityDesktop','-nohmd','-d3d11','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout',"-UserDir=$taskUser","-abslog=$taskLog",'-ForceRes','-windowed',"-ResX=$Width","-ResY=$Height",'-ExecCmds=Automation RunTests CitySim.Desktop.LowDensity','-TestExit=Automation Test Queue Empty')
if($taskExe -like '*UnrealEditor*'){$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'/Game/CitySim/Maps/CitySandbox','-game')+$taskArgs}
$taskQuoted=$taskArgs|ForEach-Object{'"'+$_.Replace('"','\"')+'"'}
$taskProc=Start-Process -FilePath $taskExe -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskOut/Desktop-$RunName-Console.log" -RedirectStandardError "$taskOut/Desktop-$RunName-Stderr.log"
$taskHandle=$taskProc.Handle
if(-not $taskProc.WaitForExit(180000)){Stop-Process -Id $taskProc.Id;throw 'Owned desktop fixture timed out'}
$taskText=Get-Content $taskLog -Raw
if($taskProc.ExitCode -ne 0 -or $taskText -match 'Result=\{Fail\}|Fatal error|Assertion failed|LogD3D11RHI: Error' -or $taskText -notmatch 'CITYSIM_LOWDENSITY_DESKTOP_COMPLETE'){throw 'Low-density desktop check failed; inspect log'}
$taskImages=@(Get-ChildItem (Join-Path $taskUser 'Saved/Automation/LowDensity') -Filter '*.png')
if($taskImages.Count -ne 17){throw 'Expected seventeen actual world captures'}
Write-Output "CITYSIM_LOW_DENSITY_DESKTOP_PASS $taskLog"
