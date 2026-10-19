[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Install','Status','Remove')][string]$Action = 'Status',
    [Parameter(Mandatory)][string]$CiRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'CityCiSmoke.ps1')
$taskLayout = Get-CityCiSmokeLayout $CiRoot
if ($Action -eq 'Status') {
    Assert-CityCiSmokeFirewall $CiRoot | Select-Object rule, executable
    return
}
$taskExisting = @(Get-NetFirewallRule -PolicyStore PersistentStore -Name $taskLayout.rule -ErrorAction SilentlyContinue)
if ($taskExisting.Count) {
    $taskApp = $taskExisting[0] | Get-NetFirewallApplicationFilter
    if ($taskExisting.Count -ne 1 -or $taskExisting[0].Group -ne $taskLayout.group -or $taskApp.Program -ne $taskLayout.executable) {
        throw 'An unrelated rule uses this name; nothing changed.'
    }
}
if ($Action -eq 'Install' -and $taskExisting.Count) {
    Assert-CityCiSmokeFirewall $CiRoot | Select-Object rule, executable
    return
}
if ($PSCmdlet.ShouldProcess($taskLayout.executable, "$Action the exact-program inbound block rule on all profiles")) {
    if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Windows administrator approval is required for this one-time firewall change. Do not elevate the CI worker.'
    }
    if ($Action -eq 'Install') {
        New-NetFirewallRule -Name $taskLayout.rule -DisplayName 'CitySimVR CI packaged smoke: block inbound' -Group $taskLayout.group `
            -Description 'Fixed CI scratch executable only. Prevents per-candidate firewall prompts without accepting inbound traffic. CI worker stays unprivileged.' `
            -Direction Inbound -Action Block -Enabled True -Profile Any -Protocol Any -Program $taskLayout.executable | Out-Null
        Assert-CityCiSmokeFirewall $CiRoot | Select-Object rule, executable
    } elseif ($taskExisting.Count) {
        $taskExisting | Remove-NetFirewallRule
    }
}
