[CmdletBinding()]
param(
    [switch]$Check,
    [string]$FormatterPath
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskSource = Join-Path $taskRoot 'CitySimVR/Source/CitySimVR'

if (-not $FormatterPath) {
    $taskFormatter = Get-Command clang-format -ErrorAction SilentlyContinue
    $FormatterPath = if ($taskFormatter) {
        $taskFormatter.Source
    } else {
        'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-format.exe'
    }
}
if (-not (Test-Path -LiteralPath $FormatterPath -PathType Leaf)) {
    throw 'Provide -FormatterPath for an installed clang-format (verified baseline: 19.1.5).'
}

$taskFiles = @(Get-ChildItem -LiteralPath $taskSource -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.h' } | Sort-Object FullName)
if ($taskFiles.Count -eq 0) {
    throw "No project C++ files found in $taskSource."
}
foreach ($taskFile in $taskFiles) {
    if ($Check) {
        & $FormatterPath --style=file --dry-run --Werror $taskFile.FullName
    } else {
        & $FormatterPath --style=file -i $taskFile.FullName
    }
    if ($LASTEXITCODE -ne 0) {
        throw "C++ formatting failed: $($taskFile.FullName)"
    }
}
Write-Output "C++ formatting $(if ($Check) { 'check' } else { 'update' }) passed for $($taskFiles.Count) project files."
