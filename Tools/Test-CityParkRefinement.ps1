param([switch]$Author)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskBlender = 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe'
$taskPython = 'C:\Users\shaun\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
Push-Location $taskRoot
try {
    New-Item -ItemType Directory -Path 'Artifacts/D83' -Force | Out-Null
    if ($Author) {
        & $taskBlender --background --factory-startup --threads 2 --python Tools/Validate-CityParkAcceptedMotion.py -- --capture *> Artifacts/D83/AcceptedMotion.log
        if ($LASTEXITCODE -ne 0 -or !(Select-String -Path Artifacts/D83/AcceptedMotion.log -SimpleMatch 'CITYSIM_D83_ACCEPTED_MOTION_PASS 30 actions' -Quiet)) {
            throw 'Accepted baseball/soccer animation guard failed'
        }
        & $taskBlender --background --factory-startup --threads 2 --python Tools/Create-CityParkRefinement.py *> Artifacts/D83/Authoring.log
        if ($LASTEXITCODE -ne 0 -or !(Select-String -Path Artifacts/D83/Authoring.log -SimpleMatch 'CITYSIM_D83_AUTHORING_PASS 8 static meshes; 6 rigged meshes' -Quiet)) {
            throw 'D83 authoring failed; inspect Artifacts/D83/Authoring.log'
        }
    }
    & $taskBlender --background --factory-startup --threads 2 --python Tools/Validate-CityParkAcceptedMotion.py *> Artifacts/D83/AcceptedMotion.log
    if ($LASTEXITCODE -ne 0 -or !(Select-String -Path Artifacts/D83/AcceptedMotion.log -SimpleMatch 'CITYSIM_D83_ACCEPTED_MOTION_PASS 30 actions' -Quiet)) {
        throw 'Accepted baseball/soccer animations changed'
    }
    & $taskBlender --background --factory-startup --threads 2 --python Tools/Validate-CityParkRefinement.py *> Artifacts/D83/Validation.log
    if ($LASTEXITCODE -ne 0 -or !(Select-String -Path Artifacts/D83/Validation.log -SimpleMatch 'CITYSIM_D83_VALIDATION_PASS 8 static; 6 source rigs; 6 FBX rigs' -Quiet)) {
        throw 'D83 validation failed; inspect Artifacts/D83/Validation.log'
    }
    if ($Author) {
        & $taskBlender --background --factory-startup --threads 2 --python Tools/Create-CityParkAnimationPreview.py *> Artifacts/D83/Sports.log
    } else {
        & $taskBlender --background --factory-startup --threads 2 --python Tools/Create-CityParkAnimationPreview.py -- --validate-only *> Artifacts/D83/Sports.log
    }
    if ($LASTEXITCODE -ne 0 -or !(Select-String -Path Artifacts/D83/Sports.log -SimpleMatch 'CITYSIM_D83_SPORTS_VALIDATION_PASS 12 scenes' -Quiet)) {
        throw 'D83 sports preview failed; inspect Artifacts/D83/Sports.log'
    }
    & $taskPython Tools/Render-CityParkRefinementSheet.py
    if ($LASTEXITCODE -ne 0) { throw 'D83 preview sheets failed' }
    Write-Output 'CITYSIM_D83_BLENDER_ONLY_PASS'
} finally { Pop-Location }
