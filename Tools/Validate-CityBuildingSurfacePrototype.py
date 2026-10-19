"""Round-trip the surface FBXs: geometry, UVs, section identity and vertex masks."""
from pathlib import Path
import bpy, hashlib, json, math, sys
from mathutils import Vector
from collections import Counter

root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'Tools'))
from CityBuildingSurfaceGeometry import components
manifest = json.loads((root/'ArtSource/Manifests/city_building_surface_prototype.json').read_text(encoding='utf-8'))
report = []
assert len(manifest['assets'])==18
assert {(e['category'],e['density'],e['damaged']) for e in manifest['assets']}=={
    (category,density,damaged) for category in ('Residential','Commercial','Industrial')
    for density in ('Low','Medium','High') for damaged in (False,True)}

def load(path):
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(meshes)==1
    obj=meshes[0]; bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    obj.data.calc_loop_triangles()
    return obj

def faces(obj, mask=False):
    attr=obj.data.color_attributes.get('WindowMask') or obj.data.color_attributes[0]
    result=[]
    for p in obj.data.polygons:
        result.append(tuple(sorted((tuple(round(c,5) for c in obj.data.vertices[obj.data.loops[i].vertex_index].co),
                                   round(attr.data[i].color[3],3) if mask else 0) for i in p.loop_indices)))
    return sorted(result)

