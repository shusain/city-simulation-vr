[CmdletBinding()]
param([ValidateSet('Import','Capture','Tests','Bake','Signs')][string]$Mode='Tests',[string]$RunName='First',[switch]$ReuseMaterials,[switch]$BottomSigns)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/LowDensity/Integration'
$taskEditor='C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',"-UserDir=$taskOut/$Mode-$RunName-User","-abslog=$taskOut/$Mode-$RunName-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0')
if($Mode -eq 'Import'){$taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityLowDensityIntegration.py",'-AllowCommandletRendering')}
elseif($Mode -eq 'Capture'){$taskArgs+="-ExecCmds=py $PSScriptRoot/Capture-CityLowDensityIntegration.py"}
elseif($Mode -eq 'Bake'){$taskArgs+="-ExecCmds=py $PSScriptRoot/Bake-CityLowDensityReflection.py"}
elseif($Mode -eq 'Signs'){$taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Update-CityLowDensitySigns.py",'-AllowCommandletRendering')}
else{$taskArgs+=@('-ExecCmds=Automation RunTests CitySim.Art.LowDensityVariants+CitySim.Presentation.LowDensityLifecycle+CitySim.Presentation.StorefrontSavedIdentityAndPools+CitySim.Presentation.SeasonalDecorationsHomePorchAndDoorWreaths','-TestExit=Automation Test Queue Empty')}
if($ReuseMaterials){$taskArgs+='-D104ReuseMaterials'}
if($BottomSigns){$taskArgs+='-D104BottomSigns'}
$taskQuoted=$taskArgs|ForEach-Object{'"'+$_.Replace('"','\"')+'"'}
$taskProcess=Start-Process -FilePath $taskEditor -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskOut/$Mode-$RunName-Console.log" -RedirectStandardError "$taskOut/$Mode-$RunName-Stderr.log"
$taskHandle=$taskProcess.Handle
if(-not $taskProcess.WaitForExit(600000)){Stop-Process -Id $taskProcess.Id;throw 'Owned D104 process timed out'}
if($taskProcess.ExitCode -ne 0){throw "D104 $Mode failed; inspect logs"}
if($Mode -eq 'Tests'){
    $taskText=Get-Content "$taskOut/$Mode-$RunName-Engine.log" -Raw
    if($taskText -match 'Result=\{Fail\}' -or ([regex]::Matches($taskText,'Result=\{Success\}')).Count -ne 4){throw 'Expected four focused test successes'}
}else{
    $taskReport=Join-Path $taskOut $(if($Mode -eq 'Import'){'Import.json'}elseif($Mode -eq 'Bake'){'ReflectionBake.json'}elseif($Mode -eq 'Signs'){'Signs.json'}else{'Unreal/Report.json'})
    if(-not(Test-Path -LiteralPath $taskReport)){throw 'D104 report missing'}
}
Write-Output "CITYSIM_D104_$($Mode.ToUpper())_PASS"
