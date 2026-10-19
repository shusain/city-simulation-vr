param([switch]$SkipBuild,[switch]$FocusedOnly,[switch]$BuildOnly)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOut=Join-Path $taskRoot 'Artifacts/D83/Integration'
$taskEngine='C:/Program Files/Epic Games/UE_5.7'
if (Get-Process UnrealEditor*,UnrealBuildTool*,CitySimVR* -ErrorAction SilentlyContinue) { throw 'Another native process is active' }
if (!$SkipBuild) {
    & "$taskEngine/Engine/Build/BatchFiles/Build.bat" CitySimVREditor Win64 Development "-Project=$taskRoot/CitySimVR/CitySimVR.uproject" "-Log=$taskOut/UBT.log" -NoUBA -WaitMutex -WarningsAsErrors *> "$taskOut/EditorBuild.log"
    if ($LASTEXITCODE -ne 0) { throw 'Editor build failed' }
}
if ($BuildOnly) { Write-Output 'CITYSIM_D83_EDITOR_BUILD_PASS'; return }
function Run-Checks([string]$Filter,[string]$Name,[int]$Expected) {
    & "$taskEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$taskRoot/CitySimVR/CitySimVR.uproject" "-ExecCmds=Automation RunTests $Filter" '-TestExit=Automation Test Queue Empty' -unattended -nop4 -nosplash -nocef -nohmd -d3d11 -RenderOffscreen "-UserDir=$taskOut/AutomationUser/$Name" -stdout "-abslog=$taskOut/$Name.log" "-CityEmergencyYieldProbe=$taskRoot/Artifacts/D74-StepServices/ReferenceRoadEditFailure.json" "-CityGrowthProbe=$taskRoot/Artifacts/D78/OriginalCity.json" -CityGrowthAdvanceDays=400 -CityGrowthExpectSuccess -CityGrowthExpectFamilies *> "$taskOut/$Name-Console.log"
    if ($LASTEXITCODE -ne 0) { throw "$Name process failed ($LASTEXITCODE)" }
    $taskLog=Get-Content "$taskOut/$Name.log" -Raw
    $taskSuccess=[regex]::Matches($taskLog,'Test Completed\. Result=\{Success\}').Count
    $taskErrors=@($taskLog -split '\r?\n' | Where-Object { $_ -match 'Log\w+: Error:' -and $_ -notmatch '\[\s*0\].*Log(Class|AutomationTest): Error: (LogClass: )?(EnumProperty FMRUKEnvironmentRaycastHit::status|StructProperty FMRUKEnvironmentRaycastHit::(point|orientation|normal)|StructProperty FOculusXRBodyJoint::OrientationQuat|ArrayProperty FOculusXRBodyState::Joints) is not initialized properly.*Module:(MRUtilityKit|OculusXRMovement)' })
    if ($taskSuccess -ne $Expected -or $taskErrors.Count -or $taskLog -match 'Test Completed\. Result=\{Fail|Ensure condition failed|Handled ensure|Fatal error|Critical error') { throw "$Name did not pass cleanly; see log" }
    Write-Output "$Name passed $taskSuccess tests"
}
Run-Checks 'CitySim.Parks.' 'Parks' 6
if (!$FocusedOnly) { Run-Checks 'CitySim.' 'Full' 253 }
& "$taskRoot/Tools/Format-CityCpp.ps1" -Check *> "$taskOut/Formatting.log"
if ($LASTEXITCODE -ne 0) { throw 'Formatting failed' }
& 'C:/Users/shaun/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' "$taskRoot/Tools/Check-CityParkIntegration.py"
if ($LASTEXITCODE -ne 0) { throw 'Protected inputs changed' }
Write-Output 'CITYSIM_D83_NATIVE_TEST_PASS'
