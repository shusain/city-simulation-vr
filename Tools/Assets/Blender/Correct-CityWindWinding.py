"""#273: winding-only derivative of the immutable approved #168 v1.

Run with Blender background/factory-startup/disable-autoexec. Never edits the
approved source, previous exports, materials, portals or native packages.
"""
from pathlib import Path
from collections import Counter
import importlib.util
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT/'Tools/Assets/Blender'/filename)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
p = module('wind_prep', 'Prepare-CityUtilityIntegration.py')
inspect = module('wind_inspect', 'Inspect-CityUtilityNativeRecipe.py')
FLIP = {'Nacelle_Contoured_Housing', 'Nacelle_Rear_Assembly_Seam',
        'Spindle_Intentional_Connection', 'Rotor_Hub', 'Spinner_Rounded_Cone'}
OUT = ROOT/'ArtSource/Exports/WindWinding273'
BLEND = ROOT/'ArtSource/Blender/UtilityIntegration/CityWind_Winding273.blend'
MANIFEST = ROOT/'ArtSource/Manifests/city_wind_winding273.json'

def solids(obj):
    mesh=obj.data; mesh.calc_loop_triangles(); result=[]
    for group in obj.vertex_groups:
        ids={v.index for v in mesh.vertices if any(g.group==group.index for g in v.groups)}
        center=sum((mesh.vertices[i].co for i in ids),Vector())/len(ids)
        triangles=[t for t in mesh.loop_triangles if t.vertices[0] in ids]
        edges=Counter(tuple(sorted(e)) for f in mesh.polygons if f.vertices[0] in ids for e in f.edge_keys)
        assert edges and set(edges.values())=={2}, group.name
        volume=sum((mesh.vertices[t.vertices[0]].co-center).dot((mesh.vertices[t.vertices[1]].co-center).cross(mesh.vertices[t.vertices[2]].co-center))/6 for t in triangles)
        result.append(dict(component=group.name,triangles=len(triangles),signed_volume_m3=volume,
                           bounds_m=p.bounds([mesh.vertices[i].co for i in ids]),closed=True))
    return result

def corrected(source, parts, name, offset):
    old=source.data; ids=[i for part in parts for i in part['ids']]; mapping={i:k for k,i in enumerate(ids)}
    faces=[]; corners=[]; flipped=[]
    for part in parts:
        for face in part['polys']:
            loops=list(face.loop_indices)
            if part['name'] in FLIP: loops.reverse()
            faces.append([mapping[old.loops[i].vertex_index] for i in loops]); corners.append((face,loops))
            flipped.append(part['name'] in FLIP)
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata([old.vertices[i].co-Vector(offset) for i in ids],[],faces); mesh.update()
    for material in old.materials: mesh.materials.append(material)
    for face,(original,_) in zip(mesh.polygons,corners):
        face.material_index=original.material_index; face.use_smooth=original.use_smooth
    for olduv in old.uv_layers:
        uv=mesh.uv_layers.new(name=olduv.name)
        for face,(_,loops) in zip(mesh.polygons,corners):
            for li,oi in zip(face.loop_indices,loops): uv.data[li].uv=olduv.data[oi].uv
    mesh.uv_layers.active_index=old.uv_layers.active_index
    normals=[old.corner_normals[li].vector*(-1 if flip else 1) for (_,loops),flip in zip(corners,flipped) for li in loops]
    mesh.normals_split_custom_set(normals)
    obj=bpy.data.objects.new(name,mesh); bpy.context.scene.collection.objects.link(obj)
    for part in parts: obj.vertex_groups.new(name=part['name']).add([mapping[i] for i in part['ids']],1.,'REPLACE')
    # Corner-by-corner shape, UV, slot and shading proof, including unedited solids.
    for face,(original,loops),flip in zip(mesh.polygons,corners,flipped):
        assert face.material_index==original.material_index and face.use_smooth==original.use_smooth
        for vi,li,oi in zip(face.vertices,face.loop_indices,loops):
            assert (mesh.vertices[vi].co+Vector(offset)-old.vertices[old.loops[oi].vertex_index].co).length<.000003
            for uv in mesh.uv_layers: assert (uv.data[li].uv-old.uv_layers[uv.name].data[oi].uv).length==0
            expected=old.corner_normals[oi].vector*(-1 if flip else 1)
            assert mesh.corner_normals[li].vector.dot(expected)>.99999
    facts=solids(obj); assert all(x['signed_volume_m3']>0 for x in facts), facts
    return obj,facts

