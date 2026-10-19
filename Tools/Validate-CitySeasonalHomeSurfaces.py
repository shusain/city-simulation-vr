"""Round-trip the surfaced home against its accepted lawn/wreath source."""
from pathlib import Path
import bpy,json
root=Path(__file__).resolve().parents[1]
entry=next(e for e in json.loads((root/'ArtSource/Manifests/city_seasonal_kit.json').read_text())['assets'] if e['name']=='SM_Building_Residential_SeasonalSpace_A')
def read(path):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(root/path))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    colors=obj.data.color_attributes.get('WindowMask');assert colors
    faces=[]
    for p in obj.data.polygons:
        corners=sorted((tuple(round(x,4) for x in obj.data.vertices[obj.data.loops[i].vertex_index].co),tuple(round(x,4) for x in colors.data[i].color)) for i in p.loop_indices)
        faces.append(corners)
    obj.data.calc_loop_triangles()
    return obj,sorted(faces),len(obj.data.loop_triangles)
_,before,old_triangles=read(entry['surface_source_entry']['export'])
obj,after,triangles=read(entry['export'])
assert before==after,'Seasonal home geometry or window RGBA changed in FBX'
assert triangles==old_triangles==entry['triangles']
assert [m.name.split('.')[0] for m in obj.data.materials]==list(entry['material_slots'])
assert min(v.co.z for v in obj.data.vertices)<-.199
assert obj.data.uv_layers.active
report=dict(roundtrip_pass=True,triangles=triangles,sections=len(obj.data.materials),geometry_and_rgba_unchanged=True,driveway_min_z_m=min(v.co.z for v in obj.data.vertices))
(root/'Artifacts/D79/Integration/seasonal-roundtrip.json').write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D79_SEASONAL_ROUNDTRIP_PASS')
