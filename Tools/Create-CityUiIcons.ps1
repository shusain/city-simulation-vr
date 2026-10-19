[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Prepare-CityUiIcons.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Icon rasterization failed.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repositoryRoot 'CitySimVR\CitySimVR.uproject'
$script = Join-Path $PSScriptRoot 'Create-CityUiIcons.py'
$editor = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
& $editor $project "-ExecutePythonScript=$script" -unattended -nop4 -nosplash -stdout
if ($LASTEXITCODE -ne 0) {
    throw "Unreal icon import failed with exit code $LASTEXITCODE."
}
