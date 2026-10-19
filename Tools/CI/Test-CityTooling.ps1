[CmdletBinding()]
param([string]$Python = $env:CITYSIM_TOOL_PYTHON)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../Common/CityTools.psm1') -Force
$taskRoot=Get-CityRepositoryRoot -StartPath $PSCommandPath
if (-not $env:CITYSIM_CI_ROOT) { throw 'CITYSIM_CI_ROOT must name the persistent worker output directory' }
$taskFallback = $null
if (-not $Python) {
    # Like Quest signing, use the Windows account profile: Drone replaces
    # USERPROFILE/APPDATA and hides the interactive Python user-site packages.
    $taskSid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $taskProfile=[Environment]::ExpandEnvironmentVariables((Get-ItemProperty -LiteralPath "HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion/ProfileList/$taskSid" -Name ProfileImagePath).ProfileImagePath)
    # Compatibility with the existing worker only; ordinary checkouts prefer
    # .venv or PATH Python and never require a Codex installation.
    $taskFallback=Join-Path $taskProfile '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
}
$Python = Resolve-CityToolPython -RepositoryRoot $taskRoot -Python $Python -Fallback $taskFallback
$taskCommit=(& git -C $taskRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $taskCommit -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify tooling revision' }
$taskBuild=$env:DRONE_BUILD_NUMBER
if (-not $taskBuild) { $taskBuild='local' }
if ($taskBuild -notmatch '^(local|[0-9]+)$') { throw 'Invalid build number' }
$taskTag=$taskBuild+'-'+$taskCommit.Substring(0,12)+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $env:CITYSIM_CI_ROOT ('Tooling/'+$taskTag)
$taskLogs=$taskOutput+'-Driver'
Write-Output "CITYSIM_TOOLING_RUNTIME $Python"
Write-Output "CITYSIM_TOOLING_OUTPUT $taskOutput"
try {
    $taskRun=Invoke-CityToolProcess -FilePath (Get-Command powershell.exe).Source -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $taskRoot 'Tools/Validation/Test-CityTooling.ps1'),'-Python',$Python,'-OutputDirectory',$taskOutput) -LogPrefix $taskLogs
    $taskResult=Read-CityToolReport -Path (Join-Path $taskOutput 'Result.json') -StartedUtc $taskRun.StartedUtc
    if ($taskResult.status -ne 'Pass' -or $taskResult.unrealLaunched) { throw 'Offline tooling result failed' }
    Get-Content -LiteralPath $taskRun.ConsoleLog
    @{build=$taskBuild;commit=$taskCommit;submittedCommit=$env:DRONE_COMMIT_SHA;status='passed';python=$Python;checks=@($taskResult.checks).Count;unrealLaunched=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskOutput 'CI-Result.json')
} catch {
    $taskFailure=$_.Exception.Message
    [void][IO.Directory]::CreateDirectory($taskOutput)
    @{build=$taskBuild;commit=$taskCommit;submittedCommit=$env:DRONE_COMMIT_SHA;status='failed';python=$Python;error=$taskFailure;unrealLaunched=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskOutput 'CI-Result.json')
    foreach ($taskLog in @(($taskLogs+'-Console.log'),($taskLogs+'-Stderr.log'))) {
        if (Test-Path -LiteralPath $taskLog) { Get-Content -LiteralPath $taskLog -Tail 80 }
    }
    throw
}
