"""Normalize accepted utility FBX units for Unreal's automated legacy factory.

Keeps the authoritative root FBXs. Geometry, UVs and vertex RGBA are checked
after reimport; only FBX unit representation and the main export path change.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import bpy,json
root=Path(__file__).resolve().parents[1]
path=root/'ArtSource/Manifests/city_starter_kit.json';doc=json.loads(path.read_text())
out=root/'ArtSource/Exports/BuildingRefinementNative';out.mkdir(parents=True,exist_ok=True)
def read(file):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(file))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    colors=obj.data.color_attributes.get('WindowMask');uv=obj.data.uv_layers.active
    faces=sorted(sorted((tuple(round(x,4) for x in obj.data.vertices[obj.data.loops[i].vertex_index].co),
                         tuple(round(x,4) for x in colors.data[i].color),tuple(round(x,4) for x in uv.data[i].uv)) for i in p.loop_indices) for p in obj.data.polygons)
    return obj,faces
report=[]
for entry in doc['assets']:
    if not has_capability(entry, 'building_refinement') or not entry['asset_id'].startswith(('building.power.','building.service.')):continue
    original=entry.get('native_export_source',entry['export'])
    obj,before=read(root/original)
    for i,m in enumerate(bpy.data.materials):m.name='_UtilityNative'+str(i)
    for m,name in zip(obj.data.materials,entry['material_slots']):m.name=name
    export=out/Path(original).name
    bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
    bpy.ops.export_scene.fbx(filepath=str(export),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
        global_scale=1,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
    obj,after=read(export);assert before==after,entry['asset_id']
    obj.data.calc_loop_triangles();assert len(obj.data.loop_triangles)==entry['triangles']
    assert [m.name.split('.')[0] for m in obj.data.materials]==list(entry['material_slots'])
    entry.update(native_export_source=original,export=export.relative_to(root).as_posix())
    report.append(dict(asset_id=entry['asset_id'],geometry_uv_rgba_unchanged=True,triangles=entry['triangles']))
assert len(report)==12
path.write_text(json.dumps(doc,indent=2)+'\n')
(root/'Artifacts/D79/Integration/utility-native-roundtrip.json').write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D79_UTILITY_NATIVE_ROUNDTRIP_PASS 12')
