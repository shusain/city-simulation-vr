[CmdletBinding()]
param([string]$Python = $env:CITYSIM_TOOL_PYTHON, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot -StartPath $PSCommandPath
$Python = Resolve-CityToolPython -RepositoryRoot $taskRoot -Python $Python
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $taskRoot ('Artifacts/ToolingValidation/' + [guid]::NewGuid().ToString('N'))
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh output directory.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$taskOutput = (Get-Item -LiteralPath $OutputDirectory).FullName
trap {
    $taskFailure = $_
    # Keep the actual failing child diagnostics visible in CI, in addition to
    # the persistent files. Fixture logs contain no credentials.
    Get-ChildItem -LiteralPath $taskOutput -Filter '*-Stderr.log' -File | Where-Object { $_.Length } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 2 | ForEach-Object {
        Write-Output "TOOLING_FAILURE_LOG $($_.FullName)"
        Get-Content -LiteralPath $_.FullName -Tail 40
    }
    throw $taskFailure
}
$taskChecks = [Collections.Generic.List[string]]::new()
function Assert-Check([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "Check failed: $Name" }
    $taskChecks.Add($Name)
}
function Assert-Rejected([scriptblock]$Action, [string]$Name, [string]$MessagePattern) {
    $taskRejected = $false
    try { & $Action | Out-Null } catch {
        if ($_.Exception.Message -notmatch $MessagePattern) { throw }
        $taskRejected = $true
    }
    Assert-Check $taskRejected $Name
}

# Use real child processes, never Unreal. Spaces, quotes, empty arguments and
# terminal backslashes exercise Windows argv rather than an implementation mock.
$taskChild = Join-Path $taskOutput 'argument child.py'
@'
import json, os, pathlib, sys, time
mode, output, *values = sys.argv[1:]
print(os.getpid(), flush=True)
if mode == 'sleep':
    time.sleep(30)
if mode == 'fail':
    print('intentional child failure', file=sys.stderr)
    sys.exit(7)
pathlib.Path(output).write_text(json.dumps(values))
'@ | Set-Content -LiteralPath $taskChild -Encoding UTF8
$taskArguments = @('plain', 'with spaces', '', 'quote"inside', 'C:\path with spaces\', 'two\\"quotes', 'line1' + "`n" + 'line2')
$taskReport = Join-Path $taskOutput 'argument report.json'
$taskRun = Invoke-CityToolProcess -FilePath $Python -ArgumentList (@($taskChild, 'pass', $taskReport) + $taskArguments) -LogPrefix "$taskOutput/arguments"
$taskActual = @(Read-CityToolReport -Path $taskReport -StartedUtc $taskRun.StartedUtc)
Assert-Check ($taskActual.Count -eq $taskArguments.Count) 'Windows argument count preserved'
for ($taskIndex = 0; $taskIndex -lt $taskArguments.Count; $taskIndex++) {
    Assert-Check ($taskActual[$taskIndex] -ceq $taskArguments[$taskIndex]) "Windows argument $taskIndex preserved"
}
Assert-Rejected { Invoke-CityToolProcess -FilePath $Python -ArgumentList @($taskChild, 'pass', $taskReport) -LogPrefix "$taskOutput/arguments" } 'Existing logs preserved' 'Preserve existing evidence'
Assert-Rejected { Invoke-CityToolProcess -FilePath $Python -ArgumentList @($taskChild, 'fail', $taskReport) -LogPrefix "$taskOutput/failure" } 'Nonzero exit rejected' 'Tool exited 7'
Assert-Check ((Get-Content "$taskOutput/failure-Stderr.log" -Raw) -match 'intentional child failure') 'Failure stderr retained'
Assert-Rejected { Invoke-CityToolProcess -FilePath $Python -ArgumentList @($taskChild, 'sleep', $taskReport) -LogPrefix "$taskOutput/timeout" -TimeoutSeconds 1 } 'Timeout rejected' 'timed out'
$taskChildId = [int](Get-Content "$taskOutput/timeout-Console.log" -First 1)
Assert-Check ($null -eq (Get-Process -Id $taskChildId -ErrorAction SilentlyContinue)) 'Timed out owned child stopped'

$taskStreamDirectory = Join-Path $taskOutput 'literal [streams]'
[void][IO.Directory]::CreateDirectory($taskStreamDirectory)
$taskStreamChild = Join-Path $taskStreamDirectory 'both.py'
@'
import sys
for _ in range(20000):
    sys.stdout.buffer.write('out \u00e9\n'.encode('utf-8'))
    sys.stderr.buffer.write('err \u00e9\n'.encode('utf-8'))
'@ | Set-Content -LiteralPath $taskStreamChild -Encoding UTF8
$taskStreams = Invoke-CityToolProcess -FilePath $Python -ArgumentList @($taskStreamChild) -LogPrefix (Join-Path $taskStreamDirectory 'large') -TimeoutSeconds 30
$taskUtf8 = [Text.UTF8Encoding]::new($false)
$taskExpectedOut = (('out ' + [char]0xe9 + "`n") * 20000) -join ''
$taskExpectedErr = (('err ' + [char]0xe9 + "`n") * 20000) -join ''
Assert-Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($taskStreams.ConsoleLog)) -eq [Convert]::ToBase64String($taskUtf8.GetBytes($taskExpectedOut)) -and
              [Convert]::ToBase64String([IO.File]::ReadAllBytes($taskStreams.StderrLog)) -eq [Convert]::ToBase64String($taskUtf8.GetBytes($taskExpectedErr))) 'Literal bracket log paths retain both large UTF-8 streams byte-for-byte without pipe deadlock'

