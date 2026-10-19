[CmdletBinding()]
param([Parameter(Mandatory)][string]$Python, [Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../Common/CityTools.psm1') -Force
$taskRoot=Get-CityRepositoryRoot -StartPath $PSCommandPath
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh fixture directory' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$taskOutput=(Get-Item -LiteralPath $OutputDirectory).FullName
$taskFake=Join-Path $taskOutput 'Fixture Renderer.exe'
# A tiny local console fixture speaks only the argument/report protocol. It
# cannot render meshes, open a blend or launch any Blender/Unreal process.
$taskSource=@'
using System;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Drawing.Imaging;
using System.Security.Cryptography;
using System.Web.Script.Serialization;
public class CityPreviewFixture {
  static string Value(string[] a,string name) { return a[Array.IndexOf(a,name)+1]; }
  static string Digest(string path) { using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-","").ToLowerInvariant(); }
  public static int Main(string[] args) {
    var mode=Environment.GetEnvironmentVariable("CITYSIM_PREVIEW_TEST_MODE")??"success";
    if(args.Contains("--manifest")) { return mode=="compose-fail"?9:0; }
    string output=Value(args,"--output"), source=Value(args,"--blend");
    Directory.CreateDirectory(output);
    var json=new JavaScriptSerializer();
    File.WriteAllText(Path.Combine(output,"Arguments.json"),json.Serialize(args));
    if(mode=="exit") return 7;
    if(mode=="timeout") System.Threading.Thread.Sleep(30000);
    if(mode=="mutate") { File.AppendAllText(source,"changed"); return 0; }
    if(args.Contains("--list")) { Console.WriteLine("CITYSIM_BLEND_OBJECT_LIST_PASS fixture mesh"); return 0; }
    if(mode=="missing") return 0;
    string report=Path.Combine(output,"PreviewManifest.json");
    if(mode=="malformed") { File.WriteAllText(report,"{broken"); return 0; }
    int size=Int32.Parse(Value(args,"--resolution"));
    string image=Path.Combine(output,"fixture.png");
    using(var bitmap=new Bitmap(size,size)) bitmap.Save(image,ImageFormat.Png);
    var view=new {image="fixture.png",sha256=Digest(image),@object="Fixture mesh",view="iso",dimensions_m=new[]{1,1,1},triangles=12};
    var manifest=new {schema_version=1,source=source,source_sha256=mode=="identity"?"wrong":Digest(source),source_unchanged=true,scene="Fixture",frame=0,engine="workbench",color="material",resolution=size,views=mode=="empty"?new object[0]:new object[]{view}};
    File.WriteAllText(report,json.Serialize(manifest));
    if(mode=="stale") File.SetLastWriteTimeUtc(report,DateTime.UtcNow.AddDays(-1));
    return 0;
  }
}
'@
Add-Type -TypeDefinition $taskSource -ReferencedAssemblies 'System.dll','System.Core.dll','System.Drawing.dll','System.Web.Extensions.dll' -OutputAssembly $taskFake -OutputType ConsoleApplication
$taskBlend=Join-Path $taskOutput 'Source [fixture] with spaces.blend'
[IO.File]::WriteAllText($taskBlend,'synthetic source protocol fixture')
$taskOriginal=[IO.File]::ReadAllBytes($taskBlend)
$taskChecks=[Collections.Generic.List[string]]::new()
$taskCanonical=Join-Path $taskRoot 'Tools/Assets/Blender/Render-CityBlendPreviewSheet.ps1'
$taskLegacy=Join-Path $taskRoot 'Tools/Render-CityBlendPreviewSheet.ps1'
$taskModeBefore=$env:CITYSIM_PREVIEW_TEST_MODE
function Assert-Check([bool]$condition,[string]$message) {
    if(-not $condition){throw $message};$taskChecks.Add($message)
}
function Invoke-Fixture([string]$Name,[string]$Mode,[string]$Entry=$taskCanonical,[string]$SheetPython=$Python) {
    $env:CITYSIM_PREVIEW_TEST_MODE=$Mode
    & $Entry -BlendFile $taskBlend -Blender $taskFake -Python $SheetPython -OutputDirectory (Join-Path $taskOutput $Name) -Resolution 64 -TileSize 64 -TimeoutSeconds 10
}
function Assert-Rejected([string]$Name,[string]$Mode,[string]$Pattern,[string]$SheetPython=$Python) {
    try { Invoke-Fixture $Name $Mode -SheetPython $SheetPython | Out-Null } catch {
        if($_.Exception.Message -notmatch $Pattern){throw}
        $taskChecks.Add($Name);return
    }
    throw "Expected rejection: $Name"
}
try {
    $env:CITYSIM_PREVIEW_TEST_MODE='success'
    $taskDirectory=Join-Path $taskOutput 'canonical [quoted]'
    $taskPatterns=@('Mesh one','Mesh[?]','quote"tail\')
    $taskResult=@(& $taskCanonical -BlendFile $taskBlend -Blender $taskFake -Python $Python -OutputDirectory $taskDirectory -Resolution 64 -TileSize 64 -ObjectPattern $taskPatterns -Scene 'Scene "A"' -Frame 0 -Views iso,front -IncludeHidden -Title 'Fixture "quoted"')
    $taskArguments=Get-Content -LiteralPath (Join-Path $taskDirectory 'Arguments.json') -Raw | ConvertFrom-Json
    Assert-Check (@($taskPatterns|Where-Object {$_ -notin $taskArguments}).Count -eq 0 -and 'Scene "A"' -in $taskArguments -and '--include-hidden' -in $taskArguments -and '0' -in $taskArguments -and 'front' -in $taskArguments) 'Raw quoted/spaced/pattern/frame/view arguments survive the native argv parser'
    Assert-Check ($taskResult.Count -eq 2 -and (Test-Path -LiteralPath $taskResult[0]) -and (Test-Path -LiteralPath $taskResult[1])) 'Canonical wrapper composes real indexed sheet and returns both paths'
    $null=Invoke-Fixture 'legacy' 'success' $taskLegacy
    Assert-Check (Test-Path -LiteralPath (Join-Path $taskOutput 'legacy/PreviewSheets.json')) 'Legacy PowerShell entry forwards to canonical workflow'
    $taskList=Join-Path $taskOutput 'list'
    $taskListing=& $taskLegacy -BlendFile $taskBlend -Blender $taskFake -Python 'missing-unused-python.exe' -OutputDirectory $taskList -ListObjects
    Assert-Check ($taskListing -match 'CITYSIM_BLEND_OBJECT_LIST_PASS' -and -not (Test-Path -LiteralPath (Join-Path $taskList 'Compose-Console.log'))) 'List mode surfaces console output without requiring or starting the composer'
    Assert-Rejected 'nonzero' 'exit' 'Tool exited 7'
    Assert-Rejected 'missing-report' 'missing' 'does not exist|Cannot find path'
    Assert-Rejected 'malformed-report' 'malformed' 'JSON|primitive|Invalid|Unexpected'
    Assert-Rejected 'stale-report' 'stale' 'Fresh tool report'
    Assert-Rejected 'wrong-source' 'identity' 'Invalid Blender preview manifest'
    Assert-Rejected 'empty-views' 'empty' 'Invalid Blender preview manifest'
    Assert-Rejected 'changed-source' 'mutate' 'Source blend changed'
    [IO.File]::WriteAllBytes($taskBlend,$taskOriginal)
    Assert-Rejected 'composer-nonzero' 'compose-fail' 'Tool exited 9' $taskFake
    Assert-Rejected 'composer-missing-report' 'compose-missing' 'does not exist|Cannot find path' $taskFake
    $env:CITYSIM_PREVIEW_TEST_MODE='timeout'
    $taskTimer=[Diagnostics.Stopwatch]::StartNew()
    try { & $taskCanonical -BlendFile $taskBlend -Blender $taskFake -Python $Python -OutputDirectory (Join-Path $taskOutput 'timeout') -TimeoutSeconds 1 | Out-Null;throw 'Expected timeout' } catch {
        if($_.Exception.Message -notmatch 'Owned tool process timed out'){throw}
    }
    Assert-Check ($taskTimer.Elapsed.TotalSeconds -lt 15) 'Wrapper enforces timeout for its owned process'
    $taskExisting=Join-Path $taskOutput 'existing'
    New-Item -ItemType Directory -Path $taskExisting | Out-Null
    'preserved'|Set-Content (Join-Path $taskExisting 'sentinel.txt')
    try { & $taskCanonical -BlendFile $taskBlend -Blender $taskFake -Python $Python -OutputDirectory $taskExisting | Out-Null;throw 'Expected output rejection' } catch {
        if($_.Exception.Message -notmatch 'preserve existing evidence'){throw}
    }
    Assert-Check ((Get-Content (Join-Path $taskExisting 'sentinel.txt')) -eq 'preserved' -and -not (Test-Path -LiteralPath (Join-Path $taskExisting 'Arguments.json'))) 'Existing output is preserved before any child launch'
    Assert-Check (([Convert]::ToBase64String([IO.File]::ReadAllBytes($taskBlend))) -eq [Convert]::ToBase64String($taskOriginal)) 'Fixture source is unchanged after successful preview runs'
} finally {
    $env:CITYSIM_PREVIEW_TEST_MODE=$taskModeBefore
}
@{status='Pass';checks=@($taskChecks);blenderLaunched=$false;unrealLaunched=$false;syntheticRenderer=$true}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $taskOutput 'Result.json')
Write-Output "CITYSIM_PREVIEW_WRAPPER_PASS $($taskChecks.Count) checks"
