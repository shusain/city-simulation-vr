[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7', [string]$ArchiveDirectory,
    [string]$AndroidSdkPath = (Join-Path $env:LOCALAPPDATA 'Android/Sdk'), [string]$CookOutputDirectory, [switch]$SkipEditorCompilation, [switch]$NoTraceServer)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
& (Join-Path $taskToolsRoot 'Build-CitySimVR.ps1') -Preset Sandbox -EngineRoot $EngineRoot -ArchiveDirectory $ArchiveDirectory -AndroidSdkPath $AndroidSdkPath -CookOutputDirectory $CookOutputDirectory -SkipEditorCompilation:$SkipEditorCompilation -NoTraceServer:$NoTraceServer