[IO.File]::SetLastWriteTimeUtc($taskReport, [datetime]::UtcNow.AddDays(-1))
Assert-Rejected { Read-CityToolReport -Path $taskReport -StartedUtc $taskRun.StartedUtc } 'Stale report rejected' 'Fresh tool report required'
Assert-Rejected { Read-CityToolReport -Path "$taskOutput/missing.json" -StartedUtc $taskRun.StartedUtc } 'Missing report rejected' 'does not exist|Cannot find path'
'{broken' | Set-Content "$taskOutput/broken.json"
Assert-Rejected { Read-CityToolReport -Path "$taskOutput/broken.json" -StartedUtc $taskRun.StartedUtc } 'Malformed JSON rejected' 'JSON|primitive|Invalid|Unexpected'
'null' | Set-Content "$taskOutput/null.json"
Assert-Rejected { Read-CityToolReport -Path "$taskOutput/null.json" -StartedUtc $taskRun.StartedUtc } 'Null JSON rejected' 'Empty tool report'

$taskFixture = Join-Path $taskOutput 'nested checkout'
New-Item -ItemType Directory -Path "$taskFixture/CitySimVR", "$taskFixture/Tools/Assets/Unreal" -Force | Out-Null
'{}' | Set-Content "$taskFixture/CitySimVR/CitySimVR.uproject"
'# fixture' | Set-Content "$taskFixture/Tools/Assets/Unreal/nested.ps1"
Assert-Check ((Get-CityRepositoryRoot "$taskFixture/Tools/Assets/Unreal/nested.ps1") -eq $taskFixture) 'Nested script repository discovery'
Assert-Rejected { Get-CityRepositoryRoot $env:SystemRoot } 'Outside checkout rejected' 'No CitySim checkout'

