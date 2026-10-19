[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Build-CitySimVR.ps1') -Preset Smoke -EngineRoot $EngineRoot
