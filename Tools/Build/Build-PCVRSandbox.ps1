[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [ValidateSet(64,80,96,112,128,160,192)][int]$CapacityGridSize = 64,
	[switch]$SkipEditorCompilation,
	[string]$ArchiveDirectory,
    [switch]$NoTraceServer
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
$taskProject = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$taskArchive = Join-Path $taskRoot 'Artifacts\PCVRSandbox\Windows'
if ($CapacityGridSize -ne 64) {
    $taskArchive = Join-Path $taskRoot "Artifacts\CapacitySandbox-$CapacityGridSize"
}
if ($ArchiveDirectory) { $taskArchive = [IO.Path]::GetFullPath($ArchiveDirectory) }
$taskLogDirectory = Join-Path $taskRoot 'Artifacts\PCVRLogs'
if (-not (Test-Path -LiteralPath $taskUat -PathType Leaf)) { throw "Unreal Automation Tool missing: $taskUat" }
& (Join-Path $taskToolsRoot 'Patch-MetaXR203.ps1')
New-Item -ItemType Directory -Path $taskLogDirectory -Force | Out-Null
$taskLog = Join-Path $taskLogDirectory ("Build-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
Write-Output "Building Win64 Development sandbox to $taskArchive"
$taskPreviousGrid = $env:CITYSIM_CAPACITY_GRID_SIZE
try {
    $env:CITYSIM_CAPACITY_GRID_SIZE = "$CapacityGridSize"
    # ModuleRules reads the grid override; cached UBT makefiles otherwise skip it.
    # Repeated capacity packages can reuse a separately verified, current Editor.
    # Default release/development calls still compile it; game code always builds.
    $taskEditorArgs = @()
    if ($SkipEditorCompilation) { $taskEditorArgs += '-nocompileeditor' }
    $taskTraceArgs = @()
    if ($NoTraceServer) { $taskTraceArgs += '-AdditionalCookerOptions=-notraceserver' }
    & $taskUat BuildCookRun "-project=$taskProject" '-UbtArgs=-NoUBTMakefiles' @taskEditorArgs @taskTraceArgs -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -package -prereqs -archive "-archivedirectory=$taskArchive" '-map=/Game/CitySim/Maps/CitySandbox' -utf8output -unattended 2>&1 | Tee-Object -FilePath $taskLog
    if ($LASTEXITCODE -ne 0) { throw "Win64 packaging failed with exit code $LASTEXITCODE; see $taskLog" }
} finally {
    $env:CITYSIM_CAPACITY_GRID_SIZE = $taskPreviousGrid
}
if ($CapacityGridSize -ne 64) {
    [ordered]@{ gridSize=$CapacityGridSize; benchmarkOnly=$true; citySaveWrites=$false; publishable=$false } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskArchive 'Windows/CapacityBuild.json')
}
Write-Output "PCVR package complete. Build log: $taskLog"
