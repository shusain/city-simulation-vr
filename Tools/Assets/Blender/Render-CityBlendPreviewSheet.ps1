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
$ErrorActionPreference = 'Stop'
$taskTools = Get-Item -LiteralPath $PSScriptRoot
while ($null -ne $taskTools -and -not (Test-Path -LiteralPath (Join-Path $taskTools.FullName 'Common/CityTools.psm1'))) { $taskTools = $taskTools.Parent }
if ($null -eq $taskTools) { throw 'Cannot locate shared CityTools module' }
Import-Module (Join-Path $taskTools.FullName 'Common/CityTools.psm1') -Force
$taskRoot = Get-CityRepositoryRoot -StartPath $PSCommandPath
$taskBlend = (Resolve-Path -LiteralPath $BlendFile).Path
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw "Blender not found: $Blender" }
if (-not $ListObjects -and -not (Test-Path -LiteralPath $Python -PathType Leaf)) { throw "Python/Pillow runtime not found; supply -Python: $Python" }
if (-not $OutputDirectory) {
    $taskLabel = [IO.Path]::GetFileNameWithoutExtension($taskBlend) + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
    $OutputDirectory = Join-Path $taskRoot ('Artifacts/ArtPreview/Sheets/' + $taskLabel)
}
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $taskOutput) {
    if (-not (Test-Path -LiteralPath $taskOutput -PathType Container) -or @(Get-ChildItem -LiteralPath $taskOutput -Force).Count) {
        throw 'Use a fresh preview output directory; preserve existing evidence'
    }
}
$taskSourceHash = (Get-FileHash -LiteralPath $taskBlend -Algorithm SHA256).Hash
$taskRenderArgs = @('--background','--factory-startup','--disable-autoexec','--threads','2','--python-exit-code','1',
    '--python',(Join-Path $PSScriptRoot 'Render-CityBlendPreviews.py'),'--','--blend',$taskBlend,'--output',$taskOutput,
    '--resolution',"$Resolution",'--engine',$Engine,'--color',$Color,'--views') + $Views
foreach ($taskPattern in $ObjectPattern) { $taskRenderArgs += @('--objects',$taskPattern) }
if ($Scene) { $taskRenderArgs += @('--scene',$Scene) }
if ($null -ne $Frame) { $taskRenderArgs += @('--frame',"$Frame") }
if ($IncludeHidden) { $taskRenderArgs += '--include-hidden' }
if ($ListObjects) { $taskRenderArgs += '--list' }
$taskRender = Invoke-CityToolProcess -FilePath $Blender -ArgumentList $taskRenderArgs -LogPrefix (Join-Path $taskOutput 'Blender') -TimeoutSeconds $TimeoutSeconds
if ((Get-FileHash -LiteralPath $taskBlend -Algorithm SHA256).Hash -ne $taskSourceHash) { throw 'Source blend changed during previews' }
if ($ListObjects) {
    Get-Content -LiteralPath $taskRender.ConsoleLog
    return
}
$taskManifest = Join-Path $taskOutput 'PreviewManifest.json'
$taskManifestResult = Read-CityToolReport -Path $taskManifest -StartedUtc $taskRender.StartedUtc
if ($taskManifestResult.schema_version -ne 1 -or $taskManifestResult.source_unchanged -ne $true -or -not @($taskManifestResult.views).Count -or
    [IO.Path]::GetFullPath($taskManifestResult.source) -ne $taskBlend -or $taskManifestResult.source_sha256 -ne $taskSourceHash) { throw 'Invalid Blender preview manifest or source identity' }
$taskSheetArgs = @((Join-Path $taskTools.FullName 'Assets/Preview/CityPreviewSheets.py'),'--manifest',$taskManifest,
    '--columns',"$Columns",'--rows',"$Rows",'--tile-size',"$TileSize")
if ($Title) { $taskSheetArgs += @('--title',$Title) }
$taskCompose = Invoke-CityToolProcess -FilePath $Python -ArgumentList $taskSheetArgs -LogPrefix (Join-Path $taskOutput 'Compose') -TimeoutSeconds $TimeoutSeconds
$taskSheets = Read-CityToolReport -Path (Join-Path $taskOutput 'PreviewSheets.json') -StartedUtc $taskCompose.StartedUtc
if ($taskSheets.schema_version -ne 1 -or $taskSheets.render_count -ne @($taskManifestResult.views).Count -or -not @($taskSheets.pages).Count) { throw 'Incomplete preview sheet report' }
Write-Output (Join-Path $taskOutput 'PreviewSheet-01.png')
Write-Output (Join-Path $taskOutput 'PreviewSheets.json')
