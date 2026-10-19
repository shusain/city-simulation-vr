[CmdletBinding()]
param([string]$Serial, [string]$ApkPath)
# Compatibility entry point; use Tools/Distribution/Deploy-QuestSandbox.ps1.
& (Join-Path $PSScriptRoot 'Distribution/Deploy-QuestSandbox.ps1') @PSBoundParameters
