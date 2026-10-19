[CmdletBinding()]
param(
    [ValidateSet('Check', 'WindowsCandidate', 'QuestCandidate')][string]$Mode = 'Check',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$PreflightOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskProject = Join-Path $taskRoot 'CitySimVR/CitySimVR.uproject'
if (-not $env:CITYSIM_CI_ROOT) { throw 'CITYSIM_CI_ROOT must name the persistent worker output directory.' }
$taskCiRoot = [IO.Path]::GetFullPath($env:CITYSIM_CI_ROOT)
. (Join-Path $PSScriptRoot 'CityCiSmoke.ps1')
. (Join-Path $PSScriptRoot 'CityCiReservations.ps1')
if ($Mode -eq 'WindowsCandidate') {
    # Read-only prerequisite: never prompt for elevation from a build job.
    Assert-CityCiSmokeFirewall $taskCiRoot | Out-Null
}
$taskBuildScript = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$taskEditor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($taskPath in @($taskProject, $taskBuildScript, $taskEditor)) {
    if (-not (Test-Path -LiteralPath $taskPath -PathType Leaf)) { throw "Required build input missing: $taskPath" }
}
$taskVersion = Get-Content (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if ($taskVersion.MajorVersion -ne 5 -or $taskVersion.MinorVersion -ne 7 -or $taskVersion.PatchVersion -ne 4) {
    throw 'This worker requires the established Unreal 5.7.4 toolchain.'
}
$taskPlugin = Join-Path $taskRoot 'CitySimVR/Plugins/MetaXR'
$taskPluginSource = $env:CITYSIM_META_XR_SOURCE
if (-not (Test-Path -LiteralPath (Join-Path $taskPlugin 'OculusXR.uplugin'))) {
    if (-not $taskPluginSource -or -not (Test-Path -LiteralPath (Join-Path $taskPluginSource 'OculusXR.uplugin'))) {
        throw 'Restore the verified Meta XR 203.0 plugin or set CITYSIM_META_XR_SOURCE.'
    }
    $taskPluginManifest = Join-Path $taskPluginSource 'OculusXR.uplugin'
} else { $taskPluginManifest = Join-Path $taskPlugin 'OculusXR.uplugin' }
$taskPluginVersion = Get-Content -LiteralPath $taskPluginManifest -Raw | ConvertFrom-Json
if ($taskPluginVersion.VersionName -ne '1.203.0') { throw 'Expected Meta XR 203.0.' }
$taskTests = @(Get-Content (Join-Path $PSScriptRoot 'Tests.txt') | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
if (-not $taskTests.Count -or @($taskTests | Where-Object { $_ -notmatch '^CitySim\.[A-Za-z0-9_.]+$' }).Count) {
    throw 'Tests.txt must list exact CitySim test paths.'
}
if (@($taskTests | Select-Object -Unique).Count -ne $taskTests.Count) { throw 'Duplicate test paths.' }
$taskClientTests = @()
$taskClientManifest = Join-Path $PSScriptRoot 'ClientTests.txt'
if (Test-Path -LiteralPath $taskClientManifest) {
    $taskClientTests = @(Get-Content -LiteralPath $taskClientManifest | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
    if (@($taskClientTests | Where-Object { $_ -notmatch '^CitySim\.[A-Za-z0-9_.]+$' }).Count -or @($taskClientTests | Select-Object -Unique).Count -ne $taskClientTests.Count) { throw 'ClientTests.txt must contain unique exact CitySim paths.' }
}
if ($Mode -eq 'QuestCandidate') {
    # Drone overrides USERPROFILE as well as HOME/LOCALAPPDATA. Resolve the
    # original signed-in account from Windows, not the temporary build profile.
    $taskWorkerSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $taskWorkerProfile = [Environment]::ExpandEnvironmentVariables((Get-ItemProperty -LiteralPath "HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion/ProfileList/$taskWorkerSid" -Name ProfileImagePath).ProfileImagePath)
    $taskKeyStoreSource = Join-Path $taskWorkerProfile '.android/debug.keystore'
    $taskAndroidSdk = if ($env:CITYSIM_ANDROID_SDK) { $env:CITYSIM_ANDROID_SDK } else {
        Join-Path $taskWorkerProfile 'AppData/Local/Android/Sdk'
    }
    $taskJava = 'C:/Program Files/Eclipse Adoptium/jdk-21.0.12.101-hotspot/bin/java.exe'
    $taskAndroidInputs = [ordered]@{ Java=$taskJava; Javac=(Join-Path (Split-Path $taskJava) 'javac.exe');
        Android35Platform=(Join-Path $taskAndroidSdk 'platforms/android-35/android.jar');
        Ndk27c=(Join-Path $taskAndroidSdk 'ndk/27.2.12479018/source.properties');
        AndroidBuildTools=(Join-Path $taskAndroidSdk 'build-tools/35.0.1/aapt2.exe');
        ApkInspector=(Join-Path $taskAndroidSdk 'build-tools/36.1.0/aapt.exe');
        ApkSigner=(Join-Path $taskAndroidSdk 'build-tools/36.1.0/lib/apksigner.jar'); DevelopmentSigningIdentity=$taskKeyStoreSource }
    foreach ($taskAndroidInput in $taskAndroidInputs.GetEnumerator()) {
        if (-not (Test-Path -LiteralPath $taskAndroidInput.Value -PathType Leaf)) { throw "Missing Quest worker prerequisite: $($taskAndroidInput.Key)." }
    }
}
function Initialize-CiQuestSigningProfile([string]$WorkspaceRoot, [string]$ProfileRoot, [string]$SourceKeyStore) {
    $taskSigningDestination = [IO.Path]::GetFullPath((Join-Path $ProfileRoot '.android/debug.keystore'))
    if ($taskSigningDestination -eq [IO.Path]::GetFullPath($SourceKeyStore)) { return }
    $taskAllowedHome = [IO.Path]::GetFullPath((Join-Path $WorkspaceRoot 'home')).TrimEnd('\', '/')
    if (-not $taskSigningDestination.StartsWith($taskAllowedHome + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Quest signing destination must stay in this isolated workspace home.'
    }
    $taskSigningDirectory = Split-Path $taskSigningDestination -Parent
    New-Item -ItemType Directory -Path $taskSigningDirectory -Force | Out-Null
    $taskSigningIdentity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    # Restrict before copying. This private build-profile copy is removed with
    # the ephemeral workspace and never enters source or artifact archives.
    & icacls $taskSigningDirectory /inheritance:r /grant:r "${taskSigningIdentity}:(OI)(CI)(F)" '*S-1-5-18:(OI)(CI)(F)' '*S-1-5-32-544:(OI)(CI)(F)' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not restrict the isolated signing directory.' }
    Copy-Item -LiteralPath $SourceKeyStore -Destination $taskSigningDestination -Force
}
if ($PreflightOnly) {
    Write-Output "Preflight passed: Unreal 5.7.4, Meta XR 203.0, $($taskTests.Count) exact tests; mode $Mode. No build started."
    return
}
if ($env:DRONE_REPO -ne 'CitySimVR/citysim' -or $env:DRONE_COMMIT_SHA -notmatch '^[a-fA-F0-9]{40}$' -or $env:DRONE_BUILD_NUMBER -notmatch '^\d+$') {
    throw 'Builds require Drone repository, commit and build-number metadata; use -PreflightOnly for a local inspection.'
}
# Never build from the mutable development checkout. Drone supplies an ephemeral clone.
if (-not $env:CITYSIM_DEVELOPMENT_ROOT) { throw 'Worker must configure CITYSIM_DEVELOPMENT_ROOT for checkout isolation.' }
if ($taskRoot.TrimEnd('\', '/') -eq ([IO.Path]::GetFullPath($env:CITYSIM_DEVELOPMENT_ROOT)).TrimEnd('\', '/')) {
    throw 'Use the isolated Drone checkout, not the development working tree.'
}
if ($taskCiRoot.StartsWith($taskRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Persistent CI output must be outside the ephemeral build checkout.'
}
$taskHead = (& git -C $taskRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the checked-out commit.' }
if ($env:DRONE_BUILD_EVENT -eq 'pull_request' -or
    ($env:DRONE_BUILD_EVENT -eq 'promote' -and $env:DRONE_COMMIT_REF -match '^refs/pull/[1-9][0-9]*/head$')) {
    # Drone preserves the PR ref/merge checkout when promoting a PR build.
    # Its built-in clone merges the submitted head into the current target branch.
    # Record that actual merge commit and prove it includes the submitted commit.
    & git -C $taskRoot merge-base --is-ancestor $env:DRONE_COMMIT_SHA $taskHead
    if ($LASTEXITCODE -ne 0) { throw 'PR checkout does not include the submitted commit.' }
} elseif ($taskHead -ne $env:DRONE_COMMIT_SHA) { throw 'Checkout does not match the Drone commit.' }
& git -C $taskRoot lfs pull
if ($LASTEXITCODE -ne 0) { throw 'Git LFS download failed.' }
& git -C $taskRoot lfs fsck
if ($LASTEXITCODE -ne 0) { throw 'Git LFS integrity check failed.' }
$taskDirty = & git -C $taskRoot status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $taskDirty) { throw 'Tracked inputs must match the checked-out commit.' }
$taskRunId = '{0}-{1}-{2}-{3}' -f $env:DRONE_BUILD_NUMBER, $taskHead.Substring(0, 12), $Mode, ([Guid]::NewGuid().ToString('N').Substring(0, 8))
$taskOutput = Join-Path $taskCiRoot "Builds/$taskRunId"
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
# File locking works across Windows sessions; acquire before any Unreal/plugin operation.
$taskLock = $null
try {
    $taskLock = [IO.File]::Open((Join-Path $taskCiRoot 'unreal-build.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
} catch { throw 'The CI Unreal build slot is in use. Retry when the owning job finishes.' }
$taskReceipt = [ordered]@{ commit = $taskHead; submittedCommit = $env:DRONE_COMMIT_SHA; build = $env:DRONE_BUILD_NUMBER; mode = $Mode; tests = $taskTests; startedUtc = [DateTime]::UtcNow.ToString('o'); status = 'running' }
$taskPreviousDdc = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath')
function Invoke-CiNative([string]$File, [string[]]$Arguments, [string]$Log) {
    & $File @Arguments 2>&1 | Tee-Object -FilePath $Log | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Native command failed ($LASTEXITCODE); see $Log" }
}
function Invoke-CiTests([string]$Executable, [string[]]$Prefix, [string]$Name, [string[]]$ExpectedTests = $taskTests) {
    $taskLog = Join-Path $taskOutput "$Name.log"
    $taskProfile = Join-Path $taskOutput "$Name-User"
    # Accepted shopping/outpatient/health regressions use immutable captured cities. Only
    # hash-pinned Artifacts inputs are copied; real player profiles are never read.
    $taskFixtureArguments = @()
    if ($taskGameplayFixtures.Count) {
        $taskProfile = Join-Path $taskOutput "HomeInspection-Outpatient-$Name-User"
        # The Editor phase creates these before packaging. Client test ordering
        # is not guaranteed, so chart tests read that completed native fixture set.
        $taskContinuation = Join-Path $taskOutput "$Name-Continuation"
        $taskFixtureArguments = @("-ShoppingAllocationSave=`"$($taskGameplayFixtures.shopping)`"", "-ShoppingAllocationOutput=`"$taskContinuation`"",
            "-OutpatientFixtureRoot=`"$taskOutput/EditorTests-Continuation`"", "-HealthServiceSave=`"$($taskGameplayFixtures.health)`"", "-HealthServiceOutput=`"$taskOutput/$Name-HealthService`"")
    }
    $taskFilter = $ExpectedTests -join '+'
    # Routine CI does not collect Insights traces. Drone's isolated profile
    # otherwise gives the auto-launched server a new firewall identity per job.
    $taskArguments = $Prefix + $taskFixtureArguments + @('-notraceserver', '-d3d11', '-RenderOffscreen', '-nohmd', '-unattended', '-nop4', '-nosplash',
        "-UserDir=`"$taskProfile`"", "-abslog=`"$taskLog`"", "-ReportExportPath=`"$taskOutput/$Name-Report`"",
        "-ExecCmds=`"Automation RunTests $taskFilter`"", '-TestExit="Automation Test Queue Empty"')
    $taskProcess = Start-Process -FilePath $Executable -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
    if (-not $taskProcess.WaitForExit(900000)) {
        Stop-Process -Id $taskProcess.Id -ErrorAction SilentlyContinue
        throw "Owned CI test process timed out; see $taskLog"
    }
    & (Join-Path $PSScriptRoot 'Test-CityAutomationLog.ps1') -LogPath $taskLog -ExpectedTests $ExpectedTests -ProcessExitCode $taskProcess.ExitCode
}
$taskGameplayFixtures = @{}
try {
$taskGameplayManifest = Join-Path $PSScriptRoot 'GameplayFixtures.json'
if (Test-Path -LiteralPath $taskGameplayManifest) {
    $taskGameplayFixtureList = Get-Content -LiteralPath $taskGameplayManifest -Raw | ConvertFrom-Json
    foreach ($taskFixture in $taskGameplayFixtureList) {
        if ($taskFixture.name -notin @('shopping','health') -or $taskFixture.path -notmatch '^Artifacts/[A-Za-z0-9_./-]+[.]json$' -or $taskFixture.path.Contains('..') -or $taskFixture.sha256 -notmatch '^[a-f0-9]{64}$' -or $taskGameplayFixtures.ContainsKey($taskFixture.name)) { throw 'Invalid gameplay fixture manifest.' }
        $taskFixtureSource = Join-Path $env:CITYSIM_DEVELOPMENT_ROOT $taskFixture.path
        if ((Get-FileHash -LiteralPath $taskFixtureSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskFixture.sha256) { throw 'Captured gameplay fixture hash mismatch.' }
        $taskFixtureCopy = Join-Path $taskOutput "$($taskFixture.name)-fixture.json"
        Copy-Item -LiteralPath $taskFixtureSource -Destination $taskFixtureCopy
        if ((Get-FileHash -LiteralPath $taskFixtureCopy -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskFixture.sha256) { throw 'Copied gameplay fixture hash mismatch.' }
        $taskGameplayFixtures[$taskFixture.name] = $taskFixtureCopy
    }
    if ($taskGameplayFixtures.Count -ne 2) { throw 'Both captured gameplay fixtures are required.' }
    $taskReceipt.gameplayFixtures = @($taskGameplayFixtureList)
}
    # The working plan reserves source/build ownership even while an owner is
    # authoring code and has no native process running yet.
    $taskPlanPath = Join-Path $env:CITYSIM_DEVELOPMENT_ROOT 'docs/DEVELOPMENT_TASKS.md'
    $taskPlan = Get-Content -LiteralPath $taskPlanPath -Raw
    $taskClaims = @(Get-CityLocalNativeClaims $taskPlan)
    if ($taskClaims.Count) {
        throw "Native build window is reserved: $($taskClaims[0].Groups['owner'].Value). Retry after that owner releases it."
    }
    # Respect any existing local Unreal/UAT process; never stop a player session.
    $taskActive = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
    })
    if ($taskActive.Count) { throw 'Another native Unreal build/editor owns this machine; retry after it releases the build window.' }
    if (-not (Test-Path -LiteralPath (Join-Path $taskPlugin 'OculusXR.uplugin'))) {
        New-Item -ItemType Directory -Path $taskPlugin -Force | Out-Null
        & robocopy $taskPluginSource $taskPlugin /E /XD Intermediate Saved /NFL /NDL /NJH /NJS /NP | Out-Host
        if ($LASTEXITCODE -ge 8) { throw 'Meta XR copy failed.' }
    }
    & (Join-Path $taskRoot 'Tools/Patch-MetaXR203.ps1')
    $taskDdc = Join-Path $taskCiRoot 'DerivedDataCache'
    New-Item -ItemType Directory -Path $taskDdc -Force | Out-Null
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $taskDdc)
    Invoke-CiNative $taskBuildScript @('CitySimVREditor', 'Win64', 'Development', $taskProject, '-WaitMutex', '-NoHotReloadFromIDE', '-WarningsAsErrors') (Join-Path $taskOutput 'EditorBuild.log')
    # Asset imports require Editor and share this worker's existing native lock.
    # Keep this optional and explicit; retain the exact products for review/commit.
    $taskPreparationManifest = Join-Path $PSScriptRoot 'AssetPreparation.txt'
    if (Test-Path -LiteralPath $taskPreparationManifest) {
        $taskPreparations = @(Get-Content -LiteralPath $taskPreparationManifest | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
        foreach ($taskPreparation in $taskPreparations) {
            if ($taskPreparation -notmatch '^Tools/[A-Za-z0-9_-]+\.py$') { throw 'Invalid asset preparation path.' }
            $taskPreparationPath = Join-Path $taskRoot $taskPreparation
            if (-not (Test-Path -LiteralPath $taskPreparationPath -PathType Leaf)) { throw 'Missing asset preparation script.' }
            $taskPreparationName = [IO.Path]::GetFileNameWithoutExtension($taskPreparation)
            $taskPreparationArguments = @("`"$taskProject`"", '-run=pythonscript', "-script=`"$taskPreparationPath`"", '-AllowCommandletRendering', '-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.PNG=0', '-notraceserver', '-d3d11', '-RenderOffscreen', '-nohmd', '-unattended', '-nop4', '-nosplash',
                "-UserDir=`"$taskOutput/AssetPreparation-User`"", "-abslog=`"$taskOutput/$taskPreparationName-Editor.log`"")
            # Native XR startup writes informational lines to stderr. PS5 turns
            # those into terminating NativeCommandError records with 2>&1.
            $taskPreparationProcess = Start-Process -FilePath $taskEditor -ArgumentList $taskPreparationArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOutput "$taskPreparationName.log") -RedirectStandardError (Join-Path $taskOutput "$taskPreparationName-stderr.log")
            # Cache the handle before WaitForExit: PS5 otherwise loses ExitCode
            # on redirected Start-Process instances.
            $taskPreparationHandle = $taskPreparationProcess.Handle
            if (-not $taskPreparationProcess.WaitForExit(900000)) {
                Stop-Process -Id $taskPreparationProcess.Id -ErrorAction SilentlyContinue
                throw 'Owned asset preparation timed out.'
            }
            if ($taskPreparationProcess.ExitCode -ne 0) { throw "Asset preparation exited $($taskPreparationProcess.ExitCode); see retained Editor/stdout/stderr logs." }
        }
        $taskPreparedList = Join-Path $taskRoot 'Artifacts/CIAssetPreparation/Files.txt'
        foreach ($taskPreparedRelative in @(Get-Content -LiteralPath $taskPreparedList)) {
            if ($taskPreparedRelative -notmatch '^CitySimVR/Content/CitySim/[A-Za-z0-9_./-]+\.uasset$' -or $taskPreparedRelative.Contains('..')) { throw 'Invalid prepared asset path.' }
            $taskPreparedSource = Join-Path $taskRoot $taskPreparedRelative
            $taskPreparedDestination = Join-Path $taskOutput "PreparedAssets/$taskPreparedRelative"
            New-Item -ItemType Directory -Force (Split-Path $taskPreparedDestination) | Out-Null
            Copy-Item -LiteralPath $taskPreparedSource -Destination $taskPreparedDestination
        }
        Copy-Item -LiteralPath (Join-Path $taskRoot 'Artifacts/CIAssetPreparation') -Destination (Join-Path $taskOutput 'AssetPreparationReceipts') -Recurse
    }
    Invoke-CiTests $taskEditor @("`"$taskProject`"") 'EditorTests'
    # Optional native visual studies use this exact compiled revision without
    # packaging a game or launching a real player's maintained executable.
    $taskVisualManifest = Join-Path $PSScriptRoot 'EditorVisualTests.txt'
    if ($Mode -eq 'Check' -and (Test-Path -LiteralPath $taskVisualManifest)) {
        $taskVisualTests = @(Get-Content -LiteralPath $taskVisualManifest | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
        if (-not $taskVisualTests.Count -or @($taskVisualTests | Where-Object { $_ -notmatch '^CitySim\.[A-Za-z0-9_.]+$' }).Count -or @($taskVisualTests | Select-Object -Unique).Count -ne $taskVisualTests.Count) { throw 'EditorVisualTests.txt must contain unique exact test paths.' }
        Invoke-CiTests $taskEditor @("`"$taskProject`"", '-game', '-CityDesktop', '-CityGroundPlotStudy', '-CityFoundationStudy', '-CityHomeGroundPilot', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=900') 'EditorVisualTests' $taskVisualTests
        $taskReceipt.editorVisualTests = $taskVisualTests
    }
    if ($Mode -eq 'WindowsCandidate') {
        $taskArchive = Join-Path $taskOutput 'WindowsCandidate'
        & (Join-Path $taskRoot 'Tools/Build-PCVRSandbox.ps1') -EngineRoot $EngineRoot -SkipEditorCompilation -NoTraceServer -ArchiveDirectory $taskArchive 2>&1 |
            Tee-Object -FilePath (Join-Path $taskOutput 'WindowsPackage.log') | Out-Host
        # Reuse the established narrow menu/render smoke; tests above include Editor-only fixtures.
        Assert-CityCiSmokeFirewall $taskCiRoot | Out-Null
        $taskSmokeRuntime = Initialize-CityCiSmokeRuntime -CiRoot $taskCiRoot -CandidateDirectory $taskArchive -ReportPath (Join-Path $taskOutput 'SmokeRuntime.json')
        $taskExecutable = $taskSmokeRuntime.executable
        try {
        $taskSmoke = Join-Path $taskRoot "Artifacts/CI-$taskRunId"
        try {
            & (Join-Path $taskRoot 'Tools/Test-DesktopSandbox.ps1') -ExecutablePath $taskExecutable -MenusOnly -NoTraceServer -RunName "CI-$taskRunId"
        } finally {
            # Preserve failed smoke logs/captures before Drone removes its clone.
            if (Test-Path -LiteralPath $taskSmoke) {
                Copy-Item -LiteralPath $taskSmoke -Destination (Join-Path $taskOutput 'PackagedSmoke') -Recurse
            }
        }
        if ($taskClientTests.Count) {
            foreach ($taskSize in @(@(1280,800), @(1024,720))) {
                $taskClientName = "Client-$($taskSize[0])"
                Invoke-CiTests $taskExecutable @('-CityDesktop','-CityPackagedAutomation','-windowed','-ForceRes',"-ResX=$($taskSize[0])","-ResY=$($taskSize[1])") $taskClientName $taskClientTests
            }
            $taskReceipt.clientTests = $taskClientTests
        }
        $taskReceipt.smokeRuntime = [ordered]@{ executable=$taskExecutable; inventory='SmokeRuntime.json'; fileCount=$taskSmokeRuntime.fileCount; inbound='Block' }
        } finally { $taskSmokeRuntime.lock.Dispose() }
        $taskZip = Join-Path $taskOutput 'CitySimVR-Windows.zip'
        & tar -a -cf $taskZip -C $taskArchive .
        if ($LASTEXITCODE -ne 0) { throw 'Candidate archive failed.' }
        $taskReceipt.packageSha256 = (Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    if ($Mode -eq 'QuestCandidate') {
        $taskArchive = Join-Path $taskOutput 'QuestCandidate'
        Initialize-CiQuestSigningProfile -WorkspaceRoot ([IO.Path]::GetFullPath((Join-Path $taskRoot '../..'))) `
            -ProfileRoot ([Environment]::GetFolderPath('UserProfile')) -SourceKeyStore $taskKeyStoreSource
        # Cooker uses this path verbatim; staging appends the platform unless
        # it is already the final component. Both must read the same directory.
        # The shorter persistent path also avoids MAX_PATH in the deep clone.
        $taskCookOutput = Join-Path $taskOutput 'Cooked/Android_ASTC'
        & (Join-Path $taskRoot 'Tools/Build-QuestSandbox.ps1') -EngineRoot $EngineRoot -SkipEditorCompilation -NoTraceServer -ArchiveDirectory $taskArchive -AndroidSdkPath $taskAndroidSdk -CookOutputDirectory $taskCookOutput 2>&1 |
            Tee-Object -FilePath (Join-Path $taskOutput 'QuestPackage.log') | Out-Host
        $taskApks = @(Get-ChildItem -LiteralPath $taskArchive -Recurse -File -Filter '*.apk')
        if ($taskApks.Count -ne 1) { throw 'Expected one archived Quest APK.' }
        $taskVerification = Join-Path $taskOutput 'QuestVerification.json'
        & (Join-Path $PSScriptRoot 'Test-CityQuestCandidate.ps1') -ApkPath $taskApks[0].FullName -AndroidSdkPath $taskAndroidSdk -JavaPath $taskJava `
            -StagedPakDirectory (Join-Path $taskRoot 'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks') -ReportPath $taskVerification 2>&1 |
            Tee-Object -FilePath (Join-Path $taskOutput 'QuestVerification.log') | Out-Host
        $taskQuestResult = Get-Content -LiteralPath $taskVerification -Raw | ConvertFrom-Json
        $taskReceipt.packagePath = $taskApks[0].FullName.Substring($taskOutput.Length + 1).Replace('\', '/')
        $taskReceipt.packageSha256 = $taskQuestResult.apkSha256
        $taskReceipt.signerSha256 = $taskQuestResult.signerSha256
        $taskReceipt.questVerification = 'QuestVerification.json'
        $taskReceipt.headset = 'unconfirmed-not-installed'
    }
    $taskReceipt.status = 'passed'
} catch {
    $taskReceipt.status = 'failed'
    $taskReceipt.error = $_.Exception.Message
    throw
} finally {
    $taskReceipt.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $taskReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskOutput 'Result.json') -Encoding UTF8
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $taskPreviousDdc)
    if ($taskLock) { $taskLock.Dispose() }
    Write-Output "CI result and artifacts retained at $taskOutput"
}
