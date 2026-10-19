"""Fresh-process read-only native persistence/source ownership check."""
from pathlib import Path
import unreal,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D83/Integration'
doc=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text());assert doc['native_selected']
for relative,expected in doc['source_inputs'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
records=[]
for e in doc['statics']:
    mesh=unreal.load_asset(e['mesh_destination']);assert mesh
    assert mesh.get_num_lods()==2
    assert [mesh.get_num_triangles(i) for i in (0,1)]==[l['triangles'] for l in e['lods']]
    assert mesh.get_minimum_lod_for_platform('Android')==1
    for slot in mesh.get_editor_property('static_materials'):
        name=str(slot.get_editor_property('imported_material_slot_name')).split('.')[0]
        assert slot.get_editor_property('material_interface')==unreal.load_asset(e['materials'][name])
    records.append(dict(asset=e['mesh_destination'],lods=2))
folder='/Game/CitySim/Art/Parks';skeleton=None
for e in (v for v in doc['rigs'] if v['lod']==0):
    mesh=unreal.load_asset(folder+'/Meshes/'+e['name']);assert mesh
    bound=mesh.get_editor_property('skeleton');assert bound
    skeleton=skeleton or bound;assert bound==skeleton
    assert mesh.get_editor_property('physics_asset') is None
    static=unreal.load_asset(folder+'/Meshes/'+e['name'].replace('SK_','SM_'));assert static
    assert [static.get_num_triangles(i) for i in (0,1)]==[754,420]
    assert static.get_minimum_lod_for_platform('Android')==1
    records.append(dict(asset=mesh.get_path_name(),skeleton=skeleton.get_path_name(),static_fallback=static.get_path_name()))
for e in doc['clips']:
    clip=unreal.load_asset(folder+'/Animations/AS_Park_'+e['name']);assert clip
    assert clip.get_editor_property('skeleton')==skeleton
    assert abs(clip.get_editor_property('sequence_length')-e['period']/24)<.001
    records.append(dict(asset=clip.get_path_name(),duration=e['period']/24))
for e in doc['props']:
    mesh=unreal.load_asset(folder+'/Meshes/'+e['name']);assert mesh
    records.append(dict(asset=mesh.get_path_name()))
for collection in ('rigs','clips','props'):
    for e in doc[collection]:assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
for e in doc['statics']:
    for l in e['lods']:assert hashlib.sha256((ROOT/l['export']).read_bytes()).hexdigest()==l['sha256']
assert len(records)==43
(OUT/'NativePersistence.json').write_text(json.dumps(dict(assets=records,source_hashes_match=True,
    shared_skeleton_persisted=True,native_physical_bone_motion='Integration/Full.log and Parks.log; six rigs actual deformed/contact/scale checks',
    maps_saved=False,read_only=True),indent=2)+'\n')
unreal.log('CITYSIM_D83_NATIVE_PERSISTENCE_PASS 43 assets')
