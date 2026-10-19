"""Capture/verify D83 preservation, permitting only declared production changes."""
from pathlib import Path
import json,hashlib,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration';OUT.mkdir(parents=True,exist_ok=True)
BASE=OUT/'ProtectedBefore.json'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text())
allowed={'CitySimVR/Content/'+e['mesh_destination'].removeprefix('/Game/')+'.uasset' for e in doc['statics']}
allowed|={'CitySimVR/Source/CitySimVR/'+p for p in ('Private/Presentation/CityBoardActor.cpp','Public/Presentation/CityBoardActor.h',
    'Private/Presentation/CityParkActivity.cpp','Public/Presentation/CityParkActivity.h')}
allowed|={'CitySimVR/Source/CitySimVR/Private/Tests/'+p for p in ('CityArtKitTests.cpp','CityParkTests.cpp','CityCitizenPresentationTests.cpp','CityUtilityUpgradeTests.cpp')}
allowed.add('ArtSource/Blender/CityParkIntegration.blend')
allowed.add('ArtSource/Blender/CityParkIntegration.blend1')
if '--capture' in sys.argv:
    assert not BASE.exists(),'Do not replace preservation baseline'
    paths=set((ROOT/'CitySimVR/Content').rglob('*'))|set((ROOT/'CitySimVR/Config').rglob('*'))|set((ROOT/'CitySimVR/Source').rglob('*'))
    paths|=set((ROOT/'CitySimVR/Saved/SaveGames').rglob('*'))
    for e in json.loads((ROOT/'Artifacts/D72-Presentation/ProtectedInputs.json').read_text(encoding='utf-8-sig')):paths.add(Path(e['Path']))
    for directory in ('ArtSource/Blender','ArtSource/Exports/ParkRefinement'):
        paths|=set((ROOT/directory).glob('*'))
    for directory in ROOT.glob('Artifacts/D72-*'):
        paths|={p for p in directory.rglob('*') if p.suffix.lower() in ('.json','.ps1','.log','.txt')}
    records={p.relative_to(ROOT).as_posix():digest(p) for p in paths if p.is_file()}
    BASE.write_text(json.dumps(records,indent=2)+'\n')
    for relative in allowed:
        p=ROOT/relative
        if p.exists():
            dest=OUT/'BeforeContent'/relative;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
    print('CITYSIM_D83_PRESERVATION_CAPTURE',len(records));sys.exit(0)
changed=[]
for relative,h in json.loads(BASE.read_text()).items():
    p=ROOT/relative
    if not p.exists() or digest(p)!=h:
        assert relative in allowed,('Unexpected protected change',relative)
        changed.append(relative)
(OUT/'Preservation.json').write_text(json.dumps(dict(protected=len(json.loads(BASE.read_text())),
    intentional_changes=changed,other_inputs_unchanged=True),indent=2)+'\n')
print('CITYSIM_D83_PRESERVATION_PASS',len(changed),'intentional changes')
