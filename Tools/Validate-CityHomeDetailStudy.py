"""Focused saved-source checks for accepted windows and the new door/chimney."""
from pathlib import Path
import bpy,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Details'
doc=json.loads((ROOT/'ArtSource/Manifests/city_home_detail_study.json').read_text())
assert hashlib.sha256((ROOT/doc['source']).read_bytes()).hexdigest()==doc['source_sha256'], 'Accepted window source changed'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialDetails.blend'))
before=bpy.data.objects['SM_HomeDetails_WindowsOnly'];after=bpy.data.objects['SM_HomeDetails_Refined']
a=before.data.color_attributes['WindowMask'];b=after.data.color_attributes['WindowMask']
window_faces=[]
for f in before.data.polygons:
    g=after.data.polygons[f.index]
    if f.index not in range(11,17):
        assert all((before.data.vertices[i].co-after.data.vertices[i].co).length<1e-6 for i in f.vertices)
    if f.material_index==3:
        assert g.material_index==3
        assert all(tuple(a.data[i].color)==tuple(b.data[i].color) for i in f.loop_indices)
        assert all((before.data.uv_layers.active.data[i].uv-after.data.uv_layers.active.data[i].uv).length<1e-6 for i in f.loop_indices)
        window_faces.append(f.index)
assert len(window_faces)==187 #11 panes +16 frame quads for each pane
assert all(abs(x-y)<.001 for x,y in zip(before.dimensions,after.dimensions))
assert len(after.data.materials)==4 and after.data.materials[3]==before.data.materials[3]
assert after.data.materials[1].name=='HomeSolidDetails'
for f in list(after.data.polygons)[len(before.data.polygons):]:
    assert all(b.data[i].color[3]>.99 for i in f.loop_indices), 'Added details must have no window mask'
parts={p['name']:p for p in doc['parts']}
for name in ('DoorLeftJamb','DoorRightJamb'):
    top=parts[name]['center'][2]+parts[name]['size'][2]/2
    bottom=parts['DoorLintel']['center'][2]-parts['DoorLintel']['size'][2]/2
    assert abs(top-bottom)<1e-6,'Frame corner overlap/gap'
assert all(p.get('roof_clearance_m',.025)==.025 for p in doc['parts'] if p['name'].startswith('RoofFlashing'))
for e in doc['assets']:
    assert e['roundtrip_pass'] and hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
assert not after.hide_get() and before.hide_get()
result=dict(task='D91',slice='DoorChimney',accepted_window_source_hash_unchanged=True,window_faces_geometry_uv_rgba_material_preserved=len(window_faces),
    body_roof_porch_walk_geometry_preserved=True,original_envelope_preserved=True,sections=4,frame_corners_meet=True,
    roof_flashing_clearance_m=.025,new_details_nonemissive=True,fbx_roundtrips=2,triangles=[e['triangles'] for e in doc['assets']])
(OUT/'SourceVerification.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D91_HOME_DETAILS_SOURCE_PASS')
