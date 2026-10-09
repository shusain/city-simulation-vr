Set-StrictMode -Version Latest

function Test-CityRuntimePath([string]$RelativePath) {
    $taskPath = $RelativePath.Replace('\','/')
    if ($taskPath -match '(^/|:|(^|/)\.\.(/|$)|(^|/)(Saved|Intermediate|DerivedDataCache|\.git)(/|$))') { return $false }
    if ($taskPath -eq 'CitySimVR.exe') { return $true }
    if ($taskPath -notmatch '^(CitySimVR/(Binaries|Content/Paks|Plugins)/|Engine/(Binaries|Plugins|Config|Content)/|Engine/Extras/Redist/en-us/)') { return $false }
    # Meta runtime DLLs are staged under Plugins/MetaXR/Source/ThirdParty; their
    # runtime extension is allowed, but actual source/assets/symbols are not.
    return [IO.Path]::GetExtension($taskPath).ToLowerInvariant() -in @('.exe','.dll','.json','.ini','.pak','.utoc','.ucas','.bin','.cur','.ttf','.tps','.license')
}

function Test-CityBundlePath([string]$RelativePath) {
    if ($RelativePath -in @('Play-Desktop.cmd','Play-Desktop-Windowed.cmd','Play-PCVR.cmd','BUILD.txt','TESTING.md','KNOWN_ISSUES.md')) { return $true }
    return $RelativePath.StartsWith('Windows/') -and (Test-CityRuntimePath $RelativePath.Substring(8))
}

function Get-CityStreamHash($Stream) {
    $taskHasher = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($taskHasher.ComputeHash($Stream))).Replace('-','') }
    finally { $taskHasher.Dispose() }
}
