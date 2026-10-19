[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BlendFile,
    [string]$OutputDirectory,
    [string[]]$ObjectPattern = @(),
    [ValidateSet('iso','front','back','left','right','top')][string[]]$Views = @('iso'),
    [string]$Scene,
    [Nullable[int]]$Frame,
    [ValidateRange(64,4096)][int]$Resolution = 512,
    [ValidateSet('workbench','eevee')][string]$Engine = 'workbench',
    [ValidateSet('material','vertex','object')][string]$Color = 'material',
    [switch]$IncludeHidden,
    [switch]$ListObjects,
    [ValidateRange(1,8)][int]$Columns = 3,
    [ValidateRange(1,8)][int]$Rows = 4,
    [ValidateRange(64,1024)][int]$TileSize = 384,
    [string]$Title,
    [string]$Blender = 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe',
    [ValidateRange(1,3600)][int]$TimeoutSeconds = 900,
    [string]$Python = (Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
)
& (Join-Path $PSScriptRoot 'Assets/Blender/Render-CityBlendPreviewSheet.ps1') @PSBoundParameters
