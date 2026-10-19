[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not $OutputDirectory){$OutputDirectory=Join-Path $taskRoot ('Artifacts/EntryPointValidation/'+[Guid]::NewGuid().ToString('N'))}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Preserve existing entry-point evidence'}
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$taskRows=Get-Content (Join-Path $taskRoot 'Tools/Common/CityToolEntryPoints.json') -Raw|ConvertFrom-Json
$taskCases=@{
 'Build-PCVRSandbox.ps1'=@{CapacityGridSize=80;ArchiveDirectory='C:\City [test] package';SkipEditorCompilation=$true}
 'Build-QuestSandbox.ps1'=@{AndroidSdkPath='C:\SDK [test]';NoTraceServer=$false}
 'Build-CitySimVR.ps1'=@{Preset='Smoke';CookOutputDirectory='C:\Cook [test]';AndroidSdkPath='C:\SDK [test]';SkipEditorCompilation=$true}
 'Start-DesktopSandbox.ps1'=@{Width=1024;Height=720;Windowed=$true;PrintOnly=$true;ExecutablePath='C:\Game [test]\game.exe'}
 'Start-PCVRSandbox.ps1'=@{GraphicsApi='D3D12';RuntimeManifest='C:\Runtime [test]\quoted" manifest.json';PreviousVRShadows=$true;PrintOnly=$true}
 'Start-CitySandboxEditor.ps1'=@{Mode='PCVR';Game=$true;LegacyColor=$true;PrintOnly=$true}
 'Deploy-CitySimVR.ps1'=@{Preset='Smoke';Serial='device [test]';ApkPath='C:\APK [test]\game.apk'}
 'Deploy-QuestSandbox.ps1'=@{Serial='device [test]';ApkPath='C:\APK [test]\game.apk'}
}
$taskResults=@()
foreach($taskRow in $taskRows){
 $taskTokens=$null;$taskErrors=$null
 $taskCanonical=Join-Path $taskRoot "Tools/$($taskRow.canonical)"
 $taskAst=[Management.Automation.Language.Parser]::ParseFile($taskCanonical,[ref]$taskTokens,[ref]$taskErrors)
 if($taskErrors.Count){throw 'Canonical entry point parse failed'}
 $taskHeader=$taskAst.Extent.Text.Substring(0,$taskAst.ParamBlock.Extent.EndOffset)
 if($taskHeader.Replace("`r`n","`n") -cne $taskRow.parameterContract.Replace("`r`n","`n")){throw "Parameter contract changed: $($taskRow.legacy)"}
 $taskFixture=Join-Path $OutputDirectory $taskRow.legacy
 $taskForward=Join-Path $taskFixture $taskRow.legacy
 $taskStub=Join-Path $taskFixture $taskRow.canonical
 [void][IO.Directory]::CreateDirectory((Split-Path $taskStub -Parent))
 Copy-Item -LiteralPath (Join-Path $taskRoot "Tools/$($taskRow.legacy)") -Destination $taskForward
 # Execute the actual forwarder into a console-only parameter receiver. It
 # cannot build, launch a game/Editor, install an APK or access a device.
 $taskReceiver='$received=@{}; foreach($item in $PSBoundParameters.GetEnumerator()) { $received[$item.Key]=if($item.Value -is [Management.Automation.SwitchParameter]){[bool]$item.Value}else{$item.Value} }; $received | ConvertTo-Json -Compress'
 [IO.File]::WriteAllText($taskStub,$taskRow.parameterContract+"`n"+$taskReceiver,[Text.UTF8Encoding]::new($false))
 $taskArguments=$taskCases[$taskRow.legacy]
 $taskActual=& $taskForward @taskArguments|ConvertFrom-Json
 if(@($taskActual.PSObject.Properties).Count -ne $taskArguments.Count){throw 'Forwarder added or dropped parameters'}
 foreach($taskPair in $taskArguments.GetEnumerator()){
  if($taskActual.($taskPair.Key).ToString() -cne $taskPair.Value.ToString()){throw "Forwarded value changed: $($taskRow.legacy)/$($taskPair.Key)"}
 }
 $taskResults+=@{legacy=$taskRow.legacy;canonical=$taskRow.canonical;status='passed';parameters=$taskArguments}
}
@{status='passed';cases=$taskResults;nativeOperations=$false}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'Result.json')
Write-Output "CITYSIM_ENTRY_POINT_PASS $($taskResults.Count) parameter contracts and forwarding cases"
