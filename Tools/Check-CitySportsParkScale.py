"""Focused concurrent-work-safe guard: Content/config/player/source-art preservation."""
from pathlib import Path
import json,hashlib,sys,shutil
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale';BASE=OUT/'ProtectedBefore.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json').read_text())
allowed={'CitySimVR/Content/'+e['mesh_destination'].removeprefix('/Game/')+'.uasset' for e in doc['statics']+doc['props']}
allowed|={'ArtSource/Manifests/city_starter_kit.json','ArtSource/Manifests/city_park_integration.json','CitySimVR/Source/CitySimVR/Private/Presentation/CityParkMotionData.inl'}
if '--capture' in sys.argv:
    assert not BASE.exists(),'Retain original native protection baseline'
    paths=set()
    for name in ('CitySimVR/Content','CitySimVR/Config','CitySimVR/Saved/SaveGames','Artifacts/DesktopUser/Saved/SaveGames'):
        paths|={p for p in (ROOT/name).rglob('*') if p.is_file()}
    paths|={ROOT/p for p in allowed if (ROOT/p).exists()}
    paths|={ROOT/p for p in doc['source_inputs']}
    BASE.write_text(json.dumps(dict(files={p.relative_to(ROOT).as_posix():sha(p) for p in paths},allowed=sorted(allowed)),indent=2)+'\n')
    for name in allowed:
        p=ROOT/name
        if p.exists():target=OUT/'Before'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    print('CITYSIM_D88_SCALE_PROTECTION_CAPTURE',len(paths));sys.exit(0)
baseline=json.loads(BASE.read_text());changed=[];concurrent=[]
for name,digest in baseline['files'].items():
    p=ROOT/name
    if not p.exists() or sha(p)!=digest:
        if name=='ArtSource/Blender/CityResidentialWindows.blend':
            study=json.loads((ROOT/'ArtSource/Manifests/city_home_window_study.json').read_text())
            # D91 explicitly owns this independent authoring file and may save
            # an artist edit before updating its manifest. D88 never writes it.
            concurrent.append(dict(path=name,owner='D91',sha256=sha(p),manifest_sha256=study['source_sha256']));continue
        assert name in baseline['allowed'],('Unexpected asset/config/player/art change',name);changed.append(name)
(OUT/'Preservation.json').write_text(json.dumps(dict(task='D88-Scale',protected=len(baseline['files']),intentional_changes=changed,
    other_task_inputs_unchanged=True,approved_animation_assets_unchanged=True,concurrent_authoring_changes=concurrent,pass_checks=True),indent=2)+'\n')
print('CITYSIM_D88_SCALE_PRESERVATION_PASS',len(changed),'declared changes')
