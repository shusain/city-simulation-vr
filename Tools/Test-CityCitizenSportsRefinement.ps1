param([switch]$BuildEditor)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D88'
$taskEngine='C:/Program Files/Epic Games/UE_5.7'
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process is active' }
if ($BuildEditor) {
    & "$taskEngine/Engine/Build/BatchFiles/Build.bat" CitySimVREditor Win64 Development "-Project=$taskRoot/CitySimVR/CitySimVR.uproject" "-Log=$taskOut/UBT.log" -NoUBA -WaitMutex -WarningsAsErrors *> "$taskOut/EditorBuild.log"
    if ($LASTEXITCODE -ne 0) { throw 'D88 Editor build failed' }
}
$taskFilter='CitySim.Parks.AuthoredMomentumAndReplay+CitySim.Parks.NativeRigLODAndDeckContact+CitySim.Parks.RefinedSportsContactsAndGrounding'
& "$taskEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$taskRoot/CitySimVR/CitySimVR.uproject" "-ExecCmds=Automation RunTests $taskFilter" '-TestExit=Automation Test Queue Empty' -unattended -nop4 -nosplash -nocef -nohmd -d3d11 -RenderOffscreen "-UserDir=$taskOut/NativeChecksUser" -stdout "-abslog=$taskOut/NativeChecks.log" *> "$taskOut/NativeChecks-Console.log"
if ($LASTEXITCODE -ne 0) { throw 'D88 native checks process failed' }
$taskLog=Get-Content "$taskOut/NativeChecks.log" -Raw
$taskPassed=[regex]::Matches($taskLog,'Test Completed\. Result=\{Success\}').Count
if ($taskPassed -ne 3 -or $taskLog -match 'Test Completed\. Result=\{Fail|Ensure condition failed|Fatal error|Critical error') { throw 'D88 targeted native checks did not pass' }
& 'C:/Users/shaun/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' "$taskRoot/Tools/Check-CityCitizenSportsRefinement.py"
if ($LASTEXITCODE -ne 0) { throw 'D88 preservation failed' }
Write-Output 'CITYSIM_D88_NATIVE_TEST_PASS 3 affected park checks'