def main():
    assert not any(x.exists() for x in (OUT,BLEND,MANIFEST)), 'Fresh correction destination required'
    source=ROOT/'ArtSource/Generated/blender/issue-168/v1'
    protected={p.relative(x):p.digest(x) for x in source.rglob('*') if x.is_file()}
    assert p.digest(source/'asset.blend')=='a604b8d1c710da011dab0cb3a1773f0ef1093a1b91c7090b55e36fbee6150536'
    obj=p.load_source(source/'asset.blend'); parts=p.parts_of(obj,p.read(source/'validation.json')['components'])
    obj.vertex_groups.clear()
    for part in parts: obj.vertex_groups.new(name=part['name']).add(part['ids'],1.,'REPLACE')
    before=solids(obj); assert {x['component'] for x in before if x['signed_volume_m3']<0}==FLIP
    base=p.read(ROOT/'ArtSource/Manifests/city_utility_integration.json')
    wind=next(a for a in base['assets'] if a['issue']==168)
    ext=p.read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json')['wind']
    roles={'Whole':set(x['name'] for x in parts),'YawNacelle':set(ext['weather_yaw_components']),'SpinRotor':set(ext['spinning_rotor_components'])}
    offsets={'Whole':[0,0,0],'YawNacelle':ext['yaw_pivot_source_m'],'SpinRotor':ext['rotor_axle_source_m']}
    targets={'Whole':'Meshes/SM_wind_turbine_v1','YawNacelle':'Wind/SM_Wind_YawNacelle_v1','SpinRotor':'Wind/SM_Wind_SpinRotor_v1'}
    OUT.mkdir(parents=True); records=[]
    for role,names in roles.items():
        lods=[]
        for lod in wind['lods']:
            selected=[x for x in parts if x['name'] in names and x['name'] not in lod.get('omitted_components',[])]
            name=f'SM_Wind273_{role}_LOD{lod["lod"]}'
            derived,volumes=corrected(obj,selected,name,offsets[role]); path=OUT/(name+'.fbx')
            with p.checked_fbx(derived,path,[m.name for m in obj.data.materials]): pass
            facts=inspect.meshinfo(derived)
            slots=[s for s in facts['material_slots'] if facts['triangles_by_material'].get(s,0)]
            facts['material_slots']=slots
            lods.append(dict(lod=lod['lod'],export=p.relative(path),export_sha256=p.digest(path),
                             facts=facts,components=volumes,roundtrip_pass=True,**p.measure(derived)))
            derived.hide_set(True); derived.hide_render=True
        records.append(dict(key='wind_turbine' if role=='Whole' else 'wind_'+role,kind='whole' if role=='Whole' else 'wind_role',
                            issue=168,version=1,role=role,target='/Game/CitySim/Art/Utilities/ApprovedBatch/'+targets[role],
                            material_slots=slots,material_asset_key='wind_turbine',source_pivot_m=offsets[role],lods=lods))
    obj.hide_set(True); obj.hide_render=True
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    assert all(p.digest(ROOT/path)==pin for path,pin in protected.items())
    p.write(MANIFEST,dict(issue=273,source_issue=168,integration_issue=238,source_approval_unchanged=True,
                         source_files=protected,corrected_components=sorted(FLIP),original_solids=before,
                         correction='Reverse polygon corners and negate corresponding source corner normals only. Preserve positions/UVs/materials/pivots/LOD omissions; no two-sided material.',
                         corner_parity_checks_pass=True,source=p.relative(BLEND),source_sha256=p.digest(BLEND),records=records))
    print('WIND273_SOURCE_PASS: five inward solids corrected, nine FBX roundtrips, all closed components positive volume')

if __name__=='__main__': main()
