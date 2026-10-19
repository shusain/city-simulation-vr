"""Normalize accepted D71 static citizens to the unchanged baseline walker bounds."""
from pathlib import Path
import hashlib,json
import bpy

root=Path(__file__).resolve().parents[1]
source=json.loads((root/'ArtSource/Manifests/city_citizen_prototype.json').read_text())
out=root/'ArtSource/Exports/CitizenPresentation';out.mkdir(parents=True,exist_ok=True)

def load(path):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(path))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    return obj

baseline=load(root/'ArtSource/Exports/SM_Citizen_A.fbx')
low=min(v.co.z for v in baseline.data.vertices);high=max(v.co.z for v in baseline.data.vertices)
records=[]
for e in source['assets']:
    path=root/e['export'];obj=load(path)
    original=[tuple(v.co) for v in obj.data.vertices]
    attr=obj.data.color_attributes.active_color
    colors=[tuple(c.color) for c in attr.data]
    before=[tuple(p.vertices) for p in obj.data.polygons]
    original_low=min(v[2] for v in original);original_height=max(v[2] for v in original)-original_low
    factor=(high-low)/original_height
    for vertex,coord in zip(obj.data.vertices,original):
        vertex.co=(coord[0]*factor,coord[1]*factor,(coord[2]-original_low)*factor+low)
    target=[tuple(v.co) for v in obj.data.vertices]
    assert [tuple(p.vertices) for p in obj.data.polygons]==before
    assert [tuple(c.color) for c in attr.data]==colors
    obj.name=e['name'];obj.location=(0,0,0)
    dest=out/(e['name']+'.fbx')
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',
        mesh_smooth_type='FACE',bake_anim=False,add_leaf_bones=False)
    # FBX can reorder corners. Compare position/RGBA tuples instead of indices.
    def colored_corners(obj):
        attr=obj.data.color_attributes.active_color
        return sorted((tuple(round(c,5) for c in obj.data.vertices[obj.data.loops[i].vertex_index].co),
                       tuple(round(c,3) for c in attr.data[i].color)) for p in obj.data.polygons for i in p.loop_indices)
    expected=sorted((tuple(round(c,5) for c in target[obj.data.loops[i].vertex_index]),
                     tuple(round(c,3) for c in colors[i])) for p in obj.data.polygons for i in p.loop_indices)
    check=load(dest);check.data.calc_loop_triangles()
    assert len(check.data.loop_triangles)==e['triangles']
    assert colored_corners(check)==expected,'Normalized citizen geometry/RGBA changed in FBX roundtrip'
    records.append(dict(name=e['name'],source_export=e['export'],source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                        export=dest.relative_to(root).as_posix(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),
                        triangles=e['triangles'],uniform_scale=factor,bounds_min_z_m=low,bounds_max_z_m=high,roundtrip_pass=True))
manifest=dict(task='D71',baseline_min_z_m=low,baseline_max_z_m=high,assets=records,
              runtime_policy='Windows Desktop/PCVR refined pools; Android baseline; original traffic and park transforms unchanged',
              lod_screen_sizes=[1.0,.025],animation='Static poses translated by existing pooled presentation; no gait animation')
(root/'ArtSource/Manifests/city_citizen_presentation.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('CITYSIM_CITIZEN_NATIVE_EXPORT_PASS',len(records),'baseline',low,high)
