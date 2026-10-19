[CmdletBinding()]
param([string]$Serial, [string]$ApkPath)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
& (Join-Path $taskToolsRoot 'Deploy-CitySimVR.ps1') -Preset Sandbox -Serial $Serial -ApkPath $ApkPath
