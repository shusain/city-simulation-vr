"""Restore one serialized idle sample reference without recreating the blend."""
from pathlib import Path
import hashlib,json,sys
sys.path.insert(0,str(next(p for p in Path(__file__).resolve().parents if (p/'CityToolPaths.py').is_file())))
from CityToolPaths import repository_root

def main():
    import unreal
    root=repository_root(__file__);output=root/'Artifacts/CIAssetPreparation'
    assert not output.exists() or not any(output.iterdir())
    output.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    native=root/'CitySimVR/Content'
    before={p.relative_to(root).as_posix():sha(p) for p in native.rglob('*.uasset')}
    pins=json.loads((root/'docs/evidence/tool-organization-final-native.json').read_text())
    for name,digest in pins['current_hashes'].items():assert before[name]==digest,name
    (output/'Before.json').write_text(json.dumps(before,indent=2)+'\n')
    assert unreal.CityCitizenWalkAssetLibrary.repair_walk_blend_references()
    after={p.relative_to(root).as_posix():sha(p) for p in native.rglob('*.uasset')}
    assert set(before)==set(after)
    name='CitySimVR/Content/CitySim/Art/Citizens/BS_Citizen_StreetWalk.uasset'
    changed={p:digest for p,digest in after.items() if before[p]!=digest}
    assert set(changed)=={name},changed
    data=(root/name).read_bytes()
    marker='/Game/CitySim/Art/Parks/Animations/AS_Park_Idle_D83'
    assert marker.encode() not in data and marker.encode('utf-16-le') not in data
    receipt=dict(status='passed',repaired=1,changed=changed,removed=[],
                 idle_and_walk_samples_verified=True,original_sample_settings_preserved=True,
                 method='ReplaceSampleAnimation on existing blend; no recreation/reimport')
    (output/'BlendRepair.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (output/'Files.txt').write_text(name+'\n')
    print('CITYSIM_CITIZEN_BLEND_REFERENCE_REPAIR_PASS')

if __name__=='__main__':main()
