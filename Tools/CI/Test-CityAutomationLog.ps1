[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][string[]]$ExpectedTests,
    [Parameter(Mandatory)][int]$ProcessExitCode
)
$ErrorActionPreference = 'Stop'
# Retained entry point for CI and historical command references.
& (Join-Path $PSScriptRoot '../Validation/Test-CityAutomationLog.ps1') @PSBoundParameters
