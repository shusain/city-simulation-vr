[CmdletBinding()]
param(
    [ValidateRange(1,36864)][int]$Lots = 500,
    [switch]$Populated,
    [switch]$LateGame,
    [ValidateSet(64,80,96,112,128,160,192)][int]$ExpectedGridSize = 64,
    [ValidateRange(1,2048)][int]$CitizenRoutes = 32,
    [ValidateRange(1,2048)][int]$VisibleTraffic = 64,
    [ValidateSet(50,75,100,125)][int]$SceneScale = 100,
    [ValidateSet(2,4,8)][int]$MSAASamples = 4,
    [switch]$DpiSceneScaling,
    [ValidateRange(1,3600)][int]$DurationSeconds = 20,
    [ValidateRange(0,300)][int]$WarmupSeconds = 5,
    [ValidateRange(90,1200)][int]$StartupTimeoutSeconds = 90,
    [ValidateRange(1024,7680)][int]$Width = 1280,
    [ValidateRange(720,4320)][int]$Height = 800,
    [ValidateSet('Hidden','Visible','Stats','Menu')][string]$Hud = 'Stats',
    [string]$SavedCityPath,
    [ValidateSet(0,1,3,5)][int]$Speed = 0,
    [ValidateSet('Natural','Clear','Rain')][string]$Weather = 'Natural',
    [ValidateSet('Day','Dusk','Night')][string]$Lighting = 'Day',
    [switch]$LegacyColor,
    [switch]$LegacyShadows,
    [switch]$LegacyCitizens,
	[switch]$StaticParkCitizens,
	[switch]$ParkMotion,
    [ValidateSet(0,2048,4096)][int]$CSMResolution = 0,
    [switch]$Capture,
    [switch]$UncachedSpatial,
    [switch]$UncachedRouting,
    [switch]$ReferenceServices,
    [switch]$ReferencePopulation,
    [switch]$ReferenceServiceReuse,
    [switch]$ReferenceCare,
    [ValidateRange(0,100000)][int]$CaptureStartStep = 0,
    [string]$ExecutablePath,
    [switch]$PrintOnly
)
$ErrorActionPreference = 'Stop'
if ($LateGame -and -not $Populated) { throw 'LateGame requires Populated.' }
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskExe = if ($ExecutablePath) { (Resolve-Path -LiteralPath $ExecutablePath).Path } else { Join-Path $taskRoot 'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe' }
if (-not (Test-Path -LiteralPath $taskExe -PathType Leaf)) { throw 'Build the Windows package first.' }
$taskLook = if ($LegacyColor) { 'Legacy' } else { 'Neutral' }
$taskShadowProfile = if ($LegacyShadows) { 'LegacyShadows' } else { 'TabletopShadows' }
$taskSource = if ($SavedCityPath) { (Resolve-Path -LiteralPath $SavedCityPath).Path } else { $null }
if ($taskSource -and ($PSBoundParameters.ContainsKey('Lots') -or $Populated)) { throw 'Choose SavedCityPath or Lots, not both.' }
$taskSourceHash = if ($taskSource) { (Get-FileHash -LiteralPath $taskSource).Hash } else { $null }
$taskScene = if ($taskSource) { "Saved-$Speed-$Weather" } elseif ($LateGame) { "Late-$ExpectedGridSize-$Lots-$Speed" } else { "$Lots" }
$taskOutput = Join-Path $taskRoot ("Artifacts/Benchmarks/Desktop-{0}-{1}-{2}-{3}-{4}x{5}-{6}-{7}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'),$taskScene,$Hud,$Lighting,$Width,$Height,$taskLook,$taskShadowProfile)
$taskLog = Join-Path $taskOutput 'Play.log'
$taskReport = Join-Path $taskOutput 'Report.json'
$taskArgs = @('-CityDesktop','-nohmd','-d3d11','-windowed','-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",'-nosound',"-CityBenchmarkDuration=$DurationSeconds","-CityBenchmarkWarmup=$WarmupSeconds",'-CityBenchmarkSampleInterval=10',"-CityBenchmarkHud=$Hud","-CityBenchmarkLighting=$Lighting",'-CityBenchmarkExit',"-CityBenchmarkReport=`"$taskReport`"","-UserDir=`"$(Join-Path $taskOutput 'User')`"","-abslog=`"$taskLog`"",('-ExecCmds="r.VSync 0,t.MaxFPS 0,t.IdleWhenNotForeground 0,r.ScreenPercentage ' + $SceneScale + ',r.SecondaryScreenPercentage.GameViewport ' + $(if ($DpiSceneScaling) { 0 } else { 100 }) + ',r.MSAACount ' + $MSAASamples + '"'),'-unattended','-stdout')
if ($taskSource) {
    $taskArgs += "-CityBenchmarkSave=`"$(Join-Path $taskOutput 'InputCity.json')`"", "-CityBenchmarkSpeed=$Speed", "-CityBenchmarkWeather=$Weather"
} else {
    if (-not $Populated -and ($Speed -ne 0 -or $Weather -ne 'Natural')) { throw 'Speed/weather controls require SavedCityPath.' }
    $taskArgs += "-CityBenchmarkLots=$Lots", '-CityBenchmarkRoads=64', '-CityBenchmarkVehicles=32'
}
if ($Populated) { $taskArgs += '-CityBenchmarkPopulated', "-CityBenchmarkSpeed=$Speed", "-CityBenchmarkWeather=$Weather" }
if ($LateGame) { $taskArgs += '-CityBenchmarkLateGame' }
$taskArgs += "-CityBenchmarkCitizenRoutes=$CitizenRoutes", "-CityBenchmarkVisibleTraffic=$VisibleTraffic"
if ($LegacyColor) { $taskArgs += '-CityLegacyWindowsColor' }
if ($LegacyShadows) { $taskArgs += '-CityLegacyWindowsShadows' }
if ($LegacyCitizens) { $taskArgs += '-CityLegacyCitizens' }
if ($CSMResolution) {
    $taskArgs = @($taskArgs | ForEach-Object { if ($_ -like '-ExecCmds=*') { $_.Substring(0, $_.Length - 1) + ',r.Shadow.MaxCSMResolution ' + $CSMResolution + '"' } else { $_ } })
}
if ($Capture) { $taskArgs += '-CityBenchmarkCapture' }
if ($UncachedSpatial) {
    if (-not $Populated -or $SavedCityPath) { throw 'UncachedSpatial requires a generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkUncachedSpatial'
}
if ($UncachedRouting) {
    if (-not $Populated -or $SavedCityPath) { throw 'UncachedRouting requires a generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkUncachedRouting'
}
if ($ReferenceServices) {
    if (-not $Populated -or $SavedCityPath) { throw 'ReferenceServices requires a generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkReferenceServices'
}
if ($ReferencePopulation) {
    if (-not $Populated -or $SavedCityPath -or $Speed -eq 0) { throw 'ReferencePopulation requires a running generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkReferencePopulation'
}
if ($ReferenceServiceReuse) {
    if (-not $Populated -or $SavedCityPath -or $Speed -eq 0) { throw 'ReferenceServiceReuse requires a running generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkReferenceServiceReuse'
}
if ($ReferenceCare) {
    if (-not $Populated -or $SavedCityPath -or $Speed -eq 0) { throw 'ReferenceCare requires a running generated populated benchmark.' }
    $taskArgs += '-CityBenchmarkReferenceCare'
}
if ($StaticParkCitizens) { $taskArgs += '-CityStaticParkCitizens' }
if ($ParkMotion) { $taskArgs += '-CityBenchmarkParkMotion' }
if ($CaptureStartStep) {
    if (-not $Populated -or -not $LateGame -or $SavedCityPath -or $Speed -eq 0) { throw 'CaptureStartStep requires a running generated late-game benchmark.' }
    $taskArgs += "-CityBenchmarkCaptureStartStep=$CaptureStartStep"
}
Write-Output "$taskExe $($taskArgs -join ' ')"
if ($PrintOnly) { return }
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
if ($taskSource) { Copy-Item -LiteralPath $taskSource -Destination (Join-Path $taskOutput 'InputCity.json') }
$taskStartedUtc = (Get-Date).ToUniversalTime().ToString('o')
$taskProcess = Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WorkingDirectory (Split-Path $taskExe -Parent) -WindowStyle Hidden -PassThru
# Bounded polls leave the shell interruptible; only this owned test process is stopped on timeout.
$taskDeadline = (Get-Date).AddSeconds($WarmupSeconds + $DurationSeconds + $StartupTimeoutSeconds)
while (-not $taskProcess.WaitForExit(1000)) {
    if (Test-Path -LiteralPath $taskLog) {
        $taskStartupLog = Get-Content -LiteralPath $taskLog -Tail 40 | Out-String
        if ($taskStartupLog -match 'CITYSIM_BENCHMARK_(LOAD|BUILD)_FAILED|CITYSIM_BENCHMARK_INVALID|CITYSIM_POPULATION_RECONCILIATION_FAILED|Handled ensure|Ensure condition failed|Fatal error|Assertion failed') {
            Stop-Process -Id $taskProcess.Id
            throw "Benchmark startup failed: $taskLog"
        }
    }
    if ((Get-Date) -gt $taskDeadline) { Stop-Process -Id $taskProcess.Id; throw "Benchmark timeout: $taskLog" }
}
$taskText = Get-Content -LiteralPath $taskLog -Raw
if ($taskProcess.ExitCode -ne 0 -or $taskText -notmatch 'CITYSIM_BENCHMARK_COMPLETE' -or $taskText -match 'LogD3D11RHI: Error|Fatal error|Assertion failed|Handled ensure|Ensure condition failed|CITYSIM_POPULATION_RECONCILIATION_FAILED|CITYSIM_BENCHMARK_(REPORT|LOAD|BUILD)_FAILED|CITYSIM_BENCHMARK_INVALID') { throw "Benchmark failed: $taskLog" }
$taskData = Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
if ($taskData.width -ne $Width -or $taskData.height -ne $Height -or $taskData.frames -lt 1 -or $taskData.captureSeconds -lt $DurationSeconds -or $taskData.targetHz -ne $null -or $taskData.metaDroppedFrames -ne $null) { throw 'Unexpected benchmark dimensions/timing/desktop telemetry.' }
if ($taskData.profile -ne 'Desktop' -or $taskData.rhi -ne 'D3D11') { throw 'Unexpected runtime/rendering mode.' }
if ($taskData.gridWidth -ne $ExpectedGridSize -or $taskData.gridHeight -ne $ExpectedGridSize -or $taskData.generatedLateGame -ne [bool]$LateGame) { throw 'Unexpected grid size/late-game fixture. Check the experimental executable.' }
if ($LateGame -and ($taskData.waterRulesVersion -ne 1 -or $taskData.developedCellPercent -lt 75 -or $taskData.buildingTypes.Police -lt 1 -or $taskData.buildingTypes.Hospital -lt 1 -or $taskData.buildingTypes.WasteDepot -lt 1 -or $taskData.trafficRecordsPeak -lt 1)) { throw 'Late-game fixture did not cover the map with current water rules, mixed civic lots and actual traffic.' }
if ($LateGame -and ($taskData.operationalHomesEnd -lt $taskData.buildingTypes.Home * .75 -or $taskData.operationalShopsEnd -lt $taskData.buildingTypes.Shop * .75)) { throw 'Late-game fixture has too many inactive homes/shops to represent an operating city.' }
if ($taskData.frameTimingSource -ne 'MonotonicWallClock' -or [Math]::Abs($taskData.frames * $taskData.frameMeanMs - $taskData.captureSeconds * 1000) -gt .01) { throw 'Frame/capture timing did not use consistent unclamped wall time.' }
if ($taskSource) {
    if ((Get-FileHash -LiteralPath $taskSource).Hash -ne $taskSourceHash) { throw 'Source save changed during benchmark.' }
    if ((Get-FileHash -LiteralPath (Join-Path $taskOutput 'InputCity.json')).Hash -ne $taskSourceHash) { throw 'Benchmark rewrote its input copy.' }
}
if ($taskSource -or $Populated) {
    if ($taskData.simulationSpeed -ne $Speed -or $taskData.weatherControl -ne $Weather) { throw 'Unexpected simulation profile.' }
    if (($Weather -eq 'Rain' -and -not $taskData.precipitationEnd) -or ($Weather -eq 'Clear' -and $taskData.precipitationEnd)) { throw 'Controlled weather did not hold.' }
    if (($Speed -eq 0 -and $taskData.captureEndStep -ne $taskData.captureStartStep) -or ($Speed -gt 0 -and $DurationSeconds * $Speed -ge 2.4 -and $taskData.captureEndStep -le $taskData.captureStartStep)) { throw 'Simulation clock did not match requested speed.' }
    if ($DurationSeconds * $Speed -ge 60 -and $taskData.monthlyTickSamples -lt 1) { throw 'No monthly settlement was captured.' }
}
if ($Populated -and ($taskData.citizensStart -le 0 -or -not $taskData.generatedPopulated -or $taskData.simulationSpeed -ne $Speed)) { throw 'Populated fixture did not initialize real residents and the requested clock.' }
if ([bool]$taskData.uncachedSpatial -ne [bool]$UncachedSpatial) { throw 'Requested spatial-cache comparison was not active.' }
if ([bool]$taskData.uncachedRouting -ne [bool]$UncachedRouting) { throw 'Requested routing-cache comparison was not active.' }
if ([bool]$taskData.referenceServices -ne [bool]$ReferenceServices) { throw 'Requested service-query comparison was not active.' }
if ([bool]$taskData.referencePopulation -ne [bool]$ReferencePopulation) { throw 'Requested population-query comparison was not active.' }
if ($Populated -and $Speed -gt 0) {
    if ([bool]$taskData.referenceCare -ne [bool]$ReferenceCare) { throw 'Requested hospital-care comparison was not active.' }
    if ($taskData.authoritativeStateSha1 -notmatch '^[A-Fa-f0-9]{40}$') { throw 'Full authoritative endpoint fingerprint missing.' }
    $taskReferenceReuse = [bool]$ReferenceServiceReuse -or [bool]$ReferenceServices
    if ([bool]$taskData.referenceServiceReuse -ne $taskReferenceReuse) { throw 'Requested step-service reuse comparison was not active.' }
    if ($taskReferenceReuse -and ($taskData.evaluatedInitialServices -ne $taskData.fixedStep.samples -or $taskData.reusedInitialServices -ne 0)) { throw 'Reference initial-service counters did not cover every step.' }
    if (-not $taskReferenceReuse -and ($taskData.reusedInitialServices -ne $taskData.fixedStep.samples -or $taskData.evaluatedInitialServices -ne 0)) { throw 'Reused initial-service counters did not cover every step.' }
}
if ($CaptureStartStep -and ($taskData.captureStartStep -ne $CaptureStartStep -or $taskData.requestedCaptureStartStep -ne $CaptureStartStep)) { throw 'Capture missed requested simulation-day boundary.' }
if ($LateGame -and $Speed -gt 0 -and $DurationSeconds -ge 15) {
    if ($ReferencePopulation -and ($taskData.referencePopulationForecasts -lt 1 -or $taskData.preparedPopulationForecasts -ne 0)) { throw 'Reference population counters did not prove execution.' }
    if (-not $ReferencePopulation -and ($taskData.preparedPopulationForecasts -lt 1 -or $taskData.referencePopulationForecasts -ne 0)) { throw 'Prepared population counters did not prove execution.' }
    if ($UncachedRouting -and ($taskData.referenceRouteBuilds -lt 1 -or $taskData.preparedRouteSearches -ne 0 -or $taskData.indexedRouteSegments -ne 0)) { throw 'Uncached routing counters did not prove reference execution.' }
    if (-not $UncachedRouting -and ($taskData.preparedRouteSearches -lt 1 -or $taskData.referenceRouteBuilds -ne 0 -or $taskData.indexedRouteSegments -lt 1)) { throw 'Prepared routing counters did not prove graph and segment reuse.' }
    if ($ReferenceServices -and ($taskData.referenceWaterQueries -lt 1 -or $taskData.preparedWaterQueries -ne 0 -or $taskData.referenceEducationCitizens -lt 1 -or $taskData.preparedEducationQueries -ne 0)) { throw 'Reference service-query counters did not prove reference execution.' }
    if (-not $ReferenceServices -and ($taskData.preparedWaterQueries -lt 1 -or $taskData.referenceWaterQueries -ne 0 -or $taskData.preparedEducationQueries -lt 1 -or $taskData.referenceEducationCitizens -ne 0)) { throw 'Prepared service-query counters did not prove current evaluation reuse.' }
}
if ($taskData.citizenRouteBudget -ne $CitizenRoutes -or $taskData.visibleTrafficLimit -ne $VisibleTraffic -or $taskData.'r.ScreenPercentage' -ne $SceneScale -or $taskData.'r.MSAACount' -ne $MSAASamples) { throw 'Capacity/quality experiment flags were not active.' }
if ($taskData.windowsNeutralColor -eq [bool]$LegacyColor) { throw 'Requested color profile was not active in the rendered game.' }
if (($LegacyShadows -and $taskData.sunShadowDistanceCm -ne 40000) -or (-not $LegacyShadows -and $taskData.sunShadowDistanceCm -ne 2000)) { throw 'Requested shadow coverage was not active in the rendered game.' }
if (($LegacyShadows -and $taskData.sunCascadeDistributionExponent -ne 3) -or (-not $LegacyShadows -and $taskData.sunCascadeDistributionExponent -ne 1)) { throw 'Requested shadow distribution was not active in the rendered game.' }
if ($CSMResolution -and $taskData.'r.Shadow.MaxCSMResolution' -ne $CSMResolution) { throw 'Requested CSM resolution comparison was not active.' }
if (-not $LegacyShadows -and ([Math]::Abs($taskData.sunShadowBias - .35) -gt .0001 -or [Math]::Abs($taskData.sunShadowSlopeBias - .5) -gt .0001 -or [Math]::Abs($taskData.sunShadowSharpen - .5) -gt .0001)) { throw 'Requested miniature contact/filter profile was not active.' }
if (-not $LegacyShadows -and -not $CSMResolution -and $taskData.'r.Shadow.MaxCSMResolution' -lt 4096) { throw 'Default sharper CSM resolution was not active.' }
if ($Capture) {
    $taskBytes = [IO.File]::ReadAllBytes((Join-Path $taskOutput 'Scene.png'))
    $taskImageWidth = $taskBytes[16]*16777216 + $taskBytes[17]*65536 + $taskBytes[18]*256 + $taskBytes[19]
    $taskImageHeight = $taskBytes[20]*16777216 + $taskBytes[21]*65536 + $taskBytes[22]*256 + $taskBytes[23]
    if ($taskImageWidth -ne $Width -or $taskImageHeight -ne $Height) { throw 'Unexpected screenshot dimensions.' }
}
# Screenshot readback can cause a hitch; leave Capture off for frame-time baselines.
$taskScope = if ($taskSource) { 'Offscreen D3D11 isolated populated save' } elseif ($Populated) { 'Offscreen D3D11 generated populated connected neighbourhood' } else { 'Offscreen D3D11 synthetic paused stress' }
$taskMetadata = [ordered]@{ startedUtc=$taskStartedUtc;populated=[bool]$Populated;citizenRouteBudget=$CitizenRoutes;visibleTrafficLimit=$VisibleTraffic;sceneScale=$SceneScale;msaaSamples=$MSAASamples;dpiSceneScaling=[bool]$DpiSceneScaling; executableSha256=(Get-FileHash -LiteralPath $taskExe).Hash; sourceSave=$taskSource; sourceSha256=$taskSourceHash; speed=$Speed; weather=$Weather; lotsRequested=$(if ($taskSource) { $null } else { $Lots }); hud=$Hud; lighting=$Lighting; legacyColor=[bool]$LegacyColor; legacyShadows=[bool]$LegacyShadows; csmResolutionOverride=$CSMResolution; screenshotDuringCapture=[bool]$Capture; audioDisabled=$true; rendering=$taskScope; primitiveCounterLimitation='Raw RHI primitives omit instance multiplicity and indirect-draw geometry in the installed D3D11 implementation; not total triangles'; warmupSeconds=$WarmupSeconds; durationSeconds=$DurationSeconds; arguments=$taskArgs }
$taskMetadata['lateGame'] = [bool]$LateGame
$taskMetadata['expectedGridSize'] = $ExpectedGridSize
$taskMetadata['startupTimeoutSeconds'] = $StartupTimeoutSeconds
$taskMetadata['uncachedSpatial'] = [bool]$UncachedSpatial
$taskMetadata['uncachedRouting'] = [bool]$UncachedRouting
$taskMetadata['referenceServices'] = [bool]$ReferenceServices
$taskMetadata['referencePopulation'] = [bool]$ReferencePopulation
$taskMetadata['referenceServiceReuse'] = [bool]$ReferenceServiceReuse
$taskMetadata['referenceCare'] = [bool]$ReferenceCare
$taskMetadata['legacyCitizens'] = [bool]$LegacyCitizens
$taskMetadata['requestedCaptureStartStep'] = $CaptureStartStep
$taskMetadata | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $taskOutput 'Metadata.json')
Write-Output "Report: $taskReport"
$taskData | Select-Object scene,frameMeanMs,frameP95Ms,frameP99Ms,gameMeanMs,renderMeanMs,gpuMeanMs,simulationTickMeanMs,simulationTickPeakMs,monthlyTickSamples,monthlyTickPeakMs,hudPresentationMeanMs,memoryMiB | Format-List
