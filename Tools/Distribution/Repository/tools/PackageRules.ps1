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

function Test-CityQuestPayload([string]$ApkPath) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $taskApk = [IO.Compression.ZipFile]::OpenRead($ApkPath)
    $taskMemory = [IO.MemoryStream]::new()
    try {
        $taskSeen = @{}
        foreach ($taskEntry in $taskApk.Entries) {
            $taskName = $taskEntry.FullName
            if ($taskName -match '(^/|:|\\|(^|/)\.\.(/|$))' -or $taskSeen.ContainsKey($taskName)) { throw 'Unsafe/duplicate APK entry.' }
            $taskSeen[$taskName] = $true
            if ($taskName -match '\.(cpp|h|pdb|uasset|umap|keystore|jks)$' -or $taskName -match '(^|/)(Saved|Intermediate|\.git)(/|$)') { throw 'Forbidden source/editable asset/data in APK.' }
        }
        foreach ($taskRequired in @('AndroidManifest.xml','classes.dex','lib/arm64-v8a/libUnreal.so','lib/arm64-v8a/libopenxr_loader.so','assets/main.obb.png')) {
            if (-not $taskSeen.ContainsKey($taskRequired)) { throw "Required Quest payload missing: $taskRequired" }
        }
        $taskStream = $taskApk.GetEntry('assets/main.obb.png').Open()
        try { $taskStream.CopyTo($taskMemory) } finally { $taskStream.Dispose() }
        $taskMemory.Position = 0
        $taskObb = [IO.Compression.ZipArchive]::new($taskMemory,[IO.Compression.ZipArchiveMode]::Read,$true)
        try {
            $taskNames = @{}
            foreach ($taskEntry in $taskObb.Entries) {
                $taskName = $taskEntry.FullName
                if ($taskName -match '(^/|:|\\|(^|/)\.\.(/|$)|(^|/)(Source|Saved|Intermediate|\.git)(/|$))' -or $taskNames.ContainsKey($taskName)) { throw 'Unsafe/source/data entry in Quest game payload.' }
                if ($taskName -notmatch '^(Engine/(Binaries/ThirdParty/Vulkan/Android/16kb_page_support\.patch|Config/[^/]+\.ini|Content/.*\.(bin|tps|ttf)|Extras/GPUDumpViewer/[^/]+\.(html|bat|sh))|CitySimVR/Content/Paks/[^/]+\.(pak|utoc|ucas))$') { throw "Unexpected Quest game payload: $taskName" }
                $taskNames[$taskName] = $true
            }
            foreach ($taskRequired in @('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas')) {
                if (-not $taskNames.ContainsKey("CitySimVR/Content/Paks/$taskRequired")) { throw "Missing cooked Quest content: $taskRequired" }
            }
        } finally { $taskObb.Dispose() }
    } finally { $taskMemory.Dispose(); $taskApk.Dispose() }
}
