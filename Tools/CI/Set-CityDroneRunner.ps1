[CmdletBinding()]
param(
    [ValidateSet('Prepare', 'Start', 'Stop', 'Run')][string]$Action = 'Prepare'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskCiRoot = Join-Path $taskRoot 'Artifacts/CI'
$taskRunnerRoot = Join-Path $taskCiRoot 'Runner'
$taskConfig = Join-Path $taskRunnerRoot 'config'
$taskExe = Join-Path $taskRunnerRoot 'drone-runner-exec.exe'
$taskTaskName = 'CitySimVR-DroneRunner'
if ($env:OS -ne 'Windows_NT') { throw 'This worker requires Windows.' }

if ($Action -eq 'Prepare') {
    foreach ($taskDirectory in @($taskRunnerRoot, (Join-Path $taskCiRoot 'Workspaces'), (Join-Path $taskCiRoot 'Builds'))) {
        New-Item -ItemType Directory -Path $taskDirectory -Force | Out-Null
    }
    if (-not (Test-Path -LiteralPath $taskExe)) {
        $taskArchive = Join-Path $taskRunnerRoot 'drone_runner_exec_windows_amd64.tar.gz'
        Invoke-WebRequest -Uri 'https://github.com/drone-runners/drone-runner-exec/releases/download/v1.0.0-beta.10/drone_runner_exec_windows_amd64.tar.gz' -OutFile $taskArchive -UseBasicParsing
        if ((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash -ne '9D096C0F72FDF5C252CFA28F678357163D02E6DEA33365F28850CA6CC05C7772') {
            throw 'Official pinned runner archive SHA256 mismatch.'
        }
        & tar -xzf $taskArchive -C $taskRunnerRoot
        if ($LASTEXITCODE -ne 0) { throw 'Runner extraction failed.' }
    }
    if (-not (Test-Path -LiteralPath $taskConfig)) {
        @(
            'DRONE_RPC_PROTO=https'
            'DRONE_RPC_HOST=drone.shaun-husain.com'
            'DRONE_RPC_SECRET=SET_LOCALLY_FROM_EXISTING_DRONE_SERVER'
            "DRONE_RUNNER_NAME=citysim-$env:COMPUTERNAME"
            'DRONE_RUNNER_CAPACITY=1'
            'DRONE_RUNNER_LABELS=citysim:ue57'
            'DRONE_LIMIT_REPOS=CitySimVR/citysim'
            'DRONE_LIMIT_EVENTS=push,pull_request,promote'
            'DRONE_HTTP_BIND=127.0.0.1:3000'
            "DRONE_RUNNER_ROOT=$taskCiRoot\Workspaces"
            "DRONE_RUNNER_ENVFILE=$taskRunnerRoot\build.env"
            "DRONE_LOG_FILE=$taskRunnerRoot\runner.log"
        ) | Set-Content -LiteralPath $taskConfig -Encoding ASCII
        @(
            "CITYSIM_CI_ROOT=$taskCiRoot"
            "CITYSIM_DEVELOPMENT_ROOT=$taskRoot"
            "CITYSIM_META_XR_SOURCE=$taskRoot\CitySimVR\Plugins\MetaXR"
        ) | Set-Content -LiteralPath (Join-Path $taskRunnerRoot 'build.env') -Encoding ASCII
    }
    # The RPC secret stays local, ignored by Git, with no inherited read access.
    $taskIdentity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    & icacls $taskConfig /inheritance:r /grant:r "${taskIdentity}:(F)" '*S-1-5-18:(F)' '*S-1-5-32-544:(F)' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not restrict runner credential file access.' }
    Write-Output "Runner staged. Set the existing server RPC secret locally in $taskConfig; do not commit or print that file."
    return
}
$taskLauncher = Join-Path $PSScriptRoot 'Set-CityDroneRunner.ps1'
$taskArguments = "daemon `"$taskConfig`""
$taskLegacyArguments = "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$taskLauncher`" -Action Run"
$taskExisting = Get-ScheduledTask -TaskName $taskTaskName -ErrorAction SilentlyContinue
if ($taskExisting) {
    $taskActions = @($taskExisting.Actions)
    if ($taskActions.Count -ne 1 -or -not (
        ($taskActions[0].Execute -eq $taskExe -and $taskActions[0].Arguments -eq $taskArguments) -or
        ($taskActions[0].Execute -eq 'powershell.exe' -and $taskActions[0].Arguments -eq $taskLegacyArguments))) {
        throw 'An existing task with this name has different actions; inspect it before changing it.'
    }
}
function Get-OwnedRunnerProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name = 'drone-runner-exec.exe'" | Where-Object { $_.ExecutablePath -eq $taskExe })
}
if ($Action -eq 'Stop') {
    if ($taskExisting) { Stop-ScheduledTask -TaskName $taskTaskName }
    # Older PowerShell task wrappers can leave their native children alive.
    # Never infer native process termination from the task's Ready/Disabled state.
    foreach ($taskProcess in Get-OwnedRunnerProcesses) {
        Stop-Process -Id $taskProcess.ProcessId -ErrorAction Stop
    }
    if (@(Get-OwnedRunnerProcesses).Count) { throw 'An owned runner process did not stop.' }
    Write-Output 'Stopped the scheduled worker and verified no owned runner process remains.'
    return
}
if (-not (Test-Path -LiteralPath $taskExe) -or -not (Test-Path -LiteralPath $taskConfig)) { throw 'Run -Action Prepare first.' }
$taskConfigText = [IO.File]::ReadAllText($taskConfig)
if ($taskConfigText -notmatch '(?m)^DRONE_RPC_SECRET=\S+' -or $taskConfigText.Contains('SET_LOCALLY_FROM_EXISTING_DRONE_SERVER')) {
    throw 'Set the existing Drone server RPC secret in the local ignored config before starting.'
}
if ($Action -eq 'Run') {
    # Run as the signed-in user: Unreal, Android SDK/user settings and GPU rendering
    # must not silently switch to the LocalSystem profile in a Windows service.
    & $taskExe daemon $taskConfig
    if ($LASTEXITCODE -ne 0) { throw "Drone runner exited with code $LASTEXITCODE." }
    return
}
$taskUser = [Security.Principal.WindowsIdentity]::GetCurrent().Name
if (@(Get-OwnedRunnerProcesses).Count) { throw 'The worker is already running. Use -Action Stop before restarting it with changed configuration.' }
if (-not $taskExisting -or $taskExisting.Actions.Execute -ne $taskExe) {
    # Task Scheduler owns the actual daemon, not a shell parent that can leave
    # stale native children behind when the task is stopped.
    $taskAction = New-ScheduledTaskAction -Execute $taskExe -Argument $taskArguments -WorkingDirectory $taskRoot
    $taskTrigger = New-ScheduledTaskTrigger -AtLogOn -User $taskUser
    $taskPrincipal = New-ScheduledTaskPrincipal -UserId $taskUser -LogonType Interactive -RunLevel Limited
    $taskSettings = New-ScheduledTaskSettingsSet -Hidden -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    Register-ScheduledTask -TaskName $taskTaskName -Action $taskAction -Trigger $taskTrigger -Principal $taskPrincipal -Settings $taskSettings -Force | Out-Null
}
Enable-ScheduledTask -TaskName $taskTaskName | Out-Null
Start-ScheduledTask -TaskName $taskTaskName
Write-Output "Started $taskTaskName as $taskUser. Verify 'successfully pinged the remote server' in the local runner log."
