param([switch]$Statics)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D83/Integration'
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process is active' }
$taskName=if ($Statics) {'StaticPreview'} else {'Preview'}
$taskScript=if ($Statics) {'Capture-CityParkStaticIntegration.py'} else {'Capture-CityParkIntegration.py'}
$taskCount=if ($Statics) {36} else {13}
# Retain both shared index generations before full Editor initialization as well
# as the isolated UserDir. Never restore discovery metadata over changed cities.
$taskLibrary=Join-Path $taskRoot 'Artifacts/DesktopUser/Saved/SaveGames/Cities'
$taskIndices=@('LibraryIndex.json','LibraryIndex.json.bak')
$taskCopies=Join-Path $taskOut "$taskName-IndexBefore"
New-Item -ItemType Directory -Path $taskCopies -Force | Out-Null
$taskCities=@{}
Get-ChildItem -LiteralPath $taskLibrary -File | Where-Object { $_.Name -notlike 'LibraryIndex*' } | ForEach-Object {
    $taskCities[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash
}
foreach ($taskIndex in $taskIndices) { Copy-Item -LiteralPath (Join-Path $taskLibrary $taskIndex) -Destination (Join-Path $taskCopies $taskIndex) }
try {
    & 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$taskRoot/CitySimVR/CitySimVR.uproject" "-ExecCmds=py $PSScriptRoot/$taskScript" -CityParkAnimationPreview -RenderOffscreen "-UserDir=$taskOut/$taskName-User" "-abslog=$taskOut/$taskName.log" -unattended -nop4 -nosplash -stdout -nocef -nohmd -nosound -d3d11 *> "$taskOut/$taskName-Console.log"
    $taskExit=$LASTEXITCODE
} finally {
    foreach ($taskCity in $taskCities.Keys) {
        if ((Get-FileHash -LiteralPath $taskCity).Hash -ne $taskCities[$taskCity]) { throw 'Shared city changed during gallery; retained metadata copies; refusing automatic index restore' }
    }
    foreach ($taskIndex in $taskIndices) {
        $taskBefore=Join-Path $taskCopies $taskIndex
        $taskCurrent=Join-Path $taskLibrary $taskIndex
        if ((Get-FileHash -LiteralPath $taskBefore).Hash -ne (Get-FileHash -LiteralPath $taskCurrent).Hash) {
            Copy-Item -LiteralPath $taskCurrent -Destination (Join-Path $taskCopies "$taskIndex.transient")
            Copy-Item -LiteralPath $taskBefore -Destination $taskCurrent
        }
    }
}
if ($taskExit -ne 0) { throw "Native park capture exited $taskExit" }
$taskReport=Get-Content "$taskOut/$taskName/Report.json" -Raw | ConvertFrom-Json
if ($taskReport.error -or $taskReport.views.Count -ne $taskCount) { throw 'Native park capture did not complete' }
Write-Output "CITYSIM_D83_NATIVE_PREVIEW_PASS $taskCount views"
