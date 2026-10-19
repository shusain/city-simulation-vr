$ErrorActionPreference = 'Stop'

$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskProjectPath = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskScriptPath = Join-Path $PSScriptRoot 'Create-CitySkyMaterial.py'

& $taskEditor $taskProjectPath "-ExecutePythonScript=$taskScriptPath" -unattended -nop4 -nosplash -stdout
if ($LASTEXITCODE -ne 0) {
    throw "City sky material generation failed with exit code $LASTEXITCODE"
}
