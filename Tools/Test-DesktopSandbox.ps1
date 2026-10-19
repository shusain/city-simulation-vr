[CmdletBinding()]
param([ValidateRange(1024,7680)][int]$Width = 1280, [ValidateRange(720,4320)][int]$Height = 800,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$')][string]$RunName, [string]$CityPath, [switch]$InspectorOnly, [switch]$PaletteOnly,
    [switch]$DimensionsOnly, [switch]$MusicOnly, [switch]$MenusOnly, [string]$ExecutablePath, [switch]$NoTraceServer)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskExecutable = Join-Path $taskRoot 'Artifacts\PCVRSandbox\Windows\CitySimVR\Binaries\Win64\CitySimVR.exe'
if ($ExecutablePath) { $taskExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path }
$taskEvidence = if ($RunName) { Join-Path $taskRoot "Artifacts/$RunName" } else { Join-Path $taskRoot 'Artifacts' }
$taskUser = Join-Path $taskEvidence "DesktopSmoke-Automation-$Width-$Height"
$taskLog = Join-Path $taskEvidence "DesktopSmoke-Packaged-$Width-$Height.log"
if (-not (Test-Path -LiteralPath $taskExecutable -PathType Leaf)) { throw 'Build the Windows package first.' }
New-Item -ItemType Directory -Path $taskUser -Force | Out-Null
$taskArgs = @('-CityDesktop', '-nohmd', '-d3d11', '-RenderOffscreen', '-ForceRes', '-windowed', "-ResX=$Width", "-ResY=$Height", "-UserDir=`"$taskUser`"", "-abslog=`"$taskLog`"", '-ExecCmds="Automation RunTests CitySim.Desktop.PackagedSmoke"', '-TestExit="Automation Test Queue Empty"', '-unattended', '-stdout')
if ($NoTraceServer) { $taskArgs += '-notraceserver' }
if (([int]$InspectorOnly.IsPresent + [int]$PaletteOnly.IsPresent + [int]$DimensionsOnly.IsPresent + [int]$MusicOnly.IsPresent + [int]$MenusOnly.IsPresent) -gt 1) { throw 'Choose only one focused scope.' }
if ($MenusOnly) { $taskArgs += '-CityDesktopMenusOnly' }
if ($MusicOnly) { $taskArgs += '-CityDesktopMusicOnly' }
if ($InspectorOnly) { $taskArgs += '-CityDesktopInspectorOnly' }
if ($PaletteOnly) { $taskArgs += '-CityDesktopPaletteOnly' }
if ($DimensionsOnly) {
    if (-not $CityPath) { throw 'Dimensions smoke requires the populated80 fixture.' }
    $taskArgs += '-CityDesktopDimensionsOnly'
}
if ($CityPath) {
    $taskCityPath = (Resolve-Path -LiteralPath $CityPath -ErrorAction Stop).Path
    if (-not (Test-Path -LiteralPath $taskCityPath -PathType Leaf)) { throw 'Chart QA requires a copied city JSON file.' }
    $taskArgs += "-CityUiCity=`"$taskCityPath`""
}
$taskProcess = Start-Process -FilePath $taskExecutable -ArgumentList $taskArgs -WorkingDirectory (Split-Path $taskExecutable -Parent) -WindowStyle Hidden -PassThru
if (-not $taskProcess.WaitForExit(180000)) {
    Stop-Process -Id $taskProcess.Id
    throw "Desktop smoke timeout; see $taskLog"
}
$taskText = Get-Content -LiteralPath $taskLog -Raw
if ($taskProcess.ExitCode -ne 0 -or $taskText -notmatch 'CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE' -or $taskText -notmatch 'Result=\{Success\}.*PackagedSmoke' -or $taskText -match 'LogD3D11RHI: Error|LogD3D12RHI: Error|Fatal error|Assertion failed|Test Completed\. Result=\{Fail') {
    throw "Packaged desktop validation failed (exit $($taskProcess.ExitCode)); see $taskLog"
}
$taskScreens = Join-Path $taskUser 'Saved\Automation\Desktop'
$taskExpected = if ($MenusOnly) { @('Entry.png','NewCitySetup.png','NewCity80.png','NewCityReview.png','Taxes.png','LoadPreview.png') } elseif ($MusicOnly) { @('Entry.png','Overview.png','MusicControls.png','MusicMuted.png') } elseif ($DimensionsOnly) { @('Entry.png','Overview.png','Map80Setup.png','Map80Review.png','Populated80.png','Map80Library.png','Switched64.png') } elseif ($PaletteOnly) { @('Entry.png','Overview.png','PaletteZone.png','PaletteResidential.png','PaletteCommercial.png','PaletteIndustrial.png','PaletteBuild.png','PalettePower.png','PaletteInspect.png') } elseif ($InspectorOnly) { @('Entry.png','Overview.png','InspectorLive.png','InspectorDetails.png','InspectorAndCity.png') } else { @('Entry.png','Overview.png','Build.png','CityInfo.png','ObjectivesOnly.png','StatsOnly.png','Reports.png','Settings.png','Debug.png','Taxes.png','Zone.png','ZoneCommercial.png','ZoneIndustrial.png','MainMenu.png','Speed3.png','Paused.png','Speed5.png','SelectedPower.png','Graphics.png','GraphicsPreview.png','Growth.png','ReportsMonthly.png','ReportsSelected.png','ReportsAnnual.png','Demand.png','Density.png','InspectorLive.png','InspectorDetails.png','InspectorAndCity.png') }
foreach ($taskName in $taskExpected) {
    $taskCapture = Join-Path $taskScreens $taskName
    if (-not (Test-Path -LiteralPath $taskCapture)) { throw "Missing rendered capture: $taskName" }
    $taskBytes = [IO.File]::ReadAllBytes($taskCapture)
    $taskImageWidth = $taskBytes[16]*16777216 + $taskBytes[17]*65536 + $taskBytes[18]*256 + $taskBytes[19]
    $taskImageHeight = $taskBytes[20]*16777216 + $taskBytes[21]*65536 + $taskBytes[22]*256 + $taskBytes[23]
    if ($taskImageWidth -ne $Width -or $taskImageHeight -ne $Height) { throw "Unexpected rendered dimensions ${taskImageWidth}x${taskImageHeight}: $taskName" }
}
Write-Output "Desktop packaged synthetic-button/rendered smoke passed at ${Width}x$Height; $($taskExpected.Count) required captures (InspectorOnly=$InspectorOnly PaletteOnly=$PaletteOnly DimensionsOnly=$DimensionsOnly). Physical input remains pending."
Write-Output "Log: $taskLog"
Write-Output "Captures: $taskScreens"
