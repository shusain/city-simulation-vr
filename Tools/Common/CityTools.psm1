Set-StrictMode -Version Latest

function Get-CityRepositoryRoot {
    [CmdletBinding()]
    param([string]$StartPath = $PSScriptRoot)
    $taskItem = Get-Item -LiteralPath $StartPath -ErrorAction Stop
    $taskDirectory = if ($taskItem.PSIsContainer) { $taskItem } else { $taskItem.Directory }
    while ($null -ne $taskDirectory) {
        if ((Test-Path -LiteralPath (Join-Path $taskDirectory.FullName 'CitySimVR/CitySimVR.uproject') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $taskDirectory.FullName 'Tools') -PathType Container)) {
            return $taskDirectory.FullName
        }
        $taskDirectory = $taskDirectory.Parent
    }
    throw "No CitySim checkout above $StartPath"
}

function ConvertTo-CityProcessArgument {
    param([AllowEmptyString()][string]$Value)
    # Start-Process joins ArgumentList on Windows. Quote for the Windows argv
    # parser, including embedded quotes and trailing backslashes, exactly once.
    $taskEscaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $taskEscaped = [regex]::Replace($taskEscaped, '(\\+)$', '$1$1')
    return '"' + $taskEscaped + '"'
}

function Invoke-CityToolProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]]$ArgumentList,
        [Parameter(Mandatory)][string]$LogPrefix,
        [ValidateRange(1, 3600)][int]$TimeoutSeconds = 900
    )
    $taskExecutable = (Get-Item -LiteralPath $FilePath -ErrorAction Stop).FullName
    $taskPrefix = [IO.Path]::GetFullPath($LogPrefix)
    $taskConsole = $taskPrefix + '-Console.log'
    $taskStderr = $taskPrefix + '-Stderr.log'
    foreach ($taskLog in @($taskConsole, $taskStderr)) {
        if (Test-Path -LiteralPath $taskLog) { throw "Preserve existing evidence; use a fresh log prefix: $taskPrefix" }
    }
    New-Item -ItemType Directory -Path (Split-Path $taskPrefix -Parent) -Force -ErrorAction Stop | Out-Null
    $taskArguments = (@($ArgumentList | ForEach-Object { ConvertTo-CityProcessArgument $_ }) -join ' ')
    # PowerShell 5 Start-Process treats redirect paths as wildcard patterns.
    # Stream to literal CreateNew files instead; this also closes the race
    # between the evidence-existence check and opening the output files.
    $taskProcess = [Diagnostics.Process]::new()
    $taskProcess.StartInfo.FileName = $taskExecutable
    $taskProcess.StartInfo.Arguments = $taskArguments
    $taskProcess.StartInfo.UseShellExecute = $false
    $taskProcess.StartInfo.CreateNoWindow = $true
    $taskProcess.StartInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $taskProcess.StartInfo.RedirectStandardOutput = $true
    $taskProcess.StartInfo.RedirectStandardError = $true
    $taskConsoleStream = $null
    $taskStderrStream = $null
    $taskStarted = $false
    try {
        $taskConsoleStream = [IO.File]::Open($taskConsole, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
        $taskStderrStream = [IO.File]::Open($taskStderr, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
        $taskStarted = $taskProcess.Start()
        if (-not $taskStarted) { throw "Could not start tool: $taskExecutable" }
        $taskCopies = @($taskProcess.StandardOutput.BaseStream.CopyToAsync($taskConsoleStream),
                        $taskProcess.StandardError.BaseStream.CopyToAsync($taskStderrStream))
        $taskStartedUtc = $taskProcess.StartTime.ToUniversalTime()
        if (-not $taskProcess.WaitForExit($TimeoutSeconds * 1000)) {
            if (-not $taskProcess.HasExited) { $taskProcess.Kill() }
            [void]$taskProcess.WaitForExit(10000)
            [void][Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]$taskCopies, 10000)
            throw "Owned tool process timed out after ${TimeoutSeconds}s; see $taskConsole"
        }
        $taskProcess.WaitForExit()
        if (-not [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]$taskCopies, 10000)) {
            throw "Owned tool output did not finish; see $taskConsole"
        }
        if ($taskProcess.ExitCode -ne 0) {
            throw "Tool exited $($taskProcess.ExitCode); see $taskConsole and $taskStderr"
        }
        return [pscustomobject]@{ ExitCode=$taskProcess.ExitCode; StartedUtc=$taskStartedUtc;
            ConsoleLog=$taskConsole; StderrLog=$taskStderr }
    } finally {
        if ($taskStarted -and -not $taskProcess.HasExited) { $taskProcess.Kill(); [void]$taskProcess.WaitForExit(10000) }
        $taskProcess.Dispose()
        if ($null -ne $taskConsoleStream) { $taskConsoleStream.Dispose() }
        if ($null -ne $taskStderrStream) { $taskStderrStream.Dispose() }
    }
}

function Read-CityToolReport {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][datetime]$StartedUtc
    )
    $taskReport = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($taskReport.PSIsContainer -or $taskReport.LastWriteTimeUtc -lt $StartedUtc.ToUniversalTime()) {
        throw "Fresh tool report required: $Path"
    }
    $taskResult = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    if ($null -eq $taskResult) { throw "Empty tool report: $Path" }
    return $taskResult
}

function Resolve-CityToolPython {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$RepositoryRoot, [string]$Python = $env:CITYSIM_TOOL_PYTHON,
          [string]$Fallback)
    $taskCandidates = if ($Python) { @($Python) } else {
        @((Join-Path $RepositoryRoot '.venv/Scripts/python.exe'), 'python', $Fallback)
    }
    foreach ($taskCandidate in $taskCandidates) {
        if (-not $taskCandidate) { continue }
        $taskCommand = Get-Command $taskCandidate -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $taskCommand) { continue }
        # Match CI's hidden user-site environment; the dependency must belong to
        # the selected environment, not an interactive account's user site.
        try {
            $taskProbe = & $taskCommand.Source -I -c 'import sys, PIL; assert sys.version_info >= (3,11); assert (12,3) <= tuple(map(int, PIL.__version__.split(chr(46))[:2])) < (13,0)' 2>&1
        } catch { continue }
        if ($LASTEXITCODE -eq 0) { return $taskCommand.Source }
    }
    throw 'Python 3.11+ with Tools/requirements.txt dependencies required; create .venv or supply -Python / CITYSIM_TOOL_PYTHON. An explicit selection never falls back silently.'
}

Export-ModuleMember -Function Get-CityRepositoryRoot, Invoke-CityToolProcess, Read-CityToolReport, Resolve-CityToolPython
