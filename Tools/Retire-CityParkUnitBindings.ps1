param([ValidatePattern('^RetiredUnitBindings[A-Za-z0-9-]*$')][string]$Snapshot='RetiredUnitBindings')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$taskContent=[IO.Path]::GetFullPath((Join-Path $taskRoot 'CitySimVR/Content/CitySim/Art/Parks'))
$taskArchive=[IO.Path]::GetFullPath((Join-Path $taskRoot "Artifacts/D83/Integration/$Snapshot"))
if (!$taskContent.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar) -or !$taskArchive.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar)) { throw 'Archive path escaped workspace' }
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process is active' }
$taskFiles=@(Get-ChildItem -LiteralPath (Join-Path $taskContent 'Meshes') -Filter 'SK_Park_*.uasset')
$taskFiles+=@(Get-ChildItem -LiteralPath (Join-Path $taskContent 'Animations') -Filter 'AS_Park_*_D83.uasset')
foreach ($taskFile in $taskFiles) {
    $taskSource=[IO.Path]::GetFullPath($taskFile.FullName)
    if (!$taskSource.StartsWith($taskContent+[IO.Path]::DirectorySeparatorChar)) { throw 'Unexpected generated binding path' }
    $taskDestination=Join-Path $taskArchive (Join-Path $taskFile.Directory.Name $taskFile.Name)
    if (Test-Path -LiteralPath $taskDestination) { throw 'Do not overwrite the retired unit snapshot' }
    New-Item -ItemType Directory -Path (Split-Path $taskDestination -Parent) -Force | Out-Null
    Move-Item -LiteralPath $taskSource -Destination $taskDestination
}
Write-Output "CITYSIM_D83_UNIT_BINDINGS_ARCHIVED $($taskFiles.Count)"
