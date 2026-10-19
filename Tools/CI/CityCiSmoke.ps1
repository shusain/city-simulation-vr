# Shared by the read-only CI checks and the one-time administrator setup.
function Assert-CityCiPlainPath([string]$Path) {
    $taskFull = [IO.Path]::GetFullPath($Path)
    if ($taskFull -notmatch '^[A-Za-z]:\\') { throw 'CI smoke requires a local drive path.' }
    $taskPart = [IO.Path]::GetPathRoot($taskFull)
    foreach ($taskComponent in $taskFull.Substring($taskPart.Length).Split('\')) {
        if (-not $taskComponent) { continue }
        $taskPart = Join-Path $taskPart $taskComponent
        if (Test-Path -LiteralPath $taskPart) {
            if ((Get-Item -LiteralPath $taskPart -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "CI smoke refuses reparse points: $taskPart"
            }
        }
    }
    return $taskFull
}

function Get-CityCiSmokeLayout([string]$CiRoot) {
    $taskBase = (Assert-CityCiPlainPath $CiRoot).TrimEnd('\')
    if ($taskBase -notmatch '\\Artifacts\\CI$') { throw 'Expected the persistent Artifacts/CI directory.' }
    $taskSlot = Join-Path $taskBase 'WindowsSmoke'
    $taskRuntime = Join-Path $taskSlot 'Runtime'
    Assert-CityCiPlainPath $taskRuntime | Out-Null
    [pscustomobject]@{ ciRoot=$taskBase; slot=$taskSlot; runtime=$taskRuntime;
        executable=(Join-Path $taskRuntime 'CitySimVR/Binaries/Win64/CitySimVR.exe');
        marker=(Join-Path $taskSlot '.citysim-ci-smoke-owned'); lock=(Join-Path $taskBase 'windows-smoke.lock');
        rule='CitySimVR-CI-PackagedSmoke-Inbound-Block'; group='CitySimVR CI packaged smoke' }
}

function Assert-CityCiSmokeFirewall([string]$CiRoot) {
    $taskLayout = Get-CityCiSmokeLayout $CiRoot
    $taskRules = @(Get-NetFirewallRule -PolicyStore ActiveStore -Name $taskLayout.rule -ErrorAction SilentlyContinue)
    if ($taskRules.Count -ne 1) { throw 'CI packaged smoke firewall setup missing. Run Set-CityCiFirewall.ps1 -Action Install once as administrator; CI will not request elevation.' }
    $taskRule = $taskRules[0]
    $taskApp = $taskRule | Get-NetFirewallApplicationFilter
    $taskPort = $taskRule | Get-NetFirewallPortFilter
    $taskAddress = $taskRule | Get-NetFirewallAddressFilter
    $taskService = $taskRule | Get-NetFirewallServiceFilter
    $taskInterface = $taskRule | Get-NetFirewallInterfaceFilter
    $taskInterfaceType = $taskRule | Get-NetFirewallInterfaceTypeFilter
    if ($taskRule.Group -ne $taskLayout.group -or [string]$taskRule.Enabled -ne 'True' -or
        [string]$taskRule.Direction -ne 'Inbound' -or [string]$taskRule.Action -ne 'Block' -or
        [string]$taskRule.Profile -ne 'Any' -or $taskApp.Program -ne $taskLayout.executable -or
        [string]$taskPort.Protocol -ne 'Any' -or @($taskAddress.LocalAddress).Count -ne 1 -or
        @($taskAddress.RemoteAddress).Count -ne 1 -or @($taskAddress.LocalAddress)[0] -ne 'Any' -or
        @($taskAddress.RemoteAddress)[0] -ne 'Any' -or $taskService.Service -ne 'Any' -or
        @($taskInterface.InterfaceAlias).Count -ne 1 -or @($taskInterface.InterfaceAlias)[0] -ne 'Any' -or
        [string]$taskInterfaceType.InterfaceType -ne 'Any') {
        throw 'CI packaged smoke firewall rule does not match the exact executable/all-profile inbound block.'
    }
    $taskLayout
}

function Get-CityCiSmokeFiles([string]$Root) {
    $taskBase = (Assert-CityCiPlainPath $Root).TrimEnd('\')
    $taskPending = [Collections.Generic.Stack[string]]::new()
    $taskPending.Push($taskBase)
    while ($taskPending.Count) {
        foreach ($taskEntry in Get-ChildItem -LiteralPath $taskPending.Pop() -Force) {
            if ($taskEntry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "CI smoke refuses reparse points: $($taskEntry.FullName)" }
            if ($taskEntry.PSIsContainer) { $taskPending.Push($taskEntry.FullName) }
            else { [pscustomobject]@{ path=$taskEntry.FullName; relative=$taskEntry.FullName.Substring($taskBase.Length + 1); length=$taskEntry.Length } }
        }
    }
}

function Initialize-CityCiSmokeRuntime([string]$CiRoot, [string]$CandidateDirectory, [string]$ReportPath) {
    $taskLayout = Get-CityCiSmokeLayout $CiRoot
    $taskSource = (Assert-CityCiPlainPath $CandidateDirectory).TrimEnd('\')
    if (-not $taskSource.StartsWith((Join-Path $taskLayout.ciRoot 'Builds') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Smoke source must be a retained candidate under this CI root/Builds.'
    }
    $ReportPath = Assert-CityCiPlainPath $ReportPath
    if (-not $ReportPath.StartsWith((Join-Path $taskLayout.ciRoot 'Builds') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $ReportPath.StartsWith($taskSource + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Smoke receipt must be in job output outside the candidate archive.' }
    if (-not (Test-Path -LiteralPath (Join-Path $taskSource 'CitySimVR/Binaries/Win64/CitySimVR.exe') -PathType Leaf)) { throw 'Candidate game executable missing.' }
    $taskFiles = @(Get-CityCiSmokeFiles $taskSource)
    $taskLock = [IO.File]::Open($taskLayout.lock, 'OpenOrCreate', 'ReadWrite', 'None')
    try {
        $taskProcesses = @(Get-CimInstance Win32_Process | Where-Object {
            $_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskLayout.runtime + '\', [StringComparison]::OrdinalIgnoreCase)
        })
        if ($taskProcesses.Count) { throw 'An existing CI smoke process still owns the fixed runtime; it will not be stopped.' }
        if (Test-Path -LiteralPath $taskLayout.slot) {
            if (-not (Test-Path -LiteralPath $taskLayout.marker -PathType Leaf) -and @(Get-ChildItem -LiteralPath $taskLayout.slot -Force).Count) {
                throw 'Refusing to replace an unowned smoke directory.'
            }
        }
        New-Item -ItemType Directory -Path $taskLayout.slot -Force | Out-Null
        if (-not (Test-Path -LiteralPath $taskLayout.marker)) { [IO.File]::WriteAllText($taskLayout.marker, 'CitySimVR CI scratch runtime; retained candidates and player profiles live elsewhere.') }
        # Keep directory nodes: a shell/file browser may hold a directory even
        # when no game is running. Refresh only the guarded owned scratch files,
        # preserving the exact executable path used by the inbound block rule.
        if (Test-Path -LiteralPath $taskLayout.runtime) {
            foreach ($taskOldFile in @(Get-CityCiSmokeFiles $taskLayout.runtime)) {
                if (-not $taskOldFile.path.StartsWith($taskLayout.runtime + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Smoke reset file escaped its owned runtime.'
                }
                [IO.File]::Delete($taskOldFile.path)
            }
        }
        New-Item -ItemType Directory -Path $taskLayout.runtime -Force | Out-Null
        $taskInventory = @()
        foreach ($taskFile in $taskFiles) {
            $taskDestination = Join-Path $taskLayout.runtime $taskFile.relative
            New-Item -ItemType Directory -Path (Split-Path $taskDestination) -Force | Out-Null
            [IO.File]::Copy($taskFile.path, $taskDestination)
            $taskHash = (Get-FileHash -LiteralPath $taskFile.path -Algorithm SHA256).Hash.ToLowerInvariant()
            if ((Get-Item -LiteralPath $taskDestination).Length -ne $taskFile.length -or
                (Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskHash) { throw 'Smoke runtime differs from the retained candidate.' }
            $taskInventory += [ordered]@{ path=$taskFile.relative; length=$taskFile.length; sha256=$taskHash }
        }
        if (@(Get-CityCiSmokeFiles $taskLayout.runtime).Count -ne $taskFiles.Count) { throw 'Smoke runtime inventory mismatch.' }
        [ordered]@{ status='passed'; source=$taskSource; runtime=$taskLayout.runtime; executable=$taskLayout.executable;
            rule=$taskLayout.rule; inbound='Block'; profiles='Any'; files=$taskInventory; verifiedUtc=[DateTime]::UtcNow.ToString('o') } |
            ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
        [pscustomobject]@{ executable=$taskLayout.executable; runtime=$taskLayout.runtime; lock=$taskLock; fileCount=$taskFiles.Count }
    } catch { $taskLock.Dispose(); throw }
}
