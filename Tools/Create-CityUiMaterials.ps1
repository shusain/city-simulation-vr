[CmdletBinding()]
param([switch]$TextOnly)
$ErrorActionPreference = 'Stop'

$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskProjectPath = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskScriptPath = Join-Path $PSScriptRoot 'Create-CityUiMaterials.py'

$taskPriorTextOnly = $env:CITYSIM_UI_TEXT_ONLY
try {
    if ($TextOnly) { $env:CITYSIM_UI_TEXT_ONLY = '1' }
    & $taskEditor $taskProjectPath -run=pythonscript "-Script=$taskScriptPath" -unattended -nop4 -nosplash -stdout
    if ($LASTEXITCODE -ne 0) {
        throw "City UI material generation failed with exit code $LASTEXITCODE"
    }
} finally {
    $env:CITYSIM_UI_TEXT_ONLY = $taskPriorTextOnly
}
