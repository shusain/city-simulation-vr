[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../CI/CityCiSmoke.ps1')
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $taskOutput) { throw 'Use a fresh smoke fixture directory.' }
$taskCi=Join-Path $taskOutput 'Artifacts/CI'
$taskLayout=Get-CityCiSmokeLayout $taskCi
$taskCandidate=Join-Path $taskCi 'Builds/fixture/Candidate'
$taskExecutable=Join-Path $taskCandidate 'CitySimVR/Binaries/Win64/CitySimVR.exe'
$taskHeld=Split-Path $taskLayout.executable -Parent
New-Item -ItemType Directory -Path (Split-Path $taskExecutable -Parent),$taskHeld -Force | Out-Null
[IO.File]::WriteAllText($taskExecutable,'Offline fixture, never executed.')
[IO.File]::WriteAllText((Join-Path $taskCandidate 'payload.bin'),'candidate bytes')
[IO.File]::WriteAllText($taskLayout.marker,'CitySimVR CI scratch fixture')
$taskPreviousWorking=[Environment]::CurrentDirectory
$taskLocked=$false
$taskRuntime=$null
try {
    [Environment]::CurrentDirectory=$taskHeld
    # Reproduce Windows351 with an actual directory handle, no game process.
    if (-not $taskLayout.runtime.StartsWith($taskOutput+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Fixture escaped output.'}
    try { [IO.Directory]::Delete($taskLayout.runtime,$true) }
    catch [IO.IOException] { $taskLocked=$true }
    if (-not $taskLocked) {throw 'Held directory did not reproduce the Windows reset failure.'}
    [IO.File]::WriteAllText((Join-Path $taskHeld 'stale.bin'),'must be removed')
    $taskReceipt=Join-Path $taskCi 'Builds/fixture/SmokeRuntime.json'
    $taskRuntime=Initialize-CityCiSmokeRuntime -CiRoot $taskCi -CandidateDirectory $taskCandidate -ReportPath $taskReceipt
    if ($taskRuntime.fileCount -ne 2 -or (Test-Path -LiteralPath (Join-Path $taskHeld 'stale.bin'))) {throw 'Stale smoke files survived reset.'}
    if ([Environment]::CurrentDirectory -ne $taskHeld -or -not (Test-Path -LiteralPath $taskHeld -PathType Container)) {throw 'Held directory was disturbed.'}
    $taskResult=Get-Content -LiteralPath $taskReceipt -Raw | ConvertFrom-Json
    if ($taskResult.status -ne 'passed' -or $taskResult.files.Count -ne 2) {throw 'Smoke inventory verification failed.'}
    [ordered]@{issue=274;status='passed';heldDirectoryReproduced=$taskLocked;directoryPreserved=$true;staleFilesRemoved=$true;candidateFiles=2;unrealLaunched=$false} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskOutput 'Result.json') -Encoding UTF8
    Write-Output 'CITYSIM_SMOKE_HELD_DIRECTORY_PASS actual Windows directory handle; stale cleanup and candidate hashes; no native launch'
} finally {
    if ($taskRuntime) {$taskRuntime.lock.Dispose()}
    [Environment]::CurrentDirectory=$taskPreviousWorking
}
