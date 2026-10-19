[CmdletBinding()]
param([switch]$Apply)

# Only consume the reviewed, dated manifest. No directory/age wildcards at removal time.
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$taskArtifactRoot = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Artifacts')).Path
$taskManifestPath = Join-Path $taskRoot 'docs/visual-changelog/cleanup-2026-10-11.json'
$taskManifest = Get-Content -LiteralPath $taskManifestPath -Raw | ConvertFrom-Json
if ($taskManifest.task -ne 'D84' -or $taskManifest.status -ne 'planned') {
    throw 'Manifest is not the planned D84 cleanup; completed plans cannot be replayed.'
}
$taskPaths = @()
foreach ($taskEntry in $taskManifest.deletion_candidates) {
    $taskPath = [IO.Path]::GetFullPath((Join-Path $taskRoot $taskEntry.path))
    if (-not $taskPath.StartsWith($taskArtifactRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Target is outside the exact Artifacts directory: $taskPath"
    }
    $taskFile = Get-Item -LiteralPath $taskPath
    if ($taskFile.PSIsContainer) { throw "Manifest target is a directory: $taskPath" }
    $taskAncestor = $taskFile
    while ($null -ne $taskAncestor -and $taskAncestor.FullName -ne $taskRoot) {
        if ($taskAncestor.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Reparse point in target ancestry: $taskPath"
        }
        $taskAncestor = if ($taskAncestor.PSIsContainer) { $taskAncestor.Parent } else { $taskAncestor.Directory }
    }
    if ($taskFile.Length -ne $taskEntry.bytes -or
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskEntry.sha256) {
        throw "Target changed since review: $taskPath"
    }
    $taskPaths += $taskPath
}
Write-Output "Preflight passed: $($taskPaths.Count) exact files, $($taskManifest.total_bytes) bytes."
if (-not $Apply) { return }

$taskJournalPath = Join-Path $taskRoot 'Artifacts/Maintenance-20261011/deletion-journal.jsonl'
$taskRemovedBytes = [long]0
$taskRemovedCount = 0
foreach ($taskPath in $taskPaths) {
    # Recheck immediately before each deletion in case another chat touched a reviewed file.
    $taskEntry = $taskManifest.deletion_candidates[$taskRemovedCount]
    if ((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskEntry.sha256) {
        throw "Target changed during removal: $taskPath. Partial journal: $taskJournalPath"
    }
    Remove-Item -LiteralPath $taskPath
    $taskRemovedBytes += $taskEntry.bytes
    $taskRemovedCount++
    @{ path=$taskEntry.path; bytes=$taskEntry.bytes; removedUtc=[DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json -Compress | Add-Content -LiteralPath $taskJournalPath
}
# Empty directories only; all resolved paths remain below Artifacts. No recursive removal.
$taskParents = $taskPaths | ForEach-Object { Split-Path -Parent $_ } | Sort-Object -Unique
foreach ($taskParent in $taskParents) {
    $taskEmptyPath = $taskParent
    while ($taskEmptyPath.StartsWith($taskArtifactRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
           (Test-Path -LiteralPath $taskEmptyPath) -and
           @(Get-ChildItem -LiteralPath $taskEmptyPath -Force).Count -eq 0) {
        Remove-Item -LiteralPath $taskEmptyPath
        $taskEmptyPath = Split-Path -Parent $taskEmptyPath
    }
}
$taskManifest.status = 'removed-awaiting-verification'
$taskManifest | Add-Member -NotePropertyName removed_files -NotePropertyValue $taskRemovedCount
$taskManifest | Add-Member -NotePropertyName removed_bytes -NotePropertyValue $taskRemovedBytes
$taskManifest | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $taskManifestPath -Encoding utf8
Write-Output "Removed $taskRemovedCount files / $taskRemovedBytes bytes. Protected files still require verification."
