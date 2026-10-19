[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][string[]]$ExpectedTests,
    [Parameter(Mandatory)][int]$ProcessExitCode
)
$ErrorActionPreference = 'Stop'
$taskText = Get-Content -LiteralPath $LogPath -Raw
if ($ProcessExitCode -ne 0 -or $taskText -match 'Result=\{Fail\}|Fatal error|Assertion failed') {
    throw "Automation failed (exit $ProcessExitCode); see $LogPath"
}
if (-not $ExpectedTests.Count) { throw 'At least one exact expected test is required.' }
foreach ($taskTest in $ExpectedTests) {
    if ($taskText -notmatch ('Test Completed\. Result=\{Success\}[^\r\n]*Path=\{' + [Regex]::Escape($taskTest) + '\}')) {
        throw "Requested test did not pass (including missing discovery): $taskTest; see $LogPath"
    }
}
Write-Output "Verified $($ExpectedTests.Count) requested automation test results."
