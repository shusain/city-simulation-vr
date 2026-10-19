[CmdletBinding()]
param(
    [ValidateSet('Smoke', 'Sandbox')]
    [string]$Preset = 'Sandbox',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [string]$ArchiveDirectory,
    [string]$AndroidSdkPath = (Join-Path $env:LOCALAPPDATA 'Android/Sdk'),
    [string]$CookOutputDirectory,
    [switch]$SkipEditorCompilation,
    [switch]$NoTraceServer
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path $PSScriptRoot -Parent) 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot $PSScriptRoot
$taskToolsRoot = Join-Path $taskRoot 'Tools'

$ErrorActionPreference = 'Stop'
. (Join-Path $taskToolsRoot 'Set-UnrealAndroidEnvironment.ps1') -SdkPath $AndroidSdkPath
& (Join-Path $taskToolsRoot 'Patch-MetaXR203.ps1')

$taskProjectPath = Join-Path $taskRoot 'CitySimVR\CitySimVR.uproject'
$taskEngineConfigPath = Join-Path $taskRoot 'CitySimVR\Config\DefaultEngine.ini'
$taskGameConfigPath = Join-Path $taskRoot 'CitySimVR\Config\DefaultGame.ini'
$taskUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

$taskPresetConfig = @{
    Smoke = @{
        Map = '/Game/VRTemplate/VRTemplateMap'
        Archive = 'Artifacts\QuestSmokeTest'
    }
    Sandbox = @{
        Map = '/Game/CitySim/Maps/CitySandbox'
        Archive = 'Artifacts\QuestSandbox'
    }
}[$Preset]

$taskMap = $taskPresetConfig.Map
$taskMapObject = "$taskMap.$(($taskMap -split '/')[-1])"
$taskArchivePath = Join-Path $taskRoot $taskPresetConfig.Archive
if ($ArchiveDirectory) { $taskArchivePath = [IO.Path]::GetFullPath($ArchiveDirectory) }
$taskOriginalEngineConfig = [IO.File]::ReadAllText($taskEngineConfigPath)
$taskOriginalGameConfig = [IO.File]::ReadAllText($taskGameConfigPath)
$taskUtf8NoBom = [Text.UTF8Encoding]::new($false)

function Set-SingleConfigValue([string]$Content, [string]$Key, [string]$Value) {
    $taskPattern = "(?m)^$([Regex]::Escape($Key))=.*$"
    if ([Regex]::Matches($Content, $taskPattern).Count -ne 1) {
        throw "Expected exactly one $Key entry in build configuration."
    }
    return [Regex]::Replace($Content, $taskPattern, "$Key=$Value")
}

function Write-BuildConfigIfChanged([string]$ConfigPath, [string]$Content) {
    # A reader can memory-map the ini while packaging. Avoid truncating an
    # unchanged file, especially during finally when it already has its original text.
    if ([string]::Equals([IO.File]::ReadAllText($ConfigPath), $Content, [StringComparison]::Ordinal)) { return }
    [IO.File]::WriteAllText($ConfigPath, $Content, $taskUtf8NoBom)
}

try {
    $taskBuildEngineConfig = Set-SingleConfigValue $taskOriginalEngineConfig 'EditorStartupMap' $taskMapObject
    $taskBuildEngineConfig = Set-SingleConfigValue $taskBuildEngineConfig 'GameDefaultMap' $taskMapObject
    $taskBuildGameConfig = Set-SingleConfigValue $taskOriginalGameConfig '+MapsToCook' "(FilePath=`"$taskMap`")"
    Write-BuildConfigIfChanged $taskEngineConfigPath $taskBuildEngineConfig
    Write-BuildConfigIfChanged $taskGameConfigPath $taskBuildGameConfig

    Write-Output "Building preset '$Preset': cook and startup map $taskMap"
    # CI has already built and tested this exact Editor; native game/cook still run.
    $taskEditorArgs = @()
    if ($SkipEditorCompilation) { $taskEditorArgs += '-nocompileeditor' }
    $taskCookArgs = @()
    if ($CookOutputDirectory) { $taskCookArgs += "-CookOutputDir=$([IO.Path]::GetFullPath($CookOutputDirectory))" }
    $taskTraceArgs = @()
    if ($NoTraceServer) { $taskTraceArgs += '-AdditionalCookerOptions=-notraceserver' }
    & $taskUat BuildCookRun "-project=$taskProjectPath" @taskEditorArgs @taskCookArgs @taskTraceArgs -noP4 -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive "-archivedirectory=$taskArchivePath" "-map=$taskMap" -utf8output -unattended
    if ($LASTEXITCODE -ne 0) { throw "Unreal packaging failed with exit code $LASTEXITCODE" }
}
finally {
    Write-BuildConfigIfChanged $taskEngineConfigPath $taskOriginalEngineConfig
    Write-BuildConfigIfChanged $taskGameConfigPath $taskOriginalGameConfig
}
