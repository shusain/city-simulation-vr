[CmdletBinding()]
param(
    [ValidateSet('Desktop', 'PCVR')][string]$Mode = 'Desktop',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$Game,
    [switch]$LegacyColor,
    [switch]$LegacyShadows,
    [switch]$PrintOnly
)
# Compatibility entry point; use Tools/Launch/Start-CitySandboxEditor.ps1.
& (Join-Path $PSScriptRoot 'Launch/Start-CitySandboxEditor.ps1') @PSBoundParameters
