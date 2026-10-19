"""Retain the maintained delivery and protect unrelated runtime/source inputs."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/LowDensity/Integration'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    if (OUT/'Protected.json').exists():print('D104 baseline retained');return
    allowed={'CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset','CitySimVR/Content/CitySim/Maps/CitySandbox.umap',
        'CitySimVR/Content/CitySim/Maps/CitySandbox_BuiltData.uasset','CitySimVR/Content/CitySim/Art/Textures/T_CityRoomReflection.uasset',
        'CitySimVR/Source/CitySimVR/Public/Simulation/CityBuildingDefinition.h','CitySimVR/Source/CitySimVR/Private/Simulation/CityBuildingDefinition.cpp',
        'CitySimVR/Source/CitySimVR/Private/Presentation/CityBoardActor.cpp','CitySimVR/Source/CitySimVR/Private/Tests/CityStorefrontPresentationTests.cpp',
        'CitySimVR/Source/CitySimVR/Private/Tests/CityLowDensityPresentationTests.cpp',
        'CitySimVR/Source/CitySimVR/Public/Presentation/CityBoardActor.h','CitySimVR/Source/CitySimVR/Public/Interaction/CityVRToolController.h',
        'CitySimVR/Source/CitySimVR/Private/Tests/CityDesktopLowDensityTests.cpp',
        'CitySimVR/Source/CitySimVR/Private/Presentation/CityDesktopModelPreview.cpp',
        'CitySimVR/Source/CitySimVR/Private/Sandbox/CitySandboxEnvironment.cpp'}
    protected={}
    for folder in ['CitySimVR/Content','CitySimVR/Source','CitySimVR/Config']:
        for p in (ROOT/folder).rglob('*'):
            rel=p.relative_to(ROOT).as_posix()
            if p.is_file() and rel not in allowed and not rel.startswith('CitySimVR/Content/CitySim/Art/LowDensity/'):
                protected[rel]=digest(p)
    for p in (ROOT/'Tools').glob('Start-*.ps1'):protected[p.relative_to(ROOT).as_posix()]=digest(p)
    default=ROOT/'Artifacts/PCVRSandbox/Windows';saved=[]
    for p in default.rglob('*'):
        if p.is_file():
            rel=p.relative_to(default);backup=OUT/'PreviousDefaultWindows'/rel;backup.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(p,backup);assert digest(p)==digest(backup)
            saved.append(dict(path=rel.as_posix(),sha256=digest(p)))
    for rel in allowed:
        p=ROOT/rel
        if p.is_file() and rel.startswith('CitySimVR/Content/'):
            backup=OUT/'Before'/rel;backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,backup)
    (OUT/'Protected.json').write_text(json.dumps(protected,indent=2)+'\n')
    (OUT/'PreviousDefault.json').write_text(json.dumps(saved,indent=2)+'\n')
    print(f'D104 baseline preserved: {len(protected)} unrelated inputs; {len(saved)} previous default files')
if __name__=='__main__':main()
