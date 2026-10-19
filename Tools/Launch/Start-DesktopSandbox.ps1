[CmdletBinding()]
param([ValidateRange(1024,7680)][int]$Width = 1280, [ValidateRange(720,4320)][int]$Height = 800, [switch]$Windowed, [switch]$LegacyColor, [switch]$LegacyShadows, [switch]$PrintOnly,
    [string]$ExecutablePath)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
$taskExecutable = Join-Path $taskRoot 'Artifacts\PCVRSandbox\Windows\CitySimVR\Binaries\Win64\CitySimVR.exe'
if ($ExecutablePath) { $taskExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path }
$taskUserDirectory = Join-Path $taskRoot 'Artifacts\DesktopUser'
$taskLogDirectory = Join-Path $taskRoot 'Artifacts\DesktopLogs'
if (-not (Test-Path -LiteralPath $taskExecutable -PathType Leaf)) { throw 'Build the Windows package with Tools/Build-PCVRSandbox.ps1 first.' }
$taskLog = Join-Path $taskLogDirectory ("Play-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskArguments = @('-CityDesktop', '-nohmd', '-d3d11', "-UserDir=`"$taskUserDirectory`"", "-abslog=`"$taskLog`"")
if ($Windowed -or $PSBoundParameters.ContainsKey('Width') -or $PSBoundParameters.ContainsKey('Height')) {
    $taskArguments += @('-windowed', "-ResX=$Width", "-ResY=$Height")
}
if ($LegacyColor) { $taskArguments += '-CityLegacyWindowsColor' }
if ($LegacyShadows) { $taskArguments += '-CityLegacyWindowsShadows' }
Write-Output "Desktop save root: $taskUserDirectory\Saved\SaveGames\Cities"
Write-Output "Launch: $taskExecutable $($taskArguments -join ' ')"
if ($PrintOnly) { return }
New-Item -ItemType Directory -Path $taskUserDirectory, $taskLogDirectory -Force | Out-Null
# This is the interactive game window requested by the desktop launch profile.
$taskProcess = Start-Process -FilePath $taskExecutable -ArgumentList $taskArguments -WorkingDirectory (Split-Path $taskExecutable -Parent) -WindowStyle Normal -PassThru
Write-Output "Started desktop process $($taskProcess.Id). Log: $taskLog"
