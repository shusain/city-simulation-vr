[CmdletBinding()]
param([ValidateRange(1024,7680)][int]$Width = 1280, [ValidateRange(720,4320)][int]$Height = 800, [switch]$Windowed, [switch]$LegacyColor, [switch]$LegacyShadows, [switch]$PrintOnly,
    [string]$ExecutablePath)
# Compatibility entry point; use Tools/Launch/Start-DesktopSandbox.ps1.
& (Join-Path $PSScriptRoot 'Launch/Start-DesktopSandbox.ps1') @PSBoundParameters
