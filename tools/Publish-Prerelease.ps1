[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$BuildId,
    [switch]$UploadDraft,
    [switch]$Publish
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot '.git') -PathType Container) -or (Get-Content (Join-Path $taskRoot '.citysim-distribution') -Raw).Trim() -ne 'CitySimVR tester distribution v1') { throw 'Run only from the independent distribution repository.' }
$taskGit = @('-c',"safe.directory=$($taskRoot.Replace('\','/'))",'-C',$taskRoot)
function Invoke-CityGit {
    $taskOutput = & git @taskGit @args
    if ($LASTEXITCODE -ne 0) { throw "Distribution Git failed: $($args -join ' ')" }
    return $taskOutput
}
if ((Invoke-CityGit remote get-url origin) -notin @("https://github.com/$Repository.git","https://github.com/$Repository")) { throw 'Distribution origin does not match the requested GitHub repository.' }
if ((Invoke-CityGit branch --show-current) -ne 'main') { throw 'Publish from the distribution main branch.' }
$taskPackage = Join-Path $taskRoot "packages/$BuildId"
& (Join-Path $PSScriptRoot 'Test-Package.ps1') -PackageDirectory $taskPackage
$taskData = Get-Content (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json
if ($taskData.buildId -ne $BuildId) { throw 'Manifest build ID does not match.' }
$taskTracked = @(Invoke-CityGit ls-files --cached --others --exclude-standard)
if (-not $taskTracked.Count) { throw 'Distribution documents are missing.' }
foreach ($taskPath in $taskTracked) {
    if ($taskPath -notmatch '^(\.gitignore|\.citysim-distribution|README.md|TESTING.md|KNOWN_ISSUES.md|MAINTAINERS.md|\.github/ISSUE_TEMPLATE/[A-Za-z0-9_-]+\.yml|tools/(PackageRules|Test-Package|Publish-Prerelease)\.ps1|releases/[A-Za-z0-9._-]+\.md)$') { throw "Unexpected distribution file: $taskPath" }
}
$taskNotesPath = Join-Path $taskRoot "releases/$BuildId.md"
if (-not (Test-Path -LiteralPath $taskNotesPath -PathType Leaf)) { throw 'Missing reviewed release notes.' }
$taskNotes = Get-Content -LiteralPath $taskNotesPath -Raw
$taskFiles = @($taskData.assets | ForEach-Object { Join-Path $taskPackage $_.name }) + @((Join-Path $taskPackage 'release-manifest.json'),(Join-Path $taskPackage 'SHA256SUMS.txt'))
$taskExpected = @{}
foreach ($taskFile in $taskFiles) {
    $taskExpected[(Split-Path $taskFile -Leaf)] = [pscustomobject]@{path=$taskFile;bytes=(Get-Item -LiteralPath $taskFile).Length;hash=(Get-FileHash -LiteralPath $taskFile).Hash.ToLowerInvariant()}
}
Write-Output "Target: https://github.com/$Repository; build: $BuildId"
Write-Output "Assets: $($taskFiles -join ', ')"
if (-not ($UploadDraft -or $Publish)) {
    Write-Output 'Preview only. -UploadDraft commits/pushes distribution docs/tag and uploads a verified draft; -Publish also publishes and verifies anonymous downloads.'
    return
}
# Credentials stay in memory. No source Git, force-push or asset replacement.
$taskOldPrompt = [Environment]::GetEnvironmentVariable('GIT_TERMINAL_PROMPT','Process')
$taskOldInteractive = [Environment]::GetEnvironmentVariable('GCM_INTERACTIVE','Process')
try {
    $env:GIT_TERMINAL_PROMPT = '0'
    $env:GCM_INTERACTIVE = 'Never'
    $taskCredential = "protocol=https`nhost=github.com`nusername=$($Repository.Split('/')[0])`n`n" | & git @taskGit credential fill 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'GitHub login required: sign in using Git Credential Manager, then retry.' }
    $taskPassword = @($taskCredential | Where-Object { $_.StartsWith('password=') })
    if ($taskPassword.Count -ne 1) { throw 'GitHub credential unavailable.' }
    $taskHeaders = @{Authorization=('Bearer '+$taskPassword[0].Substring(9));Accept='application/vnd.github+json';'X-GitHub-Api-Version'='2022-11-28';'User-Agent'='CitySimVR-Distribution'}
    $taskApi = "https://api.github.com/repos/$Repository"
    $taskRepository = Invoke-RestMethod $taskApi -Headers $taskHeaders
    if ($taskRepository.full_name -ne $Repository -or $taskRepository.private -or -not $taskRepository.permissions.push) { throw 'Expected a public repository with write access.' }
    $taskExistingTag = & git @taskGit rev-parse --verify "refs/tags/$BuildId" 2>$null
    $taskHasTag = $LASTEXITCODE -eq 0
    if ($taskHasTag) {
        & git @taskGit diff HEAD --quiet
        if ($LASTEXITCODE -ne 0 -or @(Invoke-CityGit ls-files --others --exclude-standard).Count -or $taskExistingTag -ne (Invoke-CityGit rev-parse HEAD)) { throw 'Existing build tag requires an unchanged checkout at that tag. Use a new ID for changes.' }
    } else {
        Invoke-CityGit add -- @taskTracked | Out-Null
        Invoke-CityGit diff --cached --check | Out-Null
        & git @taskGit diff --cached --quiet
        if ($LASTEXITCODE -eq 1) { Invoke-CityGit commit -m "Publish $BuildId" | Write-Output }
        elseif ($LASTEXITCODE -ne 0) { throw 'Cannot inspect staged distribution docs.' }
        Invoke-CityGit tag $BuildId | Out-Null
    }
    Invoke-CityGit push origin main "refs/tags/$BuildId" | Write-Output
    $taskCommit = Invoke-CityGit rev-parse "refs/tags/$BuildId"
    $taskRemoteTag = Invoke-CityGit ls-remote --tags origin "refs/tags/$BuildId"
    if (-not $taskRemoteTag -or $taskRemoteTag.Split()[0] -ne $taskCommit) { throw 'Remote tag does not match local tag.' }
    $taskRelease = $null
    $taskPage = 1
    do {
        $taskReleases = Invoke-RestMethod "$taskApi/releases?per_page=100&page=$taskPage" -Headers $taskHeaders
        $taskRelease = $taskReleases | Where-Object tag_name -eq $BuildId | Select-Object -First 1
        $taskPage++
    } while (-not $taskRelease -and @($taskReleases).Count -eq 100)
    if (-not $taskRelease) {
        $taskBody = @{tag_name=$BuildId;name="City Sim VR $BuildId";body=$taskNotes;draft=$true;prerelease=$true;make_latest='false'} | ConvertTo-Json
        $taskRelease = Invoke-RestMethod "$taskApi/releases" -Method Post -Headers $taskHeaders -ContentType 'application/json' -Body $taskBody
    }
    if (($taskRelease.body -replace '\r\n',"`n").Trim() -ne ($taskNotes -replace '\r\n',"`n").Trim() -or -not $taskRelease.prerelease) { throw 'Existing release notes/type differ. Refusing to replace it.' }
    foreach ($taskName in $taskExpected.Keys) {
        $taskSpec = $taskExpected[$taskName]
        $taskExisting = @($taskRelease.assets | Where-Object name -eq $taskName)
        if ($taskExisting.Count) {
            if ($taskExisting.Count -ne 1 -or $taskExisting[0].size -ne $taskSpec.bytes -or $taskExisting[0].digest -ne "sha256:$($taskSpec.hash)" -or $taskExisting[0].state -ne 'uploaded') { throw "Existing asset differs: $taskName. Never overwritten; choose a new ID." }
            continue
        }
        if (-not $taskRelease.draft) { throw 'Published release is missing assets; never modified.' }
        Write-Output "Uploading $taskName ($($taskSpec.bytes) bytes)..."
        $taskUpload = $taskRelease.upload_url.Split('{')[0]+'?name='+[uri]::EscapeDataString($taskName)
        $taskUploaded = Invoke-RestMethod $taskUpload -Method Post -Headers $taskHeaders -ContentType 'application/octet-stream' -InFile $taskSpec.path -TimeoutSec 600
        if ($taskUploaded.state -ne 'uploaded') { throw "Incomplete upload: $taskName" }
    }
    $taskRelease = Invoke-RestMethod "$taskApi/releases/$($taskRelease.id)" -Headers $taskHeaders
    if (@($taskRelease.assets).Count -ne $taskExpected.Count) { throw 'Unexpected release asset inventory.' }
    foreach ($taskAsset in $taskRelease.assets) {
        if (-not $taskExpected.ContainsKey($taskAsset.name)) { throw 'Unexpected release asset.' }
        $taskSpec = $taskExpected[$taskAsset.name]
        if ($taskAsset.state -ne 'uploaded' -or $taskAsset.size -ne $taskSpec.bytes -or $taskAsset.digest -ne "sha256:$($taskSpec.hash)") { throw "Server size/hash mismatch: $($taskAsset.name)" }
    }
    if ($Publish -and $taskRelease.draft) {
        $taskRelease = Invoke-RestMethod "$taskApi/releases/$($taskRelease.id)" -Method Patch -Headers $taskHeaders -ContentType 'application/json' -Body (@{draft=$false;prerelease=$true;make_latest='false'} | ConvertTo-Json)
    }
    if ($Publish) {
        if ($taskRelease.draft) { throw 'Release is still a draft.' }
        $taskPublic = Invoke-RestMethod "$taskApi/releases/tags/$BuildId" -Headers @{'User-Agent'='CitySimVR-Distribution'}
        if ($taskPublic.id -ne $taskRelease.id -or $taskPublic.draft -or @($taskPublic.assets).Count -ne $taskExpected.Count) { throw 'Public release identity/inventory mismatch.' }
        $taskDownload = Join-Path $taskPackage 'verification'
        New-Item -ItemType Directory -Path $taskDownload -Force | Out-Null
        foreach ($taskAsset in $taskPublic.assets) {
            if (-not $taskExpected.ContainsKey($taskAsset.name)) { throw 'Unexpected public asset.' }
            $taskTo = Join-Path $taskDownload $taskAsset.name
            Invoke-WebRequest $taskAsset.browser_download_url -OutFile $taskTo -TimeoutSec 600
            $taskSpec = $taskExpected[$taskAsset.name]
            if ((Get-Item -LiteralPath $taskTo).Length -ne $taskSpec.bytes -or (Get-FileHash -LiteralPath $taskTo).Hash.ToLowerInvariant() -ne $taskSpec.hash) { throw "Anonymous download hash mismatch: $($taskAsset.name)" }
        }
        & (Join-Path $PSScriptRoot 'Test-Package.ps1') -PackageDirectory $taskDownload
    }
    [ordered]@{repository=$Repository;buildId=$BuildId;commit=$taskCommit;releaseId=$taskRelease.id;url=$taskRelease.html_url;draft=$taskRelease.draft;anonymousDownloadsVerified=[bool]$Publish;assets=@($taskRelease.assets | Select-Object name,size,digest,browser_download_url)} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $taskPackage 'release-result.json')
    Write-Output "Verified release: $($taskRelease.html_url) (draft=$($taskRelease.draft))"
} finally {
    [Environment]::SetEnvironmentVariable('GIT_TERMINAL_PROMPT',$taskOldPrompt,'Process')
    [Environment]::SetEnvironmentVariable('GCM_INTERACTIVE',$taskOldInteractive,'Process')
    $taskCredential = $null; $taskPassword = $null; $taskHeaders = $null
}
