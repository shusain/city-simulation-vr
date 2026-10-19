param([switch]$BuildEditor)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D88/Scale'
$taskEngine='C:/Program Files/Epic Games/UE_5.7'
$taskOwners=[regex]::Matches((Get-Content "$taskRoot/docs/DEVELOPMENT_TASKS.md" -Raw),'\*\*Native window OWNED BY ([A-Z]+\d+)\b')
if ($taskOwners.Count -ne 1 -or $taskOwners[0].Groups[1].Value -ne 'D88') { throw 'D88 exclusive native ownership required' }
if (Get-Process UnrealEditor*,UnrealBuildTool*,AutomationTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process active' }
if ($BuildEditor) {
    & "$taskEngine/Engine/Build/BatchFiles/Build.bat" CitySimVREditor Win64 Development "-Project=$taskRoot/CitySimVR/CitySimVR.uproject" "-Log=$taskOut/UBT.log" -NoUBA -WaitMutex -WarningsAsErrors *> "$taskOut/EditorBuild.log"
    if ($LASTEXITCODE -ne 0) { throw 'Scale Editor build failed' }
}
$taskFilter='CitySim.Parks.CitizenRelativeFieldScale+CitySim.Parks.RefinedSportsContactsAndGrounding'
& "$taskEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$taskRoot/CitySimVR/CitySimVR.uproject" "-ExecCmds=Automation RunTests $taskFilter" '-TestExit=Automation Test Queue Empty' -unattended -nop4 -nosplash -nocef -nohmd -d3d11 -RenderOffscreen "-UserDir=$taskOut/NativeChecksUser" -stdout "-abslog=$taskOut/NativeChecks.log" *> "$taskOut/NativeChecks-Console.log"
if ($LASTEXITCODE -ne 0) { throw 'Scale native checks process failed' }
$taskLog=Get-Content "$taskOut/NativeChecks.log" -Raw
if ([regex]::Matches($taskLog,'Test Completed\. Result=\{Success\}').Count -ne 2 -or $taskLog -match 'Test Completed\. Result=\{Fail|Ensure condition failed|Fatal error|Critical error') { throw 'Scale native checks did not pass' }
& 'C:/Users/shaun/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' "$taskRoot/Tools/Check-CitySportsParkScale.py"
if ($LASTEXITCODE -ne 0) { throw 'Scale protection failed' }
Write-Output 'CITYSIM_D88_SCALE_NATIVE_TEST_PASS 2 affected cases'
