[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskProjectPath = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskScriptPath = Join-Path $PSScriptRoot 'Create-CitySandboxMap.py'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
& $taskEditor $taskProjectPath "-ExecutePythonScript=$taskScriptPath" -unattended -nop4 -nosplash -stdout
if ($LASTEXITCODE -ne 0) { throw "Sandbox map generation failed with exit code $LASTEXITCODE" }
