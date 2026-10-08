[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PackageRules.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskPackage = (Resolve-Path -LiteralPath $PackageDirectory).Path
$taskManifest = Get-Content -LiteralPath (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json
if ($taskManifest.buildId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$') { throw 'Invalid build ID.' }
$taskAllowedAssets = @("CitySimVR-$($taskManifest.buildId)-Windows.zip", "CitySimVR-$($taskManifest.buildId)-Quest.apk")
$taskAssetNames = @{}
foreach ($taskAsset in $taskManifest.assets) {
    if ($taskAsset.name -notin $taskAllowedAssets -or $taskAssetNames.ContainsKey($taskAsset.name)) { throw 'Unexpected or duplicate release asset.' }
    $taskAssetNames[$taskAsset.name] = $true
    $taskFile = Get-Item -LiteralPath (Join-Path $taskPackage $taskAsset.name)
    if ($taskFile.Length -ge 2GB -or $taskFile.Length -ne $taskAsset.bytes -or (Get-FileHash -LiteralPath $taskFile.FullName).Hash -ne $taskAsset.sha256) { throw "Asset size/hash mismatch: $($taskAsset.name)" }
}
$taskZipName = "CitySimVR-$($taskManifest.buildId)-Windows.zip"
if (-not $taskAssetNames.ContainsKey($taskZipName)) { throw 'Missing Windows ZIP.' }
$taskArchive = [IO.Compression.ZipFile]::OpenRead((Join-Path $taskPackage $taskZipName))
try {
    $taskPrefix = 'CitySimVR/'
    $taskEntryMap = @{}
    foreach ($taskEntry in $taskArchive.Entries) {
        if (-not $taskEntry.FullName.StartsWith($taskPrefix)) { throw 'Unexpected ZIP root.' }
        $taskRelative = $taskEntry.FullName.Substring($taskPrefix.Length)
        if ($taskRelative -match '(^/|:|\\|(^|/)\.\.(/|$))') { throw 'Unsafe ZIP path.' }
        if ($taskEntry.FullName.EndsWith('/')) { continue }
        if (($taskRelative -ne 'build-manifest.json' -and -not (Test-CityBundlePath $taskRelative)) -or $taskEntryMap.ContainsKey($taskRelative)) { throw "Unexpected/duplicate ZIP path: $taskRelative" }
        $taskEntryMap[$taskRelative] = $taskEntry
    }
    if (-not $taskEntryMap.ContainsKey('build-manifest.json')) { throw 'Missing bundle manifest.' }
    $taskReader = [IO.StreamReader]::new($taskEntryMap['build-manifest.json'].Open())
    try { $taskBundle = $taskReader.ReadToEnd() | ConvertFrom-Json } finally { $taskReader.Dispose() }
    if ($taskBundle.buildId -ne $taskManifest.buildId) { throw 'Bundle/release identity mismatch.' }
    $taskExpected = @{}
    foreach ($taskFile in $taskBundle.files) {
        if (-not (Test-CityBundlePath $taskFile.path) -or $taskExpected.ContainsKey($taskFile.path) -or -not $taskEntryMap.ContainsKey($taskFile.path)) { throw "Invalid/missing bundle file: $($taskFile.path)" }
        $taskExpected[$taskFile.path] = $true
        $taskEntry = $taskEntryMap[$taskFile.path]
        $taskStream = $taskEntry.Open()
        try { $taskHash = Get-CityStreamHash $taskStream } finally { $taskStream.Dispose() }
        if ($taskEntry.Length -ne $taskFile.bytes -or $taskHash -ne $taskFile.sha256) { throw "Bundle hash mismatch: $($taskFile.path)" }
    }
    if ($taskExpected.Count + 1 -ne $taskEntryMap.Count) { throw 'ZIP contains unmanifested files.' }
    foreach ($taskRequired in @('Play-Desktop.cmd','Play-PCVR.cmd','BUILD.txt','TESTING.md','KNOWN_ISSUES.md','Windows/CitySimVR/Binaries/Win64/CitySimVR.exe','Windows/CitySimVR/Content/Paks/CitySimVR-Windows.pak','Windows/CitySimVR/Content/Paks/CitySimVR-Windows.utoc','Windows/CitySimVR/Content/Paks/CitySimVR-Windows.ucas','Windows/Engine/Binaries/ThirdParty/OpenXR/win64/openxr_loader.dll','Windows/Engine/Extras/Redist/en-us/vc_redist.x64.exe')) {
        if (-not $taskExpected.ContainsKey($taskRequired)) { throw "Required runtime file missing: $taskRequired" }
    }
} finally { $taskArchive.Dispose() }
$taskSumLines = @(Get-Content -LiteralPath (Join-Path $taskPackage 'SHA256SUMS.txt'))
if ($taskSumLines.Count -ne @($taskManifest.assets).Count) { throw 'Checksum list has unexpected entries.' }
foreach ($taskAsset in $taskManifest.assets) {
    if ("$($taskAsset.sha256)  $($taskAsset.name)" -notin $taskSumLines) { throw 'Checksum list does not match release manifest.' }
}
Write-Output "Package verified: $($taskManifest.buildId), $($taskExpected.Count) bundle files, $(@($taskManifest.assets).Count) release assets; no project source/editable assets/symbols/saves/logs."
