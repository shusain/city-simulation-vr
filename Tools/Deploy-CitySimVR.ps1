[CmdletBinding()]
param(
    [ValidateSet('Smoke', 'Sandbox')]
    [string]$Preset = 'Sandbox',
    [string]$Serial,
    [string]$ApkPath
)
# Compatibility entry point; use Tools/Distribution/Deploy-CitySimVR.ps1.
& (Join-Path $PSScriptRoot 'Distribution/Deploy-CitySimVR.ps1') @PSBoundParameters
