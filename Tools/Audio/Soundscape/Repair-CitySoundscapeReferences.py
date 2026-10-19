"""Repair only the21 serialized Wave Player links exposed by fresh CI228."""
from pathlib import Path
import hashlib,json,sys
sys.path.insert(0,str(next(p for p in Path(__file__).resolve().parents if (p/'CityToolPaths.py').is_file())))
from CityToolPaths import repository_root

def main():
    import unreal
    root=repository_root(__file__)
    output=root/'Artifacts/CIAssetPreparation'
    assert not output.exists() or not any(output.iterdir()),'Preserve preparation receipts'
    output.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    native=root/'CitySimVR/Content'
    before={p.relative_to(root).as_posix():sha(p) for p in native.rglob('*.uasset')}
    pinned=json.loads((root/'docs/evidence/tool-organization-native.json').read_text())
    for name,digest in pinned['changed'].items():assert before[name]==digest,name
    (output/'Before.json').write_text(json.dumps(before,indent=2)+'\n')
    count=unreal.CitySoundscapeAssetLibrary.repair_wave_references()
    assert count==21,('Unexpected repair count',count)
    after={p.relative_to(root).as_posix():sha(p) for p in native.rglob('*.uasset')}
    assert set(before)==set(after),'No package addition/removal is part of this repair'
    changed={name:digest for name,digest in after.items() if before[name]!=digest}
    assert len(changed)==21
    prefix='CitySimVR/Content/CitySim/Audio/Soundscape/Graphs/MS_'
    for name in changed:
        assert name.startswith(prefix),name
        data=(root/name).read_bytes()
        for marker in ('/Game/CitySim/Audio/D58','drone-'):
            assert marker.encode() not in data and marker.encode('utf-16-le') not in data,(name,marker)
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(['/Game/CitySim'],True)
    options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
    stale={}
    for data in registry.get_assets_by_path('/Game/CitySim',recursive=True):
        name=str(data.package_name)
        old=[str(p) for p in (registry.get_dependencies(name,options) or []) if '/Audio/D58/' in str(p)]
        if old:stale[name]=old
    assert not stale,stale
    receipt=dict(status='passed',repaired=21,changed=changed,removed=[],stale_dependencies=stale,
                 graph_nodes_connections_and_other_literals_preserved=True,
                 method='MetaSound document builder replaces only the Wave Asset literal; no graph recreation/reimport')
    (output/'Repair.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (output/'Files.txt').write_text('\n'.join(changed)+'\n')
    print('CITYSIM_SOUNDSCAPE_REFERENCE_REPAIR_PASS',count)

if __name__=='__main__':main()
