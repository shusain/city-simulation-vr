[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$BuildId,
    [string]$DistributionRoot,
    [string]$WindowsPackage,
    [switch]$IncludeQuest,
    [string]$QuestApk,
    [string]$ReleaseNotesPath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskTemplates = Join-Path $PSScriptRoot 'Distribution/Repository'
. (Join-Path $taskTemplates 'tools/PackageRules.ps1')
if (-not $DistributionRoot) { $DistributionRoot = Join-Path $taskRoot 'TesterDistribution' }
if (-not $WindowsPackage) { $WindowsPackage = Join-Path $taskRoot 'Artifacts/PCVRSandbox/Windows' }
if (-not $QuestApk) { $QuestApk = Join-Path $taskRoot 'Artifacts/QuestSandbox/Android_ASTC/CitySimVR-arm64.apk' }
$taskDestination = [IO.Path]::GetFullPath($DistributionRoot).TrimEnd('\','/')
$taskWindows = (Resolve-Path -LiteralPath $WindowsPackage).Path.TrimEnd('\','/')
if (Test-Path -LiteralPath (Join-Path $taskWindows 'CapacityBuild.json')) { throw 'Experimental capacity builds cannot be exported to testers.' }
if ($taskDestination -eq $taskRoot -or $taskRoot.StartsWith($taskDestination + [IO.Path]::DirectorySeparatorChar) -or $taskDestination.StartsWith($taskWindows + [IO.Path]::DirectorySeparatorChar) -or $taskDestination -eq $taskWindows) { throw 'Distribution must be separate from the source root and staged build.' }
$taskMarker = Join-Path $taskDestination '.citysim-distribution'
if (Test-Path -LiteralPath $taskDestination) {
    if (-not (Test-Path -LiteralPath $taskMarker) -or (Get-Content -LiteralPath $taskMarker -Raw).Trim() -ne 'CitySimVR tester distribution v1') { throw 'Refusing to populate an unrecognized existing directory.' }
} else {
    New-Item -ItemType Directory -Path $taskDestination | Out-Null
}
$taskPackage = Join-Path $taskDestination "packages/$BuildId"
if (Test-Path -LiteralPath $taskPackage) { throw 'Build ID already exists. Choose a new ID; prepared packages are never overwritten.' }
$taskRuntime = @(Get-ChildItem -LiteralPath $taskWindows -Recurse -Force -File)
if (@(Get-ChildItem -LiteralPath $taskWindows -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Staged build contains a link/reparse point.' }
$taskIncluded = @()
$taskExcluded = @()
foreach ($taskFile in $taskRuntime) {
    $taskRelative = $taskFile.FullName.Substring($taskWindows.Length + 1).Replace('\','/')
    if ($taskRelative -eq 'CitySimVR.exe' -or $taskFile.Extension -eq '.pdb' -or $taskRelative -like 'Manifest_*.txt' -or $taskRelative.StartsWith('Engine/Extras/GPUDumpViewer/') -or $taskRelative -eq 'Engine/Extras/Redist/en-us/vc_redist.arm64.exe') {
        $taskExcluded += $taskRelative
        continue
    }
    if (-not (Test-CityRuntimePath $taskRelative)) { throw "Unexpected staged file; export refused: $taskRelative" }
    $taskIncluded += [pscustomobject]@{Source=$taskFile.FullName;Relative=$taskRelative}
}
if (-not $taskIncluded.Count) { throw 'No staged runtime files.' }
if ($IncludeQuest -and -not (Test-Path -LiteralPath $QuestApk -PathType Leaf)) { throw 'Quest APK missing.' }
if ($IncludeQuest) { & (Join-Path $PSScriptRoot 'Test-QuestPackage.ps1') -ApkPath $QuestApk }
if ($ReleaseNotesPath -and -not (Test-Path -LiteralPath $ReleaseNotesPath -PathType Leaf)) { throw 'Release notes missing.' }
# Preserve edited prose; the three managed validation/publication helpers follow source templates.
foreach ($taskFile in Get-ChildItem -LiteralPath $taskTemplates -Recurse -Force -File) {
    $taskTo = Join-Path $taskDestination $taskFile.FullName.Substring($taskTemplates.Length + 1)
    if (-not (Test-Path -LiteralPath $taskTo) -or $taskFile.Name -in @('PackageRules.ps1','Test-Package.ps1','Publish-Prerelease.ps1')) {
        New-Item -ItemType Directory -Path (Split-Path $taskTo -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $taskFile.FullName -Destination $taskTo
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $taskDestination '.git') -PathType Container)) {
    & git init -b main $taskDestination
    if ($LASTEXITCODE -ne 0) { throw 'Independent repository initialization failed.' }
}
New-Item -ItemType Directory -Path $taskPackage -Force | Out-Null
$taskBundleName = 'CitySimVR'
$taskStage = Join-Path $taskDestination "staging/$BuildId/$taskBundleName"
if (Test-Path -LiteralPath $taskStage) { throw 'Staging ID already exists.' }
foreach ($taskFile in $taskIncluded) {
    $taskTo = Join-Path $taskStage "Windows/$($taskFile.Relative)"
    New-Item -ItemType Directory -Path (Split-Path $taskTo -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $taskFile.Source -Destination $taskTo
}
foreach ($taskName in @('Play-Desktop.cmd','Play-Desktop-Windowed.cmd','Play-PCVR.cmd')) {
    # Batch launchers use Windows line endings regardless of source checkout.
    $taskText = (Get-Content -LiteralPath (Join-Path $PSScriptRoot "Distribution/$taskName") -Raw) -replace '\r?\n',"`r`n"
    [IO.File]::WriteAllText((Join-Path $taskStage $taskName), $taskText, [Text.ASCIIEncoding]::new())
}
foreach ($taskName in @('TESTING.md','KNOWN_ISSUES.md')) {
    Copy-Item -LiteralPath (Join-Path $taskDestination $taskName) -Destination (Join-Path $taskStage $taskName)
}
$taskExeHash = (Get-FileHash -LiteralPath (Join-Path $taskStage 'Windows/CitySimVR/Binaries/Win64/CitySimVR.exe')).Hash
$taskCreated = [DateTime]::UtcNow.ToString('o')
@("City Sim VR playtest $BuildId",'Configuration: Development',"Bundled UTC: $taskCreated",'Modes: Windows Desktop / SteamVR PCVR (D3D11)',"Executable SHA256: $taskExeHash",'See TESTING.md and release notes for verified coverage.') | Set-Content -LiteralPath (Join-Path $taskStage 'BUILD.txt') -Encoding UTF8
$taskFiles = @(Get-ChildItem -LiteralPath $taskStage -Recurse -File | Sort-Object FullName | ForEach-Object {
    [pscustomobject]@{path=$_.FullName.Substring($taskStage.Length + 1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
[ordered]@{schema=1;launchProfileVersion=2;buildId=$BuildId;createdUtc=$taskCreated;configuration='Development';executableSha256=$taskExeHash;files=$taskFiles} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskStage 'build-manifest.json') -Encoding UTF8
$taskZip = Join-Path $taskPackage "CitySimVR-$BuildId-Windows.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($taskStage, $taskZip, [IO.Compression.CompressionLevel]::Optimal, $true)
$taskAssets = @([pscustomobject]@{name=(Split-Path $taskZip -Leaf);bytes=(Get-Item $taskZip).Length;sha256=(Get-FileHash $taskZip).Hash})
if ($IncludeQuest) {
    $taskApkName = "CitySimVR-$BuildId-Quest.apk"
    $taskApk = Join-Path $taskPackage $taskApkName
    Copy-Item -LiteralPath $QuestApk -Destination $taskApk
    $taskAssets += [pscustomobject]@{name=$taskApkName;bytes=(Get-Item $taskApk).Length;sha256=(Get-FileHash $taskApk).Hash}
}
[ordered]@{schema=1;buildId=$BuildId;createdUtc=$taskCreated;configuration='Development';engineVersion='5.7.4';assets=$taskAssets} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskPackage 'release-manifest.json') -Encoding UTF8
$taskAssets | ForEach-Object { "$($_.sha256)  $($_.name)" } | Set-Content -LiteralPath (Join-Path $taskPackage 'SHA256SUMS.txt') -Encoding ASCII
New-Item -ItemType Directory -Path (Join-Path $taskDestination 'releases') -Force | Out-Null
$taskNotes = Join-Path $taskDestination "releases/$BuildId.md"
if (Test-Path -LiteralPath $taskNotes) { throw 'Release notes for this ID already exist.' }
if ($ReleaseNotesPath) { Copy-Item -LiteralPath $ReleaseNotesPath -Destination $taskNotes }
else {
    @("# City Sim VR $BuildId",'', 'Early playtest: one Windows ZIP supports Desktop and SteamVR PCVR. Extract fully and use the named launchers. See TESTING.md and KNOWN_ISSUES.md.', '', 'Verification: package inventory, required runtime files and SHA256 checks. Game behavior/device validation is not established by this export; the maintainer should add the exact tested coverage before publishing.', '', 'Attach the Windows ZIP, release-manifest.json and SHA256SUMS.txt. Any Quest APK is an optional separate sideload build; see its validation status before installing.') | Set-Content -LiteralPath $taskNotes -Encoding UTF8
}
# Build-machine provenance stays with private source artifacts, never in the
# distribution Git repository or downloadable package.
$taskPrivate = Join-Path $taskRoot 'Artifacts/Playtests/Provenance'
New-Item -ItemType Directory -Path $taskPrivate -Force | Out-Null
[ordered]@{buildId=$BuildId;windowsStage=$taskWindows;questSource=$(if($IncludeQuest){[IO.Path]::GetFullPath($QuestApk)}else{$null});createdUtc=$taskCreated;excluded=$taskExcluded;executableSha256=$taskExeHash} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskPrivate "$BuildId.json")
& (Join-Path $taskDestination 'tools/Test-Package.ps1') -PackageDirectory $taskPackage
Write-Output "Independent repository: $taskDestination"
Write-Output "Ready release assets: $taskPackage"
Write-Output "Review notes: $taskNotes"
Write-Output 'No remote changes, commit/push/upload or device install performed.'
