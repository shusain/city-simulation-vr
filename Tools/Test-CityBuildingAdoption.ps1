[CmdletBinding()]
param([ValidateSet('Import','Capture','Tests')][string]$Mode='Tests',[switch]$ReuseMaterials)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D91/Integration'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$taskEditor='C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskArgs=@("$taskRoot/CitySimVR/CitySimVR.uproject",'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',"-UserDir=$taskOut/$($Mode)User","-abslog=$taskOut/$Mode-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0')
if ($Mode -eq 'Import') {
    if (Test-Path -LiteralPath "$taskOut/Import.json") {throw 'D91 adoption already imported; preserve its proof and use Tests/Capture'}
    $taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityBuildingAdoption.py",'-AllowCommandletRendering')
    if ($ReuseMaterials) {$taskArgs+='-D91ReuseMaterials'}
}
elseif ($Mode -eq 'Capture') {$taskArgs+=@("-ExecCmds=py $PSScriptRoot/Capture-CityBuildingAdoption.py")}
else {
    $taskFilter='CitySim.Art.ImportedKit+CitySim.Art.CivicLightEmission+CitySim.Art.StorefrontAssets+CitySim.Presentation.SeasonalDecorationsHomePorchAndDoorWreaths+CitySim.Presentation.SeasonalDecorationsBoundedDeterministicReadOnly+CitySim.Presentation.SeasonalDecorationPoolsGroundingAndLifecycle+CitySim.Presentation.CitizenAssetsAndPlatform+CitySim.Utilities.PurchaseTimerAndResume+CitySim.Stations.PurchaseCompletionAndGates+CitySim.Utilities.AlternativeSupplyQualificationsBudgetAndDamage+CitySim.Education.SeparateStagesAndLegacyCompatibility'
    $taskArgs+=@("-ExecCmds=Automation RunTests $taskFilter",'-TestExit=Automation Test Queue Empty')
}
$taskLaunchArgs=($taskArgs | ForEach-Object {'"'+$_.Replace('"','\"')+'"'}) -join ' '
$taskProcess=Start-Process -FilePath $taskEditor -ArgumentList $taskLaunchArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskOut/$Mode-Console.log" -RedirectStandardError "$taskOut/$Mode-Stderr.log"
$taskProcess.Handle | Out-Null
if (!$taskProcess.WaitForExit(900000)) {Stop-Process -Id $taskProcess.Id;throw "D91 $Mode timed out"}
if ($taskProcess.ExitCode -ne 0) {throw "D91 $Mode exited $($taskProcess.ExitCode)"}
if ($Mode -eq 'Tests') {
    $taskLog=Get-Content "$taskOut/$Mode-Engine.log" -Raw
    if ($taskLog -match 'Result=\{Fail\}' -or ([regex]::Matches($taskLog,'Result=\{Success\}')).Count -ne 11) {throw 'Expected eleven focused native successes'}
} else {
    $taskReport=Join-Path $taskOut $(if($Mode -eq 'Import'){'Import.json'}else{'Unreal/Report.json'})
    if (!(Test-Path -LiteralPath $taskReport)) {throw 'Fresh D91 report missing'}
    $taskResult=Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
    if ($Mode -eq 'Import' -and $taskResult.meshes.Count -ne 61) {throw 'Expected61 reviewed meshes'}
    if ($Mode -eq 'Capture' -and $taskResult.views.Count -ne 30) {throw 'Expected30 native views'}
}
Write-Output "CITYSIM_D91_$($Mode.ToUpper())_PASS"
