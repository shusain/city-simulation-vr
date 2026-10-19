# Preserve only the generated pre-integration animation attempts. Source FBXs,
# current _D83 assets, canonical content and player files are not moved.
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$taskSource=[IO.Path]::GetFullPath((Join-Path $taskRoot 'CitySimVR/Content/CitySim/Art/Parks/Animations'))
$taskArchive=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Artifacts/D83/Integration/RetiredAnimationBindings'))
if (!$taskSource.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar) -or !$taskArchive.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar)) { throw 'Archive paths escaped workspace' }
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Close owned native processes before archiving failed bindings' }
$taskManifest=Get-Content (Join-Path $taskRoot 'ArtSource/Manifests/city_park_integration.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $taskArchive -Force | Out-Null
foreach ($taskClip in $taskManifest.clips) {
    if ($taskClip.name -notmatch '^[A-Za-z0-9]+$') { throw 'Unexpected clip identifier' }
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskSource "AS_Park_$($taskClip.name).uasset"))
    if (!$taskPath.StartsWith($taskSource+[IO.Path]::DirectorySeparatorChar)) { throw 'Unexpected generated asset path' }
    if (Test-Path -LiteralPath $taskPath) {
        $taskDestination=Join-Path $taskArchive ([IO.Path]::GetFileName($taskPath))
        if (Test-Path -LiteralPath $taskDestination) { throw 'Do not overwrite the retired binding backup' }
        Move-Item -LiteralPath $taskPath -Destination $taskDestination
    }
}
Write-Output 'CITYSIM_D83_FAILED_BINDINGS_ARCHIVED'
