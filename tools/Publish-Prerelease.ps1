[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$BuildId,
    [switch]$UploadDraft
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot '.git') -PathType Container) -or (Get-Content (Join-Path $taskRoot '.citysim-distribution') -Raw).Trim() -ne 'CitySimVR tester distribution v1') { throw 'Run only from the independent distribution repository.' }
$taskPackage = Join-Path $taskRoot "packages/$BuildId"
& (Join-Path $PSScriptRoot 'Test-Package.ps1') -PackageDirectory $taskPackage
$taskTracked = @(git -C $taskRoot ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0 -or $taskTracked.Count -eq 0) { throw 'Distribution documents are missing.' }
foreach ($taskPath in $taskTracked) {
    if ($taskPath -notmatch '^(\.gitignore|\.citysim-distribution|README.md|TESTING.md|KNOWN_ISSUES.md|MAINTAINERS.md|\.github/ISSUE_TEMPLATE/[A-Za-z0-9_-]+\.yml|tools/(PackageRules|Test-Package|Publish-Prerelease)\.ps1|releases/[A-Za-z0-9._-]+\.md)$') { throw "Unexpected tracked file; upload refused: $taskPath" }
}
$taskNotes = Join-Path $taskRoot "releases/$BuildId.md"
if (-not (Test-Path -LiteralPath $taskNotes)) { throw 'Missing reviewed release notes.' }
$taskData = Get-Content (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json
$taskAssets = @($taskData.assets | ForEach-Object { Join-Path $taskPackage $_.name }) + @((Join-Path $taskPackage 'release-manifest.json'),(Join-Path $taskPackage 'SHA256SUMS.txt'))
$taskArgs = @('release','create',$BuildId) + $taskAssets + @('--repo',$Repository,'--verify-tag','--draft','--prerelease','--title',"City Sim VR $BuildId",'--notes-file',$taskNotes)
Write-Output "Target: https://github.com/$Repository (draft prerelease only)"
Write-Output "Assets: $($taskAssets -join ', ')"
if (-not $UploadDraft) { Write-Output 'Preview only. Add -UploadDraft after pushing this distribution repository and its build tag.'; return }
& git -C $taskRoot rev-parse --verify HEAD *> $null
if ($LASTEXITCODE -ne 0) { throw 'Commit the distribution documents before uploading.' }
& git -C $taskRoot diff HEAD --quiet
if ($LASTEXITCODE -ne 0 -or @(git -C $taskRoot ls-files --others --exclude-standard).Count) { throw 'Commit the reviewed distribution documents and release notes before uploading.' }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'GitHub CLI is not installed. Use the GitHub release page or install/authenticate gh before uploading.' }
& gh @taskArgs
if ($LASTEXITCODE -ne 0) { throw 'Draft creation/upload failed. Inspect the repository for a partial draft before retrying; existing releases are never overwritten.' }
Write-Output 'Draft uploaded. Review and publish it on GitHub when ready.'
