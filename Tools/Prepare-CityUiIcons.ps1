[CmdletBinding()]
param([switch]$ConditionOnly)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'ThirdParty\MaterialSymbols\svg'
$outputRoot = Join-Path $repositoryRoot 'ThirdParty\MaterialSymbols\Derived'
$browser = 'C:\Program Files\Google\Chrome\Application\chrome.exe'

if (-not (Test-Path -LiteralPath $browser)) {
    throw "Chrome is required to rasterize the vendored SVG icons: $browser"
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'city-sim-ui-icons'
New-Item -ItemType Directory -Force -Path $temporaryRoot | Out-Null

$taskNames = if ($ConditionOnly) { @('build', 'construction', 'local_police') } else { @('download', 'upload', 'play_arrow', 'restart_alt', 'arrow_back', 'close', 'settings', 'bar_chart', 'build', 'construction', 'local_police') }
foreach ($name in $taskNames) {
    $sourcePath = Join-Path $sourceRoot "$name.svg"
    $renderPath = Join-Path $temporaryRoot "$name.svg"
    $outputPath = Join-Path $outputRoot "$name.png"
    $svg = Get-Content -LiteralPath $sourcePath -Raw
    $svg = $svg.Replace('height="48"', 'height="256"').Replace('width="48"', 'width="256"')
    $svg = $svg.Replace('<path ', '<path fill="#ffffff" ')
    [System.IO.File]::WriteAllText($renderPath, $svg, [System.Text.UTF8Encoding]::new($false))
    $uri = [System.Uri]::new($renderPath).AbsoluteUri
    & $browser --headless --disable-gpu --hide-scrollbars --default-background-color=00000000 `
        --window-size=256,256 --screenshot=$outputPath $uri | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputPath)) {
        throw "Failed to rasterize $name"
    }
}

Write-Output "Prepared $($taskNames.Count) 256 px Material Symbols icons in $outputRoot"
