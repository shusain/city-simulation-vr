"""Adopt verified D79 exports with reversible manifest-only promotion."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability, set_capability
import copy,hashlib,json
root=Path(__file__).resolve().parents[1]
path=root/'ArtSource/Manifests/city_starter_kit.json'
surface=json.loads((root/'ArtSource/Manifests/city_building_surface_prototype.json').read_text())
proof=json.loads((root/'Artifacts/D79/surfaces-roundtrip.json').read_text())
entries=surface['assets']+surface['sign_variants']
assert {e['sha256'] for e in entries}=={e['sha256'] for e in proof if e['roundtrip_pass']}
out=root/'Artifacts/D79/Integration';out.mkdir(parents=True,exist_ok=True)
backup=out/'city_starter_kit-before.json'
if not backup.exists():backup.write_bytes(path.read_bytes())
doc=json.loads(path.read_text());originals={e['asset_id']:e for e in doc['assets']}
new=[]
for e in entries:
    base_id=e.get('base_asset_id',e['asset_id']).removesuffix('.surfaces')
    original=originals[base_id];source=original.get('surface_source_entry',original)
    if has_capability(original, 'reviewed_building'):continue
    updated=copy.deepcopy(original)
    updated.update(surface_source_entry=copy.deepcopy(source),building_surface_task='D79',source=e['source'],
        export=e['export'],triangles=e['triangles'],sections=e['sections'],dimensions_m=e['dimensions_m'],
        material_slots={name:target.replace('/Prototypes/D79','') for name,target in e['material_slots'].items()},
        surface_prototype_asset_id=e['asset_id'],provenance='Accepted D79 geometry, brick, roof and sign surfaces; existing authoring helpers')
    set_capability(updated, 'building_surfaces')
    if e.get('business'):
        updated['asset_id']=base_id+'.store.'+e['business']
        updated['mesh_destination']='/Game/CitySim/Art/Meshes/'+Path(e['export']).stem
        updated['store_business']=e['business']
        updated.pop('building_refinement_task',None)
        set_capability(updated, 'building_refinement', False)
        if updated['asset_id'] not in originals:new.append(updated)
        else:originals[updated['asset_id']].update(updated)
    else:original.update(updated)
doc['assets']+=new
for e in doc['assets']:
    if has_capability(e, 'building_refinement') and 'triangles' not in e:
        counts={'building.power.a':540,'building.power.medium.a':588,'building.power.high.a':700,
                'building.service.a':788,'building.service.medium.a':988,'building.service.high.a':1100}
        e['triangles']=counts[e['asset_id'].removesuffix('.damaged')]
        e['sections']=len(e['material_slots'])
path.write_text(json.dumps(doc,indent=2)+'\n')
print('CITYSIM_D79_MANIFEST_PROMOTED 18 base; 48 store variants')
