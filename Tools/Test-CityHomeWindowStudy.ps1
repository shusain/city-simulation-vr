[CmdletBinding()]
param([ValidateSet('Import','Capture')][string]$Mode='Import',[switch]$MeshesOnly,[switch]$HomeDetails,[switch]$HomeScale)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskBase=Join-Path $taskRoot 'Artifacts/D91'
$taskOut=if($HomeScale){Join-Path $taskBase 'Scale'}elseif($HomeDetails){Join-Path $taskBase 'Details'}else{$taskBase}
$taskProject=Join-Path $taskBase 'UnrealProject'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
New-Item -ItemType Directory -Path $taskProject -Force | Out-Null
$taskReport=Join-Path $taskOut $(if($Mode -eq 'Import'){'Import.json'}else{'Unreal/Report.json'})
if (Test-Path -LiteralPath $taskReport) { Remove-Item -LiteralPath $taskReport }
if ($Mode -eq 'Import') {
    # Copy only the existing surface dependency set. No C++/gameplay module,
    # player data, map, vendor plugin, package or native-build output is shared.
    foreach ($taskFolder in @('Materials','Textures')) {
        $taskFrom=Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskFolder"
        $taskTo=Join-Path $taskProject "Content/CitySim/Art/$taskFolder"
        New-Item -ItemType Directory -Path $taskTo -Force | Out-Null
        Get-ChildItem -LiteralPath $taskFrom -File | Copy-Item -Destination $taskTo -Force
    }
    if ($HomeScale) {
        foreach ($taskAsset in @('PC/Materials/M_CityCitizenPalette.uasset','PC/Meshes/SM_Citizen_PC_Casual.uasset','Parks/Materials/M_ParkCitizenPalette.uasset','Parks/Meshes/SM_Park_Field_Casual.uasset')) {
            $taskTo=Join-Path $taskProject "Content/CitySim/Art/$taskAsset"
            New-Item -ItemType Directory -Path (Split-Path $taskTo -Parent) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $taskRoot "CitySimVR/Content/CitySim/Art/$taskAsset") -Destination $taskTo -Force
        }
    }
    '{"FileVersion":3,"EngineAssociation":"5.7","Plugins":[{"Name":"PythonScriptPlugin","Enabled":true},{"Name":"EditorScriptingUtilities","Enabled":true}]}' | Set-Content -LiteralPath (Join-Path $taskProject 'HomeWindowStudy.uproject')
    $taskConfig=Join-Path $taskProject 'Config';New-Item -ItemType Directory -Path $taskConfig -Force | Out-Null
    # Use the game's forward-rendering path for the material comparison.
    @('[SystemSettings]','r.ForwardShading=1','r.ReflectionMethod=0','r.DynamicGlobalIlluminationMethod=0','r.DefaultFeature.AutoExposure=0','r.DefaultFeature.MotionBlur=0') | Set-Content (Join-Path $taskConfig 'DefaultEngine.ini')
}
$taskEditor='C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskArgs=@((Join-Path $taskProject 'HomeWindowStudy.uproject'),'-nohmd','-d3d11','-nosound','-RenderOffscreen','-unattended','-nop4','-nosplash','-stdout','-nocef',"-abslog=$taskOut/$Mode-Engine.log",'-ini:Engine:[ConsoleVariables]:Interchange.FeatureFlags.Import.FBX=0')
if ($Mode -eq 'Import') { $taskArgs+=@('-run=pythonscript',"-Script=$PSScriptRoot/Import-CityHomeWindowStudy.py",'-AllowCommandletRendering') }
else { $taskArgs+=@("-ExecCmds=py $PSScriptRoot/Run-CityHomeWindowCapture.py") }
if ($MeshesOnly) { $taskArgs+='-D91MeshesOnly' }
if ($HomeDetails) { $taskArgs+='-D91HomeDetails' }
if ($HomeScale) { $taskArgs+='-D91HomeScale' }
& $taskEditor @taskArgs *> (Join-Path $taskOut "$Mode-Console.log")
if ($LASTEXITCODE -ne 0) { throw "D91 $Mode exited $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $taskReport)) { throw "D91 $Mode success report missing" }
$taskResult=Get-Content -LiteralPath $taskReport -Raw | ConvertFrom-Json
if ($Mode -eq 'Capture' -and $taskResult.views.Count -ne 12) { throw 'D91 native comparison is incomplete' }
if ($Mode -eq 'Import' -and $taskResult.meshes.Count -ne $(if($HomeDetails -or $HomeScale){2}else{3})) { throw 'D91 native mesh import is incomplete' }
Write-Output "CITYSIM_D91_$($Mode.ToUpper())_PASS"
