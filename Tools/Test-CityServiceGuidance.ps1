[CmdletBinding()]
param([string]$ExecutablePath, [int]$Width = 1280, [int]$Height = 800,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunName = 'Render-1280')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not $ExecutablePath) { $ExecutablePath = Join-Path $taskRoot 'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe' }
$taskExe = (Resolve-Path -LiteralPath $ExecutablePath).Path
$taskEvidence = Join-Path $taskRoot "Artifacts/ServiceGuidance/$RunName"
$taskLog = Join-Path $taskEvidence 'Run.log'
if (Test-Path -LiteralPath $taskLog) { throw 'Choose a fresh RunName to preserve earlier evidence.' }
$taskUser = Join-Path $taskEvidence 'ServiceGuidance-User'
New-Item -ItemType Directory -Path $taskUser -Force | Out-Null
$taskArgs = @('-CityDesktop', '-nohmd', '-d3d11', '-RenderOffscreen', '-ForceRes', '-windowed',
    "-ResX=$Width", "-ResY=$Height", "-UserDir=`"$taskUser`"", "-abslog=`"$taskLog`"",
    '-ExecCmds="Automation RunTests CitySim.Desktop.ServiceGuidanceRender"', '-TestExit="Automation Test Queue Empty"', '-unattended', '-stdout')
$taskProcess = Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WorkingDirectory (Split-Path $taskExe -Parent) -WindowStyle Hidden -PassThru
if (-not $taskProcess.WaitForExit(180000)) { Stop-Process -Id $taskProcess.Id; throw "Service guidance render timeout: $taskLog" }
$taskText = Get-Content -LiteralPath $taskLog -Raw
if ($taskProcess.ExitCode -ne 0 -or $taskText -notmatch 'CITYSIM_SERVICE_GUIDANCE_RENDER_COMPLETE' -or
    $taskText -notmatch 'Test Completed. Result=\{Success\}.*ServiceGuidanceRender' -or
    $taskText -match 'Result=\{Fail\}|Fatal error|Assertion failed|LogD3D11RHI: Error') { throw "Service guidance render failed: $taskLog" }
$taskCaptures = foreach ($taskName in @('HomeOverview.png', 'HomeNeeds.png', 'SchoolReach.png', 'WaterCapacity.png', 'WaterShortage.png', 'RepairToolbar.png', 'UtilityQuantities.png', 'HomeTrends.png')) {
    $taskPath = Join-Path $taskUser "Saved/Automation/ServiceGuidance/$taskName"
    $taskBytes = [IO.File]::ReadAllBytes($taskPath)
    $taskActualWidth = $taskBytes[16]*16777216 + $taskBytes[17]*65536 + $taskBytes[18]*256 + $taskBytes[19]
    $taskActualHeight = $taskBytes[20]*16777216 + $taskBytes[21]*65536 + $taskBytes[22]*256 + $taskBytes[23]
    if ($taskActualWidth -ne $Width -or $taskActualHeight -ne $Height) { throw "Wrong capture dimensions: $taskName" }
    [ordered]@{ path=$taskPath; width=$Width; height=$Height; sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash }
}
[ordered]@{ executable=$taskExe; executableSha256=(Get-FileHash -LiteralPath $taskExe -Algorithm SHA256).Hash;
    test='CitySim.Desktop.ServiceGuidanceRender'; result='Success'; physicalInputObserved=$false; captures=@($taskCaptures) } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskEvidence 'Verification.json')
Write-Output "Service guidance render passed: eight ${Width}x$Height captures; $taskEvidence"
