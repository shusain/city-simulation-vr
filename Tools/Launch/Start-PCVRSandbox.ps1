[CmdletBinding()]
param(
    [ValidateSet('D3D12', 'D3D11')]
    [string]$GraphicsApi = 'D3D11',
    [string]$RuntimeManifest,
    [switch]$LegacyColor,
    [switch]$LegacyShadows,
    [switch]$PreviousVRShadows,
    [switch]$PrintOnly
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
$taskExecutable = Join-Path $taskRoot 'Artifacts\PCVRSandbox\Windows\CitySimVR\Binaries\Win64\CitySimVR.exe'
$taskUserDirectory = Join-Path $taskRoot 'Artifacts\PCVRUser'
$taskLogDirectory = Join-Path $taskRoot 'Artifacts\PCVRLogs'
if (-not (Test-Path -LiteralPath $taskExecutable -PathType Leaf)) { throw 'Build the Windows package with Tools/Build-PCVRSandbox.ps1 first.' }
if (-not $RuntimeManifest) {
    $RuntimeManifest = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -Name ActiveRuntime).ActiveRuntime
}
$taskRuntimePath = (Resolve-Path -LiteralPath $RuntimeManifest).Path
$taskRuntime = Get-Content -LiteralPath $taskRuntimePath -Raw | ConvertFrom-Json
if (-not $taskRuntime.runtime.library_path) { throw "Invalid OpenXR runtime manifest: $taskRuntimePath" }
$taskRuntimeLibrary = $taskRuntime.runtime.library_path
if (-not [IO.Path]::IsPathRooted($taskRuntimeLibrary)) {
    $taskRuntimeLibrary = Join-Path (Split-Path $taskRuntimePath -Parent) $taskRuntimeLibrary
}
if (-not (Test-Path -LiteralPath $taskRuntimeLibrary -PathType Leaf)) { throw "OpenXR runtime library missing: $taskRuntimeLibrary" }
$taskLog = Join-Path $taskLogDirectory ("Play-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskArguments = @('-vr', '-nohighdpi', ('-' + $GraphicsApi.ToLowerInvariant()), "-UserDir=`"$taskUserDirectory`"", "-abslog=`"$taskLog`"")
if ($LegacyColor) { $taskArguments += '-CityLegacyWindowsColor' }
if ($LegacyShadows) { $taskArguments += '-CityLegacyWindowsShadows' }
if ($PreviousVRShadows) { $taskArguments += '-CityPreviousVRShadows' }
Write-Output "OpenXR runtime: $taskRuntimePath"
Write-Output "PCVR save root: $taskUserDirectory\Saved\SaveGames\Cities"
Write-Output "Launch: $taskExecutable $($taskArguments -join ' ')"
if ($PrintOnly) { return }
New-Item -ItemType Directory -Path $taskUserDirectory, $taskLogDirectory -Force | Out-Null
$taskPreviousRuntime = [Environment]::GetEnvironmentVariable('XR_RUNTIME_JSON', 'Process')
try {
    [Environment]::SetEnvironmentVariable('XR_RUNTIME_JSON', $taskRuntimePath, 'Process')
    $taskProcess = Start-Process -FilePath $taskExecutable -ArgumentList $taskArguments -WorkingDirectory (Split-Path $taskExecutable -Parent) -WindowStyle Hidden -PassThru
    Write-Output "Started PCVR process $($taskProcess.Id). Log: $taskLog"
}
finally {
    [Environment]::SetEnvironmentVariable('XR_RUNTIME_JSON', $taskPreviousRuntime, 'Process')
}
