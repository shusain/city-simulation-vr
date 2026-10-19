[CmdletBinding()]
param([string]$Editor='C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',
    [ValidateSet('All','Automatic','OrthographicBuild','PerspectiveBuild','PerspectiveControls','Polished','Compose')][string]$Mode='All',
    [string]$Python='C:/Users/shaun/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if($Mode -eq 'All'){
    foreach($taskMode in @('Automatic','OrthographicBuild','PerspectiveBuild','PerspectiveControls','Polished')){
        & $PSCommandPath -Editor $Editor -Python $Python -Mode $taskMode
    }
    & $PSCommandPath -Editor $Editor -Python $Python -Mode Compose
    return
}
if($Mode -eq 'Compose'){
    & $Python (Join-Path $PSScriptRoot 'Compose-CityReflectionStudy.py')
    if($LASTEXITCODE -ne 0){throw 'Reflection comparison verification failed'}
    return
}
$taskBuild=$Mode -ne 'Automatic'
$taskPerspective=$Mode -in @('PerspectiveBuild','PerspectiveControls','Polished')
$taskControls=$Mode -eq 'PerspectiveControls'
$taskPolished=$Mode -eq 'Polished'
$taskOut=Join-Path $taskRoot 'Artifacts/D104/Reflectivity'
if($taskBuild){$taskOut=Join-Path $taskOut $(if($taskPolished){'Polished'}elseif($taskControls){'PerspectiveControls'}elseif($taskPerspective){'Perspective'}else{'Built'})}
$taskProject=Join-Path $taskRoot 'Artifacts/D104/UnrealProject/CityPreview.uproject'
if(-not(Test-Path -LiteralPath $taskProject)){throw 'Run the D104 initial preview import first'}
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$taskReport=Join-Path $taskOut 'Report.json'
if(Test-Path -LiteralPath $taskReport){Remove-Item -LiteralPath $taskReport}
$taskArgs=@($taskProject,'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',
    "-abslog=$taskOut/Engine.log","-ExecCmds=py $PSScriptRoot/Capture-CityReflectiveMaterials.py")
if($taskBuild){$taskArgs+='-CityReflectionBuild'}
if($taskPerspective){$taskArgs+='-CityReflectionPerspective'}
if($taskControls){$taskArgs+='-CityReflectionControls'}
if($taskPolished){$taskArgs+='-CityReflectionPolished'}
$taskQuoted=$taskArgs | ForEach-Object {'"'+$_.Replace('"','\"')+'"'}
$taskProcess=Start-Process -FilePath $Editor -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOut 'Console.log') -RedirectStandardError (Join-Path $taskOut 'Stderr.log')
$taskHandle=$taskProcess.Handle
$taskProcess.WaitForExit()
if($taskProcess.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $taskReport)){throw 'Reflection study failed; inspect Engine.log/Error.txt'}
$taskResult=Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
if($taskResult.views.Count -ne $(if($taskPolished){4}elseif($taskBuild -and -not $taskControls){20}else{24})){throw 'Incomplete reflection study'}
Write-Output 'CITYSIM_REFLECTION_STUDY_PASS'