for e in manifest['assets']+manifest.get('sign_variants',[]):
    input_path=root/e['input_geometry_export']
    assert hashlib.sha256(input_path.read_bytes()).hexdigest()==e['input_geometry_sha256']
    refinement=e['geometry_refinement']
    refinement_checks={}
    preserved=[]
    if refinement:
        original_input=load(input_path)
        edited=set(refinement['edited_source_vertices'])
        assert len(original_input.data.vertices)==refinement['source_vertex_count']
        assert len(original_input.data.polygons)==refinement['source_face_count']
        attr=original_input.data.color_attributes.get('WindowMask')
        for p in original_input.data.polygons:
            if not edited.intersection(p.vertices):
                preserved.append(tuple(sorted((tuple(round(c,5) for c in original_input.data.vertices[original_input.data.loops[i].vertex_index].co),round(attr.data[i].color[3],3)) for i in p.loop_indices)))
    accepted_path=root/e['accepted_geometry_export']
    assert hashlib.sha256(accepted_path.read_bytes()).hexdigest()==e['accepted_geometry_sha256']
    original=load(accepted_path); geometry=faces(original); alpha=faces(original,True)
    assert not(Counter(preserved)-Counter(alpha)), 'An unrelated source face changed'
    original_triangles=len(original.data.loop_triangles)
    obj=load(root/e['export'])
    assert geometry==faces(obj) and alpha==faces(obj,True)
    assert len(obj.data.loop_triangles)==original_triangles==e['triangles']
    assert len(obj.data.materials)==e['sections']
    assert {m.name.split('.')[0] for m in obj.data.materials}==set(e['material_slots'])
    assert all(abs(a-b)<.0001 for a,b in zip(obj.dimensions,e['dimensions_m']))
    assert abs(min(v.co.z for v in obj.data.vertices))<.00001
    colors=obj.data.color_attributes.get('WindowMask') or obj.data.color_attributes[0]
    metal_values=set()
    for p in obj.data.polygons:
        if p.material_index==e.get('roof_sign_detail',{}).get('mechanical_slot',2):
            for i in p.loop_indices:
                value=colors.data[i].color
                assert value[3]>.999 and min(abs(value[0]),abs(value[0]-1))<.001
                metal_values.add(round(value[0]))
    assert metal_values==set(e['mechanical_mask_values'])
    uv=obj.data.uv_layers.active
    assert uv and all(math.isfinite(c) for item in uv.data for c in item.uv)
    for triangle in obj.data.loop_triangles:
        a,b,c=(uv.data[i].uv for i in triangle.loops)
        assert abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))>1e-8, 'Collapsed texture triangle'
    detail=e.get('roof_sign_detail')
    historical_surface_check=None
    if detail:
        roof_faces=[p for p in obj.data.polygons if p.material_index==detail['roof_slot']]
        assert len(roof_faces)==len(detail['roof_faces'])
        assert all(colors.data[i].color[3]>.999 for p in roof_faces for i in p.loop_indices)
        if not e['damaged']:
            for p in roof_faces:
                assert p.normal.z>.2
                tangent=p.normal.cross(Vector((1,0,0))).normalized()
                for i in p.loop_indices:
                    c=obj.data.vertices[obj.data.loops[i].vertex_index].co
                    assert (uv.data[i].uv-Vector((c.x/detail['roof_tile_m'],c.dot(tangent)/detail['roof_tile_m']))).length<.0001
        if 'sign_slot' in detail:
            signs=[p for p in obj.data.polygons if p.material_index==detail['sign_slot']]
            assert len(signs)==len(detail['sign_faces'])
            index=detail['sign_atlas_index'];x=index%4/4;y=1-(index//4+1)/2
            assert all(x<uv.data[i].uv.x<x+.25 and y<uv.data[i].uv.y<y+.5 for p in signs for i in p.loop_indices)
            assert all(colors.data[i].color[3]>.999 for p in signs for i in p.loop_indices)
        # Roof/sign materials and UVs are the entire scope of this slice. All
        # other surfaces must retain their accepted material, UVs and RGBA.
        def surface_faces(mesh,excluded=()):
            uv=mesh.data.uv_layers.active;attr=mesh.data.color_attributes.get('WindowMask')
            result=Counter()
            for p in mesh.data.polygons:
                if p.material_index in excluded:continue
                key=tuple(sorted(tuple(round(c,5) for c in mesh.data.vertices[v].co) for v in p.vertices))
                payload=tuple(sorted((tuple(round(c,5) for c in mesh.data.vertices[mesh.data.loops[i].vertex_index].co),
                    tuple(round(c,5) for c in uv.data[i].uv),tuple(round(c,3) for c in attr.data[i].color)) for i in p.loop_indices))
                result[(key,mesh.data.materials[p.material_index].name.split('.')[0],payload)]+=1
            return result
        untouched=surface_faces(obj,{detail['roof_slot'],detail.get('sign_slot')})
        if 'base_asset_id' in e:
            all_except_sign=surface_faces(obj,{detail['sign_slot']})
            base=next(a for a in manifest['assets'] if a['asset_id']==e['base_asset_id'])
            reference=load(root/base['export'])
            assert all_except_sign==surface_faces(reference,{base['roof_sign_detail']['sign_slot']}), 'Business variant changed another surface'
        keys={key for key,_,_ in untouched}
        snapshot=root/'Artifacts/D79/RoofsSigns/Before'/Path(e['export']).name
        if 'base_asset_id' in e:
            base=next(a for a in manifest['assets'] if a['asset_id']==e['base_asset_id'])
            snapshot=snapshot.with_name(Path(base['export']).name)
        # Historical pre-change artifacts are optional on a fresh checkout.
        # Never report a historical comparison as passed when it was absent;
        # accepted-reference geometry/masks and variant guards remain required.
        if snapshot.exists():
            before=load(snapshot)
            assert faces(before,True)==alpha, 'Roof/sign slice changed geometry or window alpha'
            prior=surface_faces(before)
            assert untouched==Counter({key:value for key,value in prior.items() if key[0] in keys}), 'Unrelated surface changed'
            historical_surface_check=True
        obj=load(root/e['export']);colors=obj.data.color_attributes.get('WindowMask');uv=obj.data.uv_layers.active
    # The intact tower's vertical UV repeat is exactly 2.4m; window quads do
    # not sample brick and are intentionally excluded from this invariant.
    if 'damaged' not in e['asset_id']:
        for p in obj.data.polygons:
            if p.material_index==0 and not(e['category']=='Residential' and e['density']=='High'):
                assert abs(p.normal.z)<.001, 'Roof cap received masonry'
            if e['category']=='Industrial' and p.material_index==0:
                assert abs(p.normal.z)<.001, 'Industrial roof/equipment received masonry'
                assert max(obj.data.vertices[v].co.z for v in p.vertices)<={'Low':7,'Medium':12,'High':17}[e['density']]+.001
            if p.material_index==0 and abs(p.normal.z)<.001:
                for i in p.loop_indices:
                    z=obj.data.vertices[obj.data.loops[i].vertex_index].co.z
                    assert abs(uv.data[i].uv.y-z/2.4)<.0001
        if refinement and e['category']=='Commercial':
            parts=components(obj)
            sign=[p for p in parts if all(abs(a-b)<.001 for a,b in zip(p['size'],(7,1,3)))]
            assert len(sign)==1 and abs(sign[0]['low'][2]-10)<.001
            assert sign[0]['low'][1]>-6 and sign[0]['high'][1]<-4, 'Sign extends beyond its podium'
            refinement_checks=dict(sign_seated_on_roof=True,sign_roof_edge_inset_m=sign[0]['low'][1]+7)
        if refinement and e['category']=='Industrial':
            roof=13 if e['density']=='Medium' else 18
            roof_parts=[p for p in components(obj) if p['low'][2]>=roof-.001 and p['size'][2]>.1]
            assert len(roof_parts)==(6 if e['density']=='Medium' else 7)
            clearances=[]
            for i,a in enumerate(roof_parts):
                assert a['low'][0]>=-8 and a['high'][0]<=8 and a['low'][1]>=-7.5 and a['high'][1]<=7.5
                assert abs(a['low'][2]-roof)<.001, 'Equipment is not seated on roof'
                for b in roof_parts[i+1:]:
                    gap=max(b['low'][axis]-a['high'][axis] for axis in (0,1))
                    gap=max(gap,max(a['low'][axis]-b['high'][axis] for axis in (0,1)))
                    assert gap>=.49, 'Roof equipment overlaps or lacks clearance'
                    clearances.append(gap)
            refinement_checks=dict(min_roof_equipment_clearance_m=min(clearances),equipment_seated_on_roof=True)
            # Twelve tilted glass panes plus twelve fixed dark openings; the
            # other window faces are the newly added vertical rows.
            glass=[p for p in obj.data.polygons if p.material_index==1 and all(colors.data[i].color[3]<.001 for i in p.loop_indices)]
            tilted=[p for p in glass if abs(p.normal.z)>.1]
            assert len(tilted)==12 and all(len(p.vertices)==4 for p in tilted)
            assert all(abs(abs(p.normal.z)-math.sin(math.radians(20)))<.001 for p in tilted)
            assert len(glass)==24+refinement['added_facade_windows'], 'Unexpected industrial window coverage'
            refinement_checks.update(open_upper_windows=len(tilted),open_angle_degrees=20,added_facade_windows=refinement['added_facade_windows'])
            # The new front windows stay beside or above the loading bay.
            for p in glass:
                coords=[obj.data.vertices[i].co for i in p.vertices]
                if all(abs(v.y+7.035)<.001 for v in coords) and min(v.z for v in coords)<(6 if e['density']=='Medium' else 7):
                    assert min(v.x for v in coords)>=4.39 or max(v.x for v in coords)<=-4.39
    report.append(dict(asset_id=e['asset_id'],sha256=hashlib.sha256((root/e['export']).read_bytes()).hexdigest(),
                       triangles=e['triangles'],sections=e['sections'],geometry_unchanged=not bool(refinement),
                       geometry_matches_reference=True,geometry_refinement=refinement,unrelated_source_faces_preserved=len(preserved),window_alpha_unchanged=True,
                       refinement_checks=refinement_checks,
                       roof_sign_detail=detail,unrelated_surface_uv_rgba_unchanged=historical_surface_check,
                       metal_mask_values=sorted(metal_values),uv_valid=True,roundtrip_pass=True))
path=root/'Artifacts/D79/surfaces-roundtrip.json'
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('CITYSIM_BUILDING_SURFACE_ROUNDTRIP_PASS',len(report))
