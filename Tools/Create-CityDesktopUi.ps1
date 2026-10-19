[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7', [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskArgs = @((Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'), '-run=CityDesktopUiAssets', '-unattended', '-nop4', '-nosplash', '-stdout', '-nohmd')
if ($Rebuild) { $taskArgs += '-Rebuild' }
& $taskEditor @taskArgs
if ($LASTEXITCODE -ne 0) { throw "Desktop UI asset commandlet failed: $LASTEXITCODE" }
