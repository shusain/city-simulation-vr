[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BatchFile,
    [ValidateSet('All','Import','Capture','Compose')][string]$Mode='All',
    [string]$Editor='C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',
    [string]$Python=(Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskBatchPath=(Resolve-Path -LiteralPath $BatchFile).Path
$taskBatch=Get-Content -LiteralPath $taskBatchPath -Raw | ConvertFrom-Json
$taskProject=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskBatch.project))
$taskOut=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskBatch.output))
$taskAllowed=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Artifacts'))+[IO.Path]::DirectorySeparatorChar
if (-not $taskProject.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase) -or -not $taskOut.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Use a separate project/output inside Artifacts' }
if ($Mode -in @('All','Import')) {
    $taskProtected=@{}
    foreach ($taskFolder in @('Materials','Textures')) {
        Get-ChildItem -LiteralPath (Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskFolder") -File | ForEach-Object { $taskProtected[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    }
    foreach ($taskAsset in @('PC/Materials/M_CityCitizenPalette.uasset','PC/Meshes/SM_Citizen_PC_Casual.uasset')) {
        $taskPath=Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskAsset"
        $taskProtected[$taskPath]=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash
    }
    $taskProtected | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskOut 'ProtectedInputs.json')
    $taskProjectDir=Split-Path $taskProject -Parent
    New-Item -ItemType Directory -Path (Join-Path $taskProjectDir 'Config') -Force | Out-Null
    foreach ($taskFolder in @('Materials','Textures')) {
        $taskTo=Join-Path $taskProjectDir "Content/CitySim/Art/$taskFolder"
        New-Item -ItemType Directory -Path $taskTo -Force | Out-Null
        Get-ChildItem -LiteralPath (Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskFolder") -File | Copy-Item -Destination $taskTo -Force
    }
    foreach ($taskAsset in @('PC/Materials/M_CityCitizenPalette.uasset','PC/Meshes/SM_Citizen_PC_Casual.uasset')) {
        $taskTo=Join-Path $taskProjectDir "Content/CitySim/Art/$taskAsset"
        New-Item -ItemType Directory -Path (Split-Path $taskTo -Parent) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskAsset") -Destination $taskTo -Force
    }
    '{"FileVersion":3,"EngineAssociation":"5.7","Plugins":[{"Name":"PythonScriptPlugin","Enabled":true},{"Name":"EditorScriptingUtilities","Enabled":true}]}' | Set-Content -LiteralPath $taskProject
    @('[SystemSettings]','r.ForwardShading=1','r.ReflectionMethod=0','r.DynamicGlobalIlluminationMethod=0','r.DefaultFeature.AutoExposure=0','r.DefaultFeature.MotionBlur=0') | Set-Content -LiteralPath (Join-Path $taskProjectDir 'Config/DefaultEngine.ini')
}
foreach ($taskStage in @('Import','Capture')) {
    if ($Mode -notin @('All',$taskStage)) { continue }
    $taskReport=Join-Path $taskOut $(if($taskStage -eq 'Import'){'Import.json'}else{'Unreal/Report.json'})
    # Removing only this receipt prevents stale captures from being reported as a pass.
    if (Test-Path -LiteralPath $taskReport) { Remove-Item -LiteralPath $taskReport }
    $taskArgs=@($taskProject,'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',
        "-abslog=$taskOut/$taskStage-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0',"-CityPreviewBatch=$taskBatchPath")
    if ($taskStage -eq 'Import') { $taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityUnrealPreview.py",'-AllowCommandletRendering') }
    else { $taskArgs+="-ExecCmds=py $PSScriptRoot/Render-CityUnrealPreviews.py" }
    # A hidden helper and separate streams avoid telemetry stderr becoming a PowerShell exception.
    $taskQuoted=$taskArgs | ForEach-Object { '"'+$_.Replace('"','\"')+'"' }
    $taskProcess=Start-Process -FilePath $Editor -ArgumentList $taskQuoted -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOut "$taskStage-Console.log") -RedirectStandardError (Join-Path $taskOut "$taskStage-Stderr.log")
    $taskHandle=$taskProcess.Handle
    $taskProcess.WaitForExit()
    if ($taskProcess.ExitCode -ne 0) { throw "Unreal $taskStage exited $($taskProcess.ExitCode)" }
    if (-not (Test-Path -LiteralPath $taskReport)) { throw "Unreal $taskStage success report missing; inspect logs" }
    $taskResult=Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
    if ($taskStage -eq 'Capture' -and $taskResult.views.Count -ne $taskBatch.renders.Count) { throw 'Incomplete Unreal capture set' }
    if ($taskStage -eq 'Import' -and $taskResult.meshes.Count -ne $taskBatch.meshes.Count) { throw 'Incomplete Unreal import set' }
}
if (Test-Path -LiteralPath (Join-Path $taskOut 'ProtectedInputs.json')) {
    $taskProtected=Get-Content -LiteralPath (Join-Path $taskOut 'ProtectedInputs.json') -Raw | ConvertFrom-Json -AsHashtable
    foreach ($taskPath in $taskProtected.Keys) {
        if ((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskProtected[$taskPath]) { throw "Protected input changed: $taskPath" }
    }
}
if ($Mode -in @('All','Capture','Compose')) {
    $taskRenderer=if(Test-Path -LiteralPath (Join-Path $taskOut 'Blender/Report.json')){'Both'}else{'Unreal'}
    & $Python (Join-Path $PSScriptRoot 'Compose-CityMaterialComparison.py') --batch $taskBatchPath --renderer $taskRenderer
    if ($LASTEXITCODE -ne 0) { throw 'Unreal sheet composition failed' }
}
Write-Output "CITYSIM_UNREAL_PREVIEW_SHEET_$($Mode.ToUpper())_PASS"