$taskAutomation = "$taskOutput/automation.log"
$taskExpected = @('CitySim.Art.Icons', 'CitySim.Art.Rooms')
$taskSuccess = "Test Completed. Result={Success} Name={Icons} Path={CitySim.Art.Icons}`nTest Completed. Result={Success} Name={Rooms} Path={CitySim.Art.Rooms}"
$taskValidator = Join-Path $taskRoot 'Tools/Validation/Test-CityAutomationLog.ps1'
$taskLegacy = Join-Path $taskRoot 'Tools/CI/Test-CityAutomationLog.ps1'
$taskSuccess | Set-Content $taskAutomation
& $taskValidator -LogPath $taskAutomation -ExpectedTests $taskExpected -ProcessExitCode 0 | Out-Null
& $taskLegacy -LogPath $taskAutomation -ExpectedTests $taskExpected -ProcessExitCode 0 | Out-Null
Assert-Check $true 'Canonical and existing CI validator entry points pass exact tests'
foreach ($taskCase in @(
    @{ Name='Zero discovered tests'; Text='No tests found'; Code=0 },
    @{ Name='Wrong test identity'; Text=$taskSuccess.Replace('CitySim.Art.Rooms', 'CitySim.Art.Other'); Code=0 },
    @{ Name='Prefix-only identity'; Text=$taskSuccess.Replace('CitySim.Art.Rooms}', 'CitySim.Art.Rooms.Extra}'); Code=0 },
    @{ Name='Failed test'; Text=$taskSuccess.Replace('Success', 'Fail'); Code=0 },
    @{ Name='Fatal after successes'; Text=($taskSuccess + "`nFatal error: fixture"); Code=0 },
    @{ Name='Nonzero automation exit'; Text=$taskSuccess; Code=3 }
)) {
    $taskCase.Text | Set-Content $taskAutomation
    Assert-Rejected { & $taskValidator -LogPath $taskAutomation -ExpectedTests $taskExpected -ProcessExitCode $taskCase.Code } $taskCase.Name 'did not pass|Automation failed'
}
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityTooling.py')) -LogPrefix "$taskOutput/python-contracts"
Assert-Check $true 'Offline Python path and icon contracts'
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityManifestContracts.py')) -LogPrefix "$taskOutput/manifest-contracts"
Assert-Check $true 'Read-only manifest APIs and importer dry-run contracts'
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityCaptureTools.py')) -LogPrefix "$taskOutput/capture-contracts"
Assert-Check $true 'Temporary PIE capture ownership, cleanup and seven driver/forwarding contracts'
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityPreviewTools.py')) -LogPrefix "$taskOutput/preview-contracts"
Assert-Check $true 'Preview sheet API/CLI pagination, references and image validation'
$taskPowerShell = (Get-Command powershell.exe -ErrorAction Stop).Source
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityAssetOrganization.py')) -LogPrefix "$taskOutput/organization-contracts"
Assert-Check $true 'Semantic asset mapping, relocated approved payloads, import-safe producers and pinned approval API'
$null = Invoke-CityToolProcess -FilePath $taskPowerShell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Test-CityPreviewWrapper.ps1'),'-Python',$Python,'-OutputDirectory',"$taskOutput/preview-wrapper") -LogPrefix "$taskOutput/preview-wrapper-contracts"
Assert-Check $true 'Canonical/legacy preview wrapper process, source/report and evidence contracts'
$taskReceipt = [ordered]@{ status='Pass'; checks=@($taskChecks); unrealLaunched=$false; output=$taskOutput }
$null = Invoke-CityToolProcess -FilePath $taskPowerShell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Test-CityEntryPoints.ps1'),'-OutputDirectory',"$taskOutput/entry-points") -LogPrefix "$taskOutput/entry-point-contracts"
Assert-Check $true 'Build/launch/distribution canonical parameter contracts and exact forwarding without native operations'
$taskReceipt.checks = @($taskChecks)
& (Join-Path $PSScriptRoot 'Test-CityCiSmokeRuntime.ps1') -OutputDirectory "$taskOutput/smoke-held-directory"
Assert-Check $true 'CI scratch reset preserves held directory nodes and verifies refreshed candidate bytes'
$taskReceipt.checks = @($taskChecks)
$null = Invoke-CityToolProcess -FilePath $Python -ArgumentList @((Join-Path $PSScriptRoot 'Test-CityPublicSource.py')) -LogPrefix "$taskOutput/public-source-contracts"
Assert-Check $true 'Public source defaults, semantic profiles and paired fixture interfaces'
Assert-Rejected { Resolve-CityToolPython -RepositoryRoot $taskRoot -Python "$taskOutput/missing-python.exe" -Fallback $Python } 'Explicit invalid interpreter never falls back' 'Python 3.11'
$taskOldPath = $env:PATH
try {
    $taskProbeDirectory = Join-Path $taskOutput 'python-probe'
    [void][IO.Directory]::CreateDirectory($taskProbeDirectory)
    "@echo off`necho Missing dependency fixture 1>&2`nexit /b 7" | Set-Content (Join-Path $taskProbeDirectory 'python.cmd') -Encoding ASCII
    $env:PATH = $taskProbeDirectory + ';' + $taskOldPath
    Assert-Check ((Resolve-CityToolPython -RepositoryRoot $taskProbeDirectory -Python '' -Fallback $Python) -eq $Python) 'Invalid PATH Python stderr permits the explicitly provided CI fallback'
    Assert-Rejected { Resolve-CityToolPython -RepositoryRoot $taskProbeDirectory -Python (Join-Path $taskProbeDirectory 'python.cmd') -Fallback $Python } 'Explicit broken Python never falls back after stderr' 'Python 3.11'
} finally { $env:PATH = $taskOldPath }
$taskReceipt.checks = @($taskChecks)
$taskReceipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$taskOutput/Result.json"
Write-Output "CITYSIM_TOOLING_CHECKS_PASS $($taskChecks.Count) checks; $taskOutput/Result.json"
