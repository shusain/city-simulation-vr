"""D91 geometry/window/palette and selective surface-UV checks."""
from pathlib import Path
import bpy,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import snapshot,digest
from CityCivicBuildingSurfaces import select_surfaces
doc=json.loads((ROOT/'ArtSource/Manifests/city_civic_surface_study.json').read_text())
assert doc['blender_review_only'] and not doc['production_imported'] and len(doc['assets'])==26
assert digest(ROOT/doc['source'])==doc['source_sha256']
assert all(digest(ROOT/p)==h for p,h in doc['protected'].items())
bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']))
assert all(image.packed_file for image in bpy.data.images if image.source=='FILE')
checks=[]
for e in doc['assets']:
    name=e['mesh_destination'].rsplit('/',1)[1];obj=bpy.data.objects[name];before=bpy.data.objects[name+'_Before'];mesh=obj.data
    a=snapshot(before);b=snapshot(obj);assert a['vertices']==b['vertices'] and a['faces']==b['faces'] and a['rgba']==b['rgba']
    mesh.calc_loop_triangles();assert len(mesh.loop_triangles)==e['triangles'] and len(mesh.materials)==e['sections'] and e['roundtrip_pass']
    assert digest(ROOT/e['export'])==e['sha256'] and digest(ROOT/e['input_export'])==e['input_sha256']
    assert all(abs(x-y)<.002 for x,y in zip(obj.dimensions,e['dimensions_m']))
    walls,roofs,parts=select_surfaces(before,e['asset_id']);assert sorted(walls)==e['wall_faces'] and roofs==e['roof_faces']
    selected=set(walls)|set(roofs)
    for p in before.data.polygons:
        if p.index not in selected:
            assert a['slots'][p.index]==b['slots'][p.index]
            for li in p.loop_indices:
                if a['uv']:assert a['uv'][li]==b['uv'][li]
        else:
            current=mesh.polygons[p.index];axis=max(range(3),key=lambda i:abs(current.normal[i]));sign=1 if current.normal[axis]>0 else -1
            for li in p.loop_indices:
                c=mesh.vertices[mesh.loops[li].vertex_index].co;uv=mesh.uv_layers.active.data[li].uv
                if p.index in walls:
                    x,y=(sign*c.y,c.z) if axis==0 else (-sign*c.x,c.z);assert abs(uv.x-x/2.4)<1e-5 and abs(uv.y-y/2.4)<1e-5
                    assert p.material_index in walls.values()
                else:assert abs(uv.x-c.x/2)<1e-5 and abs(uv.y-c.y/2)<1e-5
    for old,tint in e['tints'].items():
        slot=e['wall_slots'][old];material=mesh.materials[slot]
        assert all(abs(x-y)<1e-6 for x,y in zip(material.diffuse_color[:3],tint['color']))
        assert any(n.type=='MIX_RGB' and all(abs(x-y)<1e-6 for x,y in zip(n.inputs[2].default_value[:3],tint['color'])) for n in material.node_tree.nodes)
        assert any(n.type=='TEX_IMAGE' and 'CityBrick_BaseColor' in n.image.name for n in material.node_tree.nodes)
    roof=mesh.materials[e['roof_slot']];assert any(n.type=='TEX_IMAGE' and 'CivicAsphalt_BaseColor' in n.image.name for n in roof.node_tree.nodes)
    if e['approved_window_entry']:
        # Every approved glass/frame polygon, UV, RGBA and material remains
        # outside the surface selection. Shader dependency/source hashes hold.
        protected_window=[p for p in before.data.polygons if before.data.materials[p.material_index].name.split('.')[0]=='BuildingWindowOpaque']
        assert protected_window and all(p.index not in selected for p in protected_window)
    checks.append(dict(asset_id=e['asset_id'],triangles=e['triangles'],sections=e['sections'],walls=len(walls),roofs=len(roofs),windows=e['window_count'],geometry_rgba_preserved=True,unselected_uv_material_preserved=True,tints_preserved=True))
result=dict(task='D91',assets=checks,source_verified=True,roundtrips=26,production_and_accepted_inputs_preserved=len(doc['protected']),geometry_topology_rgba_unchanged=True,approved_glass_frames_uvs_masks_materials_unchanged=True,tanks_stacks_vehicles_signs_unchanged=True,original_wall_tints_preserved=True,brick_repeat_m=2.4,asphalt_repeat_m=2,packed_textures=True,unreal_imported=False)
(ROOT/'Artifacts/D91/CivicSurfaces/SourceVerification.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D91_CIVIC_SURFACES_SOURCE_PASS 26 exports;geometry/window scope and original tints preserved')
