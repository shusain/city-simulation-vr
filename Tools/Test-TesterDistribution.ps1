[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$BuildId,
    [string]$DistributionRoot
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not $DistributionRoot) { $DistributionRoot = Join-Path $taskRoot 'TesterDistribution' }
$taskDistribution = (Resolve-Path -LiteralPath $DistributionRoot).Path
$taskPackage = Join-Path $taskDistribution "packages/$BuildId"
$taskCheck = Join-Path $taskDistribution 'tools/Test-Package.ps1'
& $taskCheck -PackageDirectory $taskPackage
$taskQuestIncluded = @((Get-Content -LiteralPath (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json).assets | Where-Object name -eq "CitySimVR-$BuildId-Quest.apk").Count -eq 1
. (Join-Path $taskDistribution 'tools/PackageRules.ps1')
foreach ($taskForbidden in @('Windows/CitySimVR/Source/Game.cpp','Windows/CitySimVR/Content/Loose.uasset','Windows/CitySimVR/Binaries/Win64/CitySimVR.pdb','Windows/CitySimVR/Saved/SaveGames/City.json','Windows/Engine/Binaries/../private.key','Windows/Engine/Binaries/private.pem')) {
    if (Test-CityBundlePath $taskForbidden) { throw "Payload policy admitted $taskForbidden" }
}
if (-not (Test-CityBundlePath 'Windows/CitySimVR/Plugins/MetaXR/Source/ThirdParty/OVRPlugin/OVRPlugin/Lib/Win64/OpenXR/OVRPlugin.dll')) { throw 'Required staged Meta runtime DLL was rejected.' }
$taskQa = Join-Path $taskRoot ('Artifacts/QA-D73/' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $taskQa -Force | Out-Null
# Prove the release validator rejects a tampered hash before upload.
$taskBadPackage = Join-Path $taskQa 'BadHash'
New-Item -ItemType Directory -Path $taskBadPackage | Out-Null
$taskData = Get-Content (Join-Path $taskPackage 'release-manifest.json') -Raw | ConvertFrom-Json
$taskAsset = $taskData.assets[0]
New-Item -ItemType HardLink -Path (Join-Path $taskBadPackage $taskAsset.name) -Target (Join-Path $taskPackage $taskAsset.name) | Out-Null
$taskAsset.sha256 = '0' * 64
$taskData | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $taskBadPackage 'release-manifest.json')
$taskRejected = $false
try { & $taskCheck -PackageDirectory $taskBadPackage } catch { if ($_ -notmatch 'size/hash mismatch') { throw }; $taskRejected = $true }
if (-not $taskRejected) { throw 'Tampered package was accepted.' }
# Prove export rejects a source file even when someone points it at a bad stage.
$taskBadStage = Join-Path $taskQa 'BadStage/CitySimVR/Source'
New-Item -ItemType Directory -Path $taskBadStage -Force | Out-Null
'canary, not game source' | Set-Content (Join-Path $taskBadStage 'Canary.cpp')
$taskRejected = $false
try { & (Join-Path $PSScriptRoot 'Prepare-TesterDistribution.ps1') -BuildId bad-stage -WindowsPackage (Join-Path $taskQa 'BadStage') -DistributionRoot (Join-Path $taskQa 'RejectedDistribution') }
catch { if ($_ -notmatch 'Unexpected staged file') { throw }; $taskRejected = $true }
if (-not $taskRejected) { throw 'Source-containing stage was accepted.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskExtract = Join-Path $taskQa 'Extracted Game'
[IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $taskPackage "CitySimVR-$BuildId-Windows.zip"), $taskExtract)
$taskGame = Join-Path $taskExtract 'CitySimVR'
$taskProfile = Join-Path $taskQa 'DesktopSmoke-Automation'
$taskLog = Join-Path $taskProfile 'Desktop/Logs/Latest.log'
$taskPreviousProfile = [Environment]::GetEnvironmentVariable('CITYSIM_PLAYTEST_PROFILE_ROOT','Process')
$taskPreviousRuntime = [Environment]::GetEnvironmentVariable('CITYSIM_TEST_RUNTIME_DIR','Process')
$taskGameProcess = $null
try {
    [Environment]::SetEnvironmentVariable('CITYSIM_PLAYTEST_PROFILE_ROOT',$taskProfile,'Process')
    $taskLauncher = Join-Path $taskGame 'Play-Desktop.cmd'
    # Missing prerequisites must stop before starting a game or an installer.
    $taskMissingRuntime = Join-Path $taskQa 'MissingRuntime'
    New-Item -ItemType Directory -Path $taskMissingRuntime | Out-Null
    [Environment]::SetEnvironmentVariable('CITYSIM_TEST_RUNTIME_DIR',$taskMissingRuntime,'Process')
    $taskMissing = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',('""'+$taskLauncher+'""')) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $taskQa 'MissingRuntime.txt')
    if ($taskMissing.ExitCode -ne 2) { throw 'Missing runtime did not stop with setup instructions.' }
    [Environment]::SetEnvironmentVariable('CITYSIM_TEST_RUNTIME_DIR',$taskPreviousRuntime,'Process')
    foreach ($taskRun in @('ColdStart','Relaunch')) {
    $taskGameProcess = $null
    # The previous successful run's exit marker must not be read as startup failure.
    if (Test-Path -LiteralPath $taskLog) {
        Move-Item -LiteralPath $taskLog -Destination (Join-Path $taskQa "$taskRun-BeforeLaunch.log")
    }
    $taskCommand = '""' + $taskLauncher + '" -windowed -ResX=1280 -ResY=800 -RenderOffscreen -ForceRes -unattended -stdout "-ExecCmds=Automation RunTests CitySim.Desktop.PackagedSmoke" "-TestExit=Automation Test Queue Empty""'
    if ($taskRun -eq 'ColdStart') { $taskCommand = $taskCommand.Replace(' -windowed',' -CityDesktopStartupOnly -windowed') }
    $taskShell = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',$taskCommand) -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru
    $taskDeadline = (Get-Date).AddSeconds(180)
    while (-not $taskGameProcess -and (Get-Date) -lt $taskDeadline) {
        $taskOwned = Get-CimInstance Win32_Process -Filter "Name='CitySimVR.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($taskProfile) } | Select-Object -First 1
        if ($taskOwned) {
            $taskGameProcess = Get-Process -Id $taskOwned.ProcessId
            # Retain the process handle while alive so ExitCode remains available
            # after the process exits (the launcher starts it asynchronously).
            $null = $taskGameProcess.Handle
            break
        }
        if ((Test-Path -LiteralPath $taskLog) -and (Get-Content -LiteralPath $taskLog -Raw) -match 'Fatal error|Assertion failed|RequestExitWithStatus\(1,\s*[1-9]') { throw "Extracted game failed at startup. See $taskLog" }
        if ($taskShell.HasExited -and $taskShell.ExitCode -ne 0) { throw 'Extracted launcher failed.' }
        Start-Sleep -Milliseconds 500
    }
    if (-not $taskGameProcess) { throw "Extracted game did not start. See $taskLog" }
    while (-not $taskGameProcess.WaitForExit(1000)) {
        if ((Get-Date) -gt $taskDeadline) { throw 'Extracted game smoke timeout.' }
    }
    $taskText = Get-Content -LiteralPath $taskLog -Raw
    if ($taskGameProcess.ExitCode -ne 0 -or $taskText -notmatch 'CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE' -or $taskText -notmatch 'Result=\{Success\}.*PackagedSmoke' -or $taskText -match 'LogD3D11RHI: Error|Fatal error|Assertion failed|Test Completed\. Result=\{Fail') { throw "Extracted package smoke failed (exit $($taskGameProcess.ExitCode)): $taskLog" }
    if ($taskRun -eq 'ColdStart') {
        Copy-Item -LiteralPath $taskLog -Destination (Join-Path $taskQa 'ColdStart.log')
        Copy-Item -LiteralPath (Join-Path $taskProfile 'Desktop/Saved/Automation/Desktop/Entry.png') -Destination (Join-Path $taskQa 'ColdStart.png')
    }
    }
    $taskScreens = Join-Path $taskProfile 'Desktop/Saved/Automation/Desktop'
    foreach ($taskName in @('Entry','Overview','Build','CityInfo','ObjectivesOnly','StatsOnly','Reports','Settings','Debug','Taxes','Zone','ZoneCommercial','ZoneIndustrial','MainMenu','Speed3','Paused','Speed5','SelectedPower','Graphics','GraphicsPreview')) {
        $taskBytes = [IO.File]::ReadAllBytes((Join-Path $taskScreens "$taskName.png"))
        $taskWidth = $taskBytes[16]*16777216 + $taskBytes[17]*65536 + $taskBytes[18]*256 + $taskBytes[19]
        $taskHeight = $taskBytes[20]*16777216 + $taskBytes[21]*65536 + $taskBytes[22]*256 + $taskBytes[23]
        if ($taskWidth -ne 1280 -or $taskHeight -ne 800) { throw "Unexpected extracted screenshot dimensions: $taskName" }
    }
    [ordered]@{buildId=$BuildId;archiveAndFileHashes=$true;forbiddenPathsRejected=$true;tamperedHashRejected=$true;sourceStageRejected=$true;missingRuntimeStopsBeforeLaunch=$true;coldStartPassed=$true;relaunchPassed=$true;extractedLauncherPassed=$true;pathContainsSpaces=$true;gameExitCode=$taskGameProcess.ExitCode;captures=20;coldStartCapture=$true;viewport='1280x800';log=$taskLog;physicalInput='Pending';pcvrHeadset='Pending';quest=$(if ($taskQuestIncluded) { 'Included; static package/hash checks passed; headset pending' } else { 'Not included' })} | ConvertTo-Json | Set-Content (Join-Path $taskQa 'Summary.json')
    Write-Output "Extracted launcher/button/render/save smoke passed; 20 captures. Evidence: $taskQa"
} finally {
    [Environment]::SetEnvironmentVariable('CITYSIM_PLAYTEST_PROFILE_ROOT',$taskPreviousProfile,'Process')
    [Environment]::SetEnvironmentVariable('CITYSIM_TEST_RUNTIME_DIR',$taskPreviousRuntime,'Process')
    if ($taskGameProcess -and -not $taskGameProcess.HasExited) { Stop-Process -Id $taskGameProcess.Id }
}
