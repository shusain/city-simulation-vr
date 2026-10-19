[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet(100, 500, 1000)]
    [int]$Lots,
    [ValidateRange(0, 4096)]
    [int]$RoadCells = 64,
    [ValidateRange(0, 4096)]
    [int]$Vehicles = 32,
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 1200,
    [ValidateRange(0, 300)]
    [int]$WarmupSeconds = 10,
    [ValidateRange(1, 60)]
    [int]$SampleIntervalSeconds = 10,
    [string]$Serial,
    [string]$ApkPath,
    [switch]$Capture
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskAdb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$taskPackage = 'com.shaun.citysimvr.smoketest'
if (-not (Test-Path -LiteralPath $taskAdb)) { throw "ADB not found: $taskAdb" }
if (-not $Serial) {
    $taskDevices = @(& $taskAdb devices | Where-Object { $_ -match '^\S+\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
    if ($taskDevices.Count -ne 1) { throw 'Connect one authorized headset or specify -Serial.' }
    $Serial = $taskDevices[0]
}
if (-not $ApkPath) {
    $taskApks = @(Get-ChildItem (Join-Path $taskRoot 'Artifacts\QuestSandbox') -Recurse -Filter *.apk)
    if ($taskApks.Count -ne 1) { throw 'Specify -ApkPath: expected exactly one sandbox APK.' }
    $ApkPath = $taskApks[0].FullName
}
if (-not (Test-Path -LiteralPath $ApkPath)) { throw "APK not found: $ApkPath" }

$taskActivity = (& $taskAdb -s $Serial shell cmd package resolve-activity --brief $taskPackage | Select-Object -Last 1).Trim()
if ($taskActivity -notlike "$taskPackage/*") { throw "Could not resolve launch activity: $taskActivity" }
$taskCommandLine = "-CityBenchmarkLots=$Lots -CityBenchmarkRoads=$RoadCells -CityBenchmarkVehicles=$Vehicles -CityBenchmarkDuration=$DurationSeconds -CityBenchmarkWarmup=$WarmupSeconds -CityBenchmarkSampleInterval=$SampleIntervalSeconds"
$taskShellCommandLine = $taskCommandLine.Replace(' ', '\ ')
$taskStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$taskOutputDirectory = Join-Path $taskRoot "Artifacts\Benchmarks\${taskStamp}-${Lots}lots"
New-Item -ItemType Directory -Force -Path $taskOutputDirectory | Out-Null

$taskMetadata = [ordered]@{
    startedUtc = (Get-Date).ToUniversalTime().ToString('o')
    serial = $Serial
    deviceModel = (& $taskAdb -s $Serial shell getprop ro.product.model).Trim()
    osRelease = (& $taskAdb -s $Serial shell getprop ro.build.version.release).Trim()
    osBuild = (& $taskAdb -s $Serial shell getprop ro.build.display.id).Trim()
    apkPath = $ApkPath
    apkBytes = (Get-Item -LiteralPath $ApkPath).Length
    apkSha256 = (Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash
    gitCommit = (& git -c safe.directory=C:/Users/shaun/Development/city-sim-builder -C $taskRoot rev-parse HEAD).Trim()
    buildConfiguration = 'Development'
    lotsRequested = $Lots
    roadsRequested = $RoadCells
    vehiclesRequested = $Vehicles
    durationSeconds = $DurationSeconds
    warmupSeconds = $WarmupSeconds
    sampleIntervalSeconds = $SampleIntervalSeconds
    commandLine = $taskCommandLine
}
$taskMetadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskOutputDirectory 'run.json') -Encoding UTF8

& $taskAdb -s $Serial logcat -c
& $taskAdb -s $Serial shell am force-stop $taskPackage
& $taskAdb -s $Serial shell am start -W -n $taskActivity --es cmdline $taskShellCommandLine
if ($LASTEXITCODE -ne 0) { throw 'Benchmark activity launch failed.' }

Write-Output "Launched $Lots-lot benchmark on $($taskMetadata.deviceModel)."
Write-Output "Evidence directory: $taskOutputDirectory"
if (-not $Capture) {
    Write-Output "Capture is running in-headset for $DurationSeconds seconds after a $WarmupSeconds-second warmup."
    Write-Output 'Rerun with -Capture to keep this script attached and collect the completed log automatically.'
    return
}

$taskBeginDeadline = (Get-Date).AddSeconds([Math]::Max(30, $WarmupSeconds + 20))
$taskBegan = $false
$taskMissingPidPolls = 0
while ((Get-Date) -lt $taskBeginDeadline) {
    Start-Sleep -Seconds 5
    $taskPid = (@(& $taskAdb -s $Serial shell pidof $taskPackage) -join '').Trim()
    if (-not $taskPid) {
        $taskMissingPidPolls++
        if ($taskMissingPidPolls -ge 3) { throw 'Benchmark app was absent for three consecutive polls before capture began.' }
        continue
    }
    $taskMissingPidPolls = 0
    $taskStartupLog = & $taskAdb -s $Serial logcat -d -v brief
    if (@($taskStartupLog | Select-String -Pattern 'CITYSIM_BENCHMARK_BEGIN').Count -gt 0) {
        $taskBegan = $true
        break
    }
}
if (-not $taskBegan) {
    throw 'Benchmark did not enter capture mode. Wake/wear the headset so the app remains foregrounded, then rerun.'
}

$taskDeadline = (Get-Date).AddSeconds($DurationSeconds + 10)
$taskMissingPidPolls = 0
while ((Get-Date) -lt $taskDeadline) {
    Start-Sleep -Seconds 5
    $taskPid = (@(& $taskAdb -s $Serial shell pidof $taskPackage) -join '').Trim()
    if (-not $taskPid) {
        $taskMissingPidPolls++
        if ($taskMissingPidPolls -ge 3) { throw 'Benchmark app was absent for three consecutive polls during capture.' }
        continue
    }
    $taskMissingPidPolls = 0
    $taskProgressLog = & $taskAdb -s $Serial logcat -d -v brief
    if (@($taskProgressLog | Select-String -Pattern 'CITYSIM_BENCHMARK_COMPLETE').Count -gt 0) {
        break
    }
}

$taskLog = & $taskAdb -s $Serial logcat -d -v threadtime
$taskEvidence = @($taskLog | Select-String -Pattern 'CITYSIM_BENCHMARK_|CITYSIM_LIFECYCLE_|CITYSIM_INPUT_EDGE' | ForEach-Object { $_.Line })
$taskEvidence | Set-Content -LiteralPath (Join-Path $taskOutputDirectory 'citysim-log.txt') -Encoding UTF8
& $taskAdb -s $Serial shell dumpsys meminfo $taskPackage | Set-Content -LiteralPath (Join-Path $taskOutputDirectory 'meminfo.txt') -Encoding UTF8
$taskComplete = @($taskEvidence | Where-Object { $_ -match 'CITYSIM_BENCHMARK_COMPLETE' })
if ($taskComplete.Count -ne 1) {
    throw "Expected one completed benchmark record; found $($taskComplete.Count). See $taskOutputDirectory"
}
Write-Output $taskComplete[0]
Write-Output "Benchmark evidence saved to $taskOutputDirectory"
