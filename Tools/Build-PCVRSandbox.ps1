[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [ValidateSet(64,80,96,112,128,160,192)][int]$CapacityGridSize = 64,
	[switch]$SkipEditorCompilation,
	[string]$ArchiveDirectory,
    [switch]$NoTraceServer
)
# Compatibility entry point; use Tools/Build/Build-PCVRSandbox.ps1.
& (Join-Path $PSScriptRoot 'Build/Build-PCVRSandbox.ps1') @PSBoundParameters
