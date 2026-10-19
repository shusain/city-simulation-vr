[CmdletBinding()]
param([string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe')
$ErrorActionPreference = 'Stop'
$taskScript = Join-Path $PSScriptRoot 'Create-CityArtKit.py'
& $Blender --background --factory-startup --python $taskScript
if ($LASTEXITCODE -ne 0) { throw "Blender kit generation failed with exit code $LASTEXITCODE" }
