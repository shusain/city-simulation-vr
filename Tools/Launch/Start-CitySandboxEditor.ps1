[CmdletBinding()]
param(
    [ValidateSet('Desktop', 'PCVR')][string]$Mode = 'Desktop',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$Game,
    [switch]$LegacyColor,
    [switch]$LegacyShadows,
    [switch]$PrintOnly
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$taskProject = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskUser = Join-Path $taskRoot "Artifacts\Editor${Mode}User"
if (-not (Test-Path -LiteralPath $taskEditor -PathType Leaf)) { throw "Editor missing: $taskEditor" }
$taskArgs = @("`"$taskProject`"", '/Game/CitySim/Maps/CitySandbox', '-d3d11', "-UserDir=`"$taskUser`"")
if ($Mode -eq 'Desktop') { $taskArgs += @('-CityDesktop', '-nohmd') }
if ($LegacyColor) { $taskArgs += '-CityLegacyWindowsColor' }
if ($LegacyShadows) { $taskArgs += '-CityLegacyWindowsShadows' }
if ($Game) {
    $taskArgs += '-game'
    if ($Mode -eq 'Desktop') { $taskArgs += @('-windowed', '-ResX=1280', '-ResY=800') }
    else { $taskArgs += @('-vr', '-nohighdpi') }
}
Write-Output "Editor $Mode profile: $taskEditor $($taskArgs -join ' ')"
Write-Output "Play saves: $taskUser\Saved (separate from packaged profiles)"
if ($PrintOnly) { return }
New-Item -ItemType Directory -Path $taskUser -Force | Out-Null
# These are interactive authoring/play windows explicitly selected by the user.
Start-Process -FilePath $taskEditor -ArgumentList $taskArgs -WorkingDirectory $taskRoot -WindowStyle Normal
