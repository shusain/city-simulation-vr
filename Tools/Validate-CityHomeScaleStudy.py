"""D91 shared brick-repeat, doorway clearance, citizen-height and export checks."""
from pathlib import Path
import bpy,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Scale'
doc=json.loads((ROOT/'ArtSource/Manifests/city_home_scale_study.json').read_text())
assert hashlib.sha256((ROOT/doc['source']).read_bytes()).hexdigest()==doc['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialScale.blend'))
before=bpy.data.objects['SM_HomeScale_Before'];after=bpy.data.objects['SM_HomeScale_Refined'];mesh=after.data
def bounds(indices):
    points=[mesh.vertices[i].co for fi in indices for i in mesh.polygons[fi].vertices]
    return [min(c[i] for c in points) for i in range(3)],[max(c[i] for c in points) for i in range(3)]
low,high=bounds(range(23,29));assert abs(high[0]-low[0]-1.1)<1e-5 and abs(high[2]-low[2]-1.85)<1e-5
low,high=bounds(range(6));assert abs(high[2]-3)<1e-5 and abs(high[0]-low[0]-9.24)<1e-5
low,high=bounds(range(266,272));assert abs((low[2]+high[2])/2-.85)<1e-5
windows=[]
for wi in range(11):
    f=mesh.polygons[29+wi];points=[mesh.vertices[i].co for i in f.vertices]
    assert abs(min(c.z for c in points)-.75)<1e-5
    assert abs(max(c.z for c in points)-2.05)<1e-5
    assert abs((points[1]-points[0]).length-1.4)<1e-5
    windows.append(f.index)
for f in before.data.polygons:
    if f.material_index==3:
        g=mesh.polygons[f.index];assert g.material_index==3
        assert all(tuple(before.data.color_attributes['WindowMask'].data[i].color)==tuple(mesh.color_attributes['WindowMask'].data[i].color) for i in f.loop_indices)
        assert all((before.data.uv_layers.active.data[i].uv-mesh.uv_layers.active.data[i].uv).length<1e-6 for i in f.loop_indices)
for fi in list(range(6))+list(range(11,17)):
    f=mesh.polygons[fi];axis=max(range(3),key=lambda i:abs(f.normal[i]));sign=1 if f.normal[axis]>0 else -1
    for li in f.loop_indices:
        c=mesh.vertices[mesh.loops[li].vertex_index].co
        a,b=(sign*c.y,c.z) if axis==0 else (-sign*c.x,c.z) if axis==1 else (sign*c.x,c.y)
        uv=mesh.uv_layers.active.data[li].uv;assert abs(uv.x-a/2.4)<1e-5 and abs(uv.y-b/2.4)<1e-5
assert max(abs(c) for v in mesh.vertices for c in (v.co.x,v.co.y))<=8.001
street,park=doc['citizens']
assert abs(street['height_after_m']-1.43175)<1e-4 and abs(street['height_after_m']-park['height_after_m'])<.04
assert abs(doc['street_origin_offset_cm']+12.5*street['scale_after']-104)<1e-6
for e in doc['assets']:
    assert e['roundtrip_pass'] and e['triangles']==668 and e['sections']==4
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
for e in doc['citizens']:assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
(OUT/'SourceVerification.json').write_text(json.dumps(dict(task='D91',slice='CitizenScale',source_inputs_preserved=True,
    matching_wall_chimney_repeat_m=2.4,door_m=[1.1,1.85],door_head_clearance_m=1.85-street['height_after_m'],knob_height_m=.85,
    window_sill_m=.75,window_m=[1.4,1.3],eaves_m=3,street_citizen_height_m=street['height_after_m'],park_posed_reference_height_m=park['height_after_m'],
    street_feet_support_preserved_cm=104,logical_lot_preserved_m=[16,16],window_surface_masks_uv_material_preserved=True,roundtrips=2,triangles=668,sections=4),indent=2)+'\n')
print('CITYSIM_D91_SCALE_SOURCE_PASS')
