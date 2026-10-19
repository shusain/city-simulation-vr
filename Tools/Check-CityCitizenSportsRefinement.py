"""D88 focused native/asset/player preservation guard with recoverable backups."""
from pathlib import Path
import json,sys,hashlib,shutil
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88';BASE=OUT/'ProtectedBefore.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_native.json').read_text())
allowed={f'CitySimVR/Content/CitySim/Art/Parks/Meshes/{prefix}_Park_Field_{style}.uasset' for prefix in ('SK','SM') for style in ('Casual','Office','Worker')}
allowed|={f'CitySimVR/Content/CitySim/Art/Parks/Animations/AS_Park_{e["name"]}.uasset' for e in doc['clips']}
allowed|={'CitySimVR/Source/CitySimVR/Private/Presentation/CityParkMotionData.inl',
          'CitySimVR/Source/CitySimVR/Private/Tests/CityParkRigTests.cpp','ArtSource/Manifests/city_park_integration.json'}
if '--capture' in sys.argv:
    assert not BASE.exists(),'Preserve the first native protection baseline'
    paths=set()
    for name in ('CitySimVR/Content','CitySimVR/Config','CitySimVR/Source','CitySimVR/Saved/SaveGames',
                 'Artifacts/DesktopUser/Saved/SaveGames'):
        paths|={p for p in (ROOT/name).rglob('*') if p.is_file()}
    paths.add(ROOT/'ArtSource/Manifests/city_park_integration.json')
    paths|={p for p in (ROOT/'ArtSource/Blender').glob('*.blend') if p.name!='CityCitizenSportsRefinement.blend'}
    entries={p.relative_to(ROOT).as_posix():sha(p) for p in paths}
    BASE.write_text(json.dumps(dict(files=entries,allowed=sorted(allowed)),indent=2)+'\n')
    for name in allowed:
        source=ROOT/name
        if source.exists():
            target=OUT/'Before'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    print('CITYSIM_D88_PROTECTION_CAPTURE',len(entries),'files');sys.exit(0)
baseline=json.loads(BASE.read_text());changed=[]
for name,digest in baseline['files'].items():
    p=ROOT/name
    if not p.exists() or sha(p)!=digest:
        assert name in baseline['allowed'],('Unexpected protected change',name)
        changed.append(name)
(OUT/'Preservation.json').write_text(json.dumps(dict(task='D88',protected=len(baseline['files']),intentional_changes=changed,
        all_other_inputs_unchanged=True,pass_checks=True),indent=2)+'\n')
print('CITYSIM_D88_PRESERVATION_PASS',len(changed),'intentional changes')
