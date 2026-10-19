param([switch]$ScaleReview)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D88/NativePreview'
if ($ScaleReview) {
    $taskOut=Join-Path $taskRoot 'Artifacts/D88/Scale/NativePreview'
    $taskOwners=[regex]::Matches((Get-Content "$taskRoot/docs/DEVELOPMENT_TASKS.md" -Raw),'\*\*Native window OWNED BY ([A-Z]+\d+)\b')
    if ($taskOwners.Count -ne 1 -or $taskOwners[0].Groups[1].Value -ne 'D88') { throw 'D88 exclusive native ownership required' }
}
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process is active' }
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$taskLibrary=Join-Path $taskRoot 'Artifacts/DesktopUser/Saved/SaveGames/Cities'
$taskCopies=Join-Path $taskOut 'IndexBefore';New-Item -ItemType Directory -Path $taskCopies -Force | Out-Null
$taskCities=@{}
Get-ChildItem -LiteralPath $taskLibrary -File | Where-Object { $_.Name -notlike 'LibraryIndex*' } | ForEach-Object { $taskCities[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash }
foreach ($taskIndex in @('LibraryIndex.json','LibraryIndex.json.bak')) { Copy-Item -LiteralPath (Join-Path $taskLibrary $taskIndex) -Destination (Join-Path $taskCopies $taskIndex) }
try {
    [string[]]$taskExtra=if ($ScaleReview) { @('-CityD88ScalePreview') } else { @() }
    & 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$taskRoot/CitySimVR/CitySimVR.uproject" "-ExecCmds=py $PSScriptRoot/Capture-CityCitizenSportsRefinement.py" -CityParkAnimationPreview -CityDesktop -RenderOffscreen "-UserDir=$taskOut/User" "-abslog=$taskOut/Engine.log" -unattended -nop4 -nosplash -stdout -nocef -nohmd -nosound -d3d11 @taskExtra *> "$taskOut/Console.log"
    $taskExit=$LASTEXITCODE
} finally {
    foreach ($taskCity in $taskCities.Keys) { if ((Get-FileHash -LiteralPath $taskCity).Hash -ne $taskCities[$taskCity]) { throw 'Shared city changed; preserve metadata backups and refuse automatic restore' } }
    foreach ($taskIndex in @('LibraryIndex.json','LibraryIndex.json.bak')) {
        $taskBefore=Join-Path $taskCopies $taskIndex;$taskCurrent=Join-Path $taskLibrary $taskIndex
        if ((Get-FileHash -LiteralPath $taskBefore).Hash -ne (Get-FileHash -LiteralPath $taskCurrent).Hash) {
            Copy-Item -LiteralPath $taskCurrent -Destination (Join-Path $taskCopies "$taskIndex.transient")
            Copy-Item -LiteralPath $taskBefore -Destination $taskCurrent
        }
    }
}
if ($taskExit -ne 0) { throw 'Native capture process failed' }
$taskReport=Get-Content "$taskOut/Report.json" -Raw | ConvertFrom-Json
$taskCount=if ($ScaleReview) { 18 } else { 16 }
if ($taskReport.error -or $taskReport.views.Count -ne $taskCount) { throw 'Native sports capture incomplete' }
Write-Output "CITYSIM_D88_NATIVE_PREVIEW_PASS $taskCount actual pooled views"
