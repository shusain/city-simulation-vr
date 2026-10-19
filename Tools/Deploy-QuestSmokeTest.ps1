[CmdletBinding()]
param([string]$Serial, [string]$ApkPath)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Deploy-CitySimVR.ps1') -Preset Smoke -Serial $Serial -ApkPath $ApkPath
