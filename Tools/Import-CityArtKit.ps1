[CmdletBinding()]
param(
    [switch]$DryRun,
    [switch]$ArchitectureOnly,
    [switch]$BuildingRefinementOnly,
    [switch]$RoadsOnly,
    [switch]$ConstructionOnly,
    [switch]$UtilitiesOnly,
    [switch]$CivicOnly,
    [switch]$SchoolOnly,
    [switch]$ParksOnly,
    [switch]$StationUpgradesOnly,
    [switch]$DamageOnly,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskProject = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskScript = Join-Path $PSScriptRoot 'Import-CityArtKit.py'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskReportName = if ($BuildingRefinementOnly) { if ($DryRun) { 'D79-building-refinement-dry-run.json' } else { 'D79-building-refinement-import.json' } } else { if ($DryRun) { 'city_starter_kit_dry_run.json' } else { 'city_starter_kit_import.json' } }
$taskReport = Join-Path $taskRoot ('Artifacts\AssetImport\' + $taskReportName)
if (Test-Path -LiteralPath $taskReport) { Remove-Item -LiteralPath $taskReport -Force }
$taskArgs = @($taskProject, "-ExecutePythonScript=$taskScript", '-unattended', '-nop4', '-nosplash', '-stdout')
if (([int]$BuildingRefinementOnly.IsPresent + [int]$ArchitectureOnly.IsPresent + [int]$RoadsOnly.IsPresent + [int]$ConstructionOnly.IsPresent + [int]$UtilitiesOnly.IsPresent + [int]$CivicOnly.IsPresent + [int]$DamageOnly.IsPresent + [int]$SchoolOnly.IsPresent + [int]$ParksOnly.IsPresent + [int]$StationUpgradesOnly.IsPresent) -gt 1) { throw 'Choose one focused import mode.' }
if ($BuildingRefinementOnly) { $taskArgs += '-BuildingRefinementOnly' }
if ($StationUpgradesOnly) { $taskArgs += '-StationUpgradesOnly' }
if ($ParksOnly) { $taskArgs += '-ParksOnly' }
if ($SchoolOnly) { $taskArgs += '-SchoolOnly' }
if ($DamageOnly) { $taskArgs += '-DamageOnly' }
if ($CivicOnly) { $taskArgs += '-CivicOnly' }
if ($UtilitiesOnly) { $taskArgs += '-UtilitiesOnly' }
if ($ConstructionOnly) { $taskArgs += '-ConstructionOnly' }
if ($RoadsOnly) { $taskArgs += '-RoadsOnly' }
if ($ArchitectureOnly) { $taskArgs += '-ArchitectureOnly' }
if ($DryRun) { $taskArgs += '-DryRun' }
& $taskEditor $taskArgs
if ($LASTEXITCODE -ne 0) { throw "City art import failed with exit code $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $taskReport)) { throw 'City art import did not produce its success report; inspect Unreal Python errors above.' }
