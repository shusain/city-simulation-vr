"""Check saved source geometry/masks/UV scope after the FBX round-trip study."""
from pathlib import Path
import bpy,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
doc=json.loads((ROOT/'ArtSource/Manifests/city_home_window_study.json').read_text())
assert hashlib.sha256((ROOT/doc['source']).read_bytes()).hexdigest()==doc['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialWindows.blend'))
before=bpy.data.objects['SM_HomeWindows_Before'];records=[]
for e in doc['assets']:
    obj=bpy.data.objects[e['name']]
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
    assert all((v.co-obj.data.vertices[v.index].co).length<1e-6 for v in before.data.vertices), 'Old geometry moved'
    ca=obj.data.color_attributes['WindowMask'];a=before.data.color_attributes['WindowMask']
    preserved=0
    for p in before.data.polygons:
        q=obj.data.polygons[p.index]
        assert list(p.vertices)==list(q.vertices)
        assert all(tuple(a.data[i].color)==tuple(ca.data[i].color) for i in p.loop_indices)
        if all(a.data[i].color[3]>.99 for i in p.loop_indices):
            assert p.material_index==q.material_index
            assert all((before.data.uv_layers.active.data[i].uv-obj.data.uv_layers.active.data[i].uv).length<1e-6 for i in p.loop_indices)
            preserved+=1
    if e['name']!='SM_HomeWindows_Before':
        assert all(all(ca.data[i].color[3]>.99 for i in p.loop_indices) for p in list(obj.data.polygons)[len(before.data.polygons):]), 'Frames would emit at night'
    records.append(dict(name=e['name'],original_vertices_unchanged=len(before.data.vertices),old_pane_mask_unchanged=True,nonwindow_geometry_uv_materials_preserved=preserved,new_frames_nonemissive=True))
(ROOT/'Artifacts/D91/SourceVerification.json').write_text(json.dumps(dict(task='D91',source_hash_unchanged=True,checks=records),indent=2)+'\n')
print('CITYSIM_D91_SOURCE_SCOPE_PASS')
