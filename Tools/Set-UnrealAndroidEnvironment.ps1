# Dot-source before launching Unreal or running its build tools:
# . .\Tools\Set-UnrealAndroidEnvironment.ps1
# Sets this PowerShell process and its future children only.
[CmdletBinding()]
param(
    [string]$JdkPath = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot',
    [string]$SdkPath = (Join-Path $env:LOCALAPPDATA 'Android\Sdk')
)

$taskNdkPath = Join-Path $SdkPath 'ndk\27.2.12479018'
foreach ($taskRequiredFile in @(
    (Join-Path $JdkPath 'bin\java.exe'),
    (Join-Path $JdkPath 'bin\javac.exe'),
    (Join-Path $SdkPath 'platforms\android-35\android.jar'),
    (Join-Path $SdkPath 'build-tools\35.0.1\aapt2.exe'),
    (Join-Path $taskNdkPath 'source.properties')
)) {
    if (-not (Test-Path -LiteralPath $taskRequiredFile)) {
        throw "Required Android toolchain file missing: $taskRequiredFile"
    }
}

$env:JAVA_HOME = $JdkPath
$env:ANDROID_HOME = $SdkPath
$env:NDKROOT = $taskNdkPath
$env:NDK_ROOT = $taskNdkPath
Write-Output "Unreal Android environment configured for this process: Java 21, SDK $SdkPath, NDK r27c."
