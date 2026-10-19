[CmdletBinding()]
param(
    [ValidateSet('D3D12', 'D3D11')]
    [string]$GraphicsApi = 'D3D11',
    [string]$RuntimeManifest,
    [switch]$LegacyColor,
    [switch]$LegacyShadows,
    [switch]$PreviousVRShadows,
    [switch]$PrintOnly
)
# Compatibility entry point; use Tools/Launch/Start-PCVRSandbox.ps1.
& (Join-Path $PSScriptRoot 'Launch/Start-PCVRSandbox.ps1') @PSBoundParameters
