[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$BuildId,
    [Parameter(Mandatory)][string]$ReleaseNotesPath,
    [ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository = 'shusain/city-simulation-vr',
    [switch]$Publish,
    [switch]$IncludeQuest,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskDistribution = Join-Path $taskRoot 'TesterDistribution'
$taskPackage = Join-Path $taskDistribution "packages/$BuildId"
$taskNotes = (Resolve-Path -LiteralPath $ReleaseNotesPath).Path
if (-not $Resume) {
    if (Test-Path -LiteralPath $taskPackage) { throw 'Build ID already exists; choose a new ID, or -Resume the unchanged candidate.' }
    & (Join-Path $PSScriptRoot 'Build-PCVRSandbox.ps1')
    if ($IncludeQuest) { & (Join-Path $PSScriptRoot 'Build-QuestSandbox.ps1') }
    & (Join-Path $PSScriptRoot 'Prepare-TesterDistribution.ps1') -BuildId $BuildId -ReleaseNotesPath $taskNotes -IncludeQuest:$IncludeQuest
} else {
    $taskPreparedNotes = Join-Path $taskDistribution "releases/$BuildId.md"
    if (-not (Test-Path -LiteralPath $taskPackage) -or -not (Test-Path -LiteralPath $taskPreparedNotes) -or
        (Get-Content -LiteralPath $taskNotes -Raw) -ne (Get-Content -LiteralPath $taskPreparedNotes -Raw)) { throw 'Resume requires an existing package and identical reviewed notes.' }
}
if ($IncludeQuest) {
    $taskManifest = Get-Content -LiteralPath (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json
    $taskQuest = @($taskManifest.assets | Where-Object name -eq "CitySimVR-$BuildId-Quest.apk")
    if ($taskQuest.Count -ne 1) { throw 'IncludeQuest requires the prepared candidate to contain its APK.' }
    & (Join-Path $PSScriptRoot 'Test-QuestPackage.ps1') -ApkPath (Join-Path $taskPackage $taskQuest[0].name)
}
# These three repository helpers are managed by the source templates. Public
# prose stays editable in the independent repository and is never auto-replaced.
foreach ($taskHelper in @('PackageRules','Test-Package','Publish-Prerelease')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "Distribution/Repository/tools/$taskHelper.ps1") -Destination (Join-Path $taskDistribution "tools/$taskHelper.ps1")
}
& (Join-Path $PSScriptRoot 'Test-TesterDistribution.ps1') -BuildId $BuildId
if ($Publish) {
    # Preserve other README edits while updating the current download link.
    $taskReadme = Join-Path $taskDistribution 'README.md'
    $taskText = Get-Content -LiteralPath $taskReadme -Raw
    $taskLink = "https://github.com/$Repository/releases/tag/$BuildId"
    if ($taskText -match 'https://github\.com/[^/]+/[^/]+/releases/tag/[A-Za-z0-9._-]+') {
        $taskText = $taskText -replace 'https://github\.com/[^/]+/[^/]+/releases/tag/[A-Za-z0-9._-]+',$taskLink
    } else {
        $taskText = $taskText.Replace("this repository's **Releases** page", "[this playtest release]($taskLink)")
    }
    [IO.File]::WriteAllText($taskReadme,$taskText,[Text.UTF8Encoding]::new($false))
    & (Join-Path $taskDistribution 'tools/Publish-Prerelease.ps1') -Repository $Repository -BuildId $BuildId -Publish
} else {
    Write-Output "Local candidate ready: $taskPackage. No commit, push or upload. Add -Resume -Publish to release this unchanged candidate."
}
