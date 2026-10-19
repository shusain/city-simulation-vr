[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7', [string]$ArchiveDirectory,
    [string]$AndroidSdkPath = (Join-Path $env:LOCALAPPDATA 'Android/Sdk'), [string]$CookOutputDirectory, [switch]$SkipEditorCompilation, [switch]$NoTraceServer)
# Compatibility entry point; use Tools/Build/Build-QuestSandbox.ps1.
& (Join-Path $PSScriptRoot 'Build/Build-QuestSandbox.ps1') @PSBoundParameters
