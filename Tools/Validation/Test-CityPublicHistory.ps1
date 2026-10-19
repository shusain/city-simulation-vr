[CmdletBinding()]
param(
    [string]$Ref = 'github-main',
    [string]$RepositoryRoot,
    [string]$PushInputFile,
    [string]$RemoteName,
    [string]$RemoteUrl
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$cleanRoot = 'ed1726a4e8d051ac8ebe8944509b0d343bd159d6'
function Read-Git([string[]]$Arguments) {
    $result = @(& git --no-replace-objects -C $RepositoryRoot @Arguments)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot verify public history; push blocked.' }
    return $result
}
if ((Read-Git @('rev-parse', '--is-shallow-repository')) -ne 'false') {
    throw 'Full history is required to verify public ancestry.'
}
$grafts = @(Read-Git @('rev-parse', '--git-path', 'info/grafts'))[0]
if (-not [IO.Path]::IsPathRooted($grafts)) { $grafts = Join-Path $RepositoryRoot $grafts }
if ((Test-Path -LiteralPath $grafts) -and (Get-Item -LiteralPath $grafts).Length) {
    throw 'Legacy grafts prevent reliable ancestry verification.'
}
function Assert-PublicRef([string]$Candidate) {
    $commit = @(Read-Git @('rev-parse', '--verify', "$Candidate`^{commit}"))[0]
    $roots = @(Read-Git @('rev-list', '--max-parents=0', $commit))
    if ($roots.Count -ne 1 -or $roots[0] -ne $cleanRoot) {
        throw 'Unsafe public ancestry: expected only the clean snapshot root. Do not merge private history.'
    }
    Write-Output "CITYSIM_PUBLIC_HISTORY_PASS $commit root=$cleanRoot"
}
if ($PushInputFile) {
    if ($RemoteName -ne 'github' -or $RemoteUrl -notmatch '^(https://github\.com/|git@github\.com:|ssh://git@github\.com/)') {
        throw 'Public worktree pushes require the explicitly configured github remote on github.com.'
    }
    foreach ($line in Get-Content -LiteralPath $PushInputFile) {
        if (-not $line.Trim()) { continue }
        $parts = $line -split '\s+'
        if ($parts.Count -ne 4 -or $parts[2] -notmatch '^refs/heads/' -or $parts[1] -match '^0+$') {
            throw 'Only explicit non-deletion branch pushes are allowed from this worktree.'
        }
        Assert-PublicRef $parts[1]
    }
} else { Assert-PublicRef $Ref }
