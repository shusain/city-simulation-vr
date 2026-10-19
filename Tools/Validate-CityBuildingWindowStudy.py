"""Validate saved D91 source/export scope; no Unreal import or promotion."""
from pathlib import Path
import bpy,json,hashlib,sys,bmesh
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingWindows import select_panes,base_name
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_window_study.json').read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert doc['blender_review_only'] and not doc['production_imported']
assert len(doc['assets'])==51 and doc['normal_candidates']==50 and doc['damaged_candidates']==1
assert sum(e['window_count'] for e in doc['assets'])==1641
assert digest(ROOT/doc['source'])==doc['source_sha256']
assert all(digest(ROOT/p)==h for p,h in doc['protected'].items()),'Production inputs changed'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']))
assert all(image.packed_file for image in bpy.data.images if image.source=='FILE'), 'Editable source must contain its actual image dependencies'
validated=[];original_sources={}
for entry in doc['assets']:
    obj=bpy.data.objects[entry['mesh_destination'].rsplit('/',1)[1]];mesh=obj.data;mesh.calc_loop_triangles()
    assert len(mesh.loop_triangles)==entry['triangles'] and len(mesh.materials)==entry['sections'] and entry['roundtrip_pass']
    assert digest(ROOT/entry['export'])==entry['sha256']
    assert all(abs(a-b)<.002 for a,b in zip(obj.dimensions,entry['dimensions_m']))
    attr=mesh.color_attributes['WindowMask'];panes=[p for p in mesh.polygons if base_name(mesh.materials[p.material_index]) in ('BuildingWindowOpaque','HomeWindowOpaque') and all(attr.data[i].color[3]<.01 for i in p.loop_indices)]
    assert len(panes)==entry['window_count']
    if panes:
        material=mesh.materials[panes[0].material_index]
        shader=material.node_tree.nodes.get('Principled BSDF')
        assert shader.inputs['Alpha'].default_value==1 and not shader.inputs['Alpha'].is_linked
        assert shader.inputs['Transmission Weight'].default_value==0
        assert any(n.type=='TEX_IMAGE' and list(n.image.size)==[128,128] for n in material.node_tree.nodes)
    if not entry.get('home_scale_adopted'):
        before=bpy.data.objects[obj.name+'_Before'];old=before.data;panes_old,window_faces=select_panes(before,entry['asset_id'])
        assert panes_old==entry['pane_faces'] and sorted(window_faces)==entry['window_box_faces']
        assert len(mesh.polygons)==len(old.polygons)+16*entry['window_count']
        assert [tuple(v.co) for v in mesh.vertices[:len(old.vertices)]]==[tuple(v.co) for v in old.vertices]
        for p in old.polygons:
            assert tuple(mesh.polygons[p.index].vertices)==tuple(p.vertices)
            if p.index in set(panes_old)|window_faces:continue
            assert mesh.polygons[p.index].material_index==p.material_index
            for li in p.loop_indices:
                if old.uv_layers.active:assert (mesh.uv_layers.active.data[li].uv-old.uv_layers.active.data[li].uv).length<1e-6
                original_color=old.color_attributes.get('WindowMask')
                expected=tuple(original_color.data[li].color) if original_color else (1,1,1,1)
                assert tuple(attr.data[li].color)==expected
        # Added frames stay opaque/non-emissive; neither glass UVs nor tilted
        # pane geometry move the source wall or any other architectural detail.
        assert all(attr.data[li].color[3]>.99 for p in mesh.polygons[len(old.polygons):] for li in p.loop_indices)
        for p in panes:
            assert all(-1e-5<=c<=1.00001 for li in p.loop_indices for c in mesh.uv_layers.active.data[li].uv)
    else:
        assert entry['asset_id'] in ('building.residential.a','SM_Building_Residential_SeasonalSpace_A','building.residential.a.damaged')
        assert abs(entry['dimensions_m'][0]-10.56)<.001 and abs(entry['dimensions_m'][1]-13.28)<.001
        if entry.get('damaged_scale_only'):
            assert all(v.color[3]>.99 for v in attr.data) and entry['triangles']==668
            intact=bpy.data.objects['SM_Building_Residential_A'].data
            assert [tuple(sorted(p.vertices)) for p in mesh.polygons]==[tuple(sorted(p.vertices)) for p in intact.polygons]
            bm=bmesh.new();bm.from_mesh(mesh);assert all(f.calc_area()>1e-8 for f in bm.faces);boundary=sum(e.is_boundary for e in bm.edges);bm.free()
            bm=bmesh.new();bm.from_mesh(intact);assert boundary==sum(e.is_boundary for e in bm.edges);bm.free()
        else:assert entry['triangles']==668 and entry['sections']==4 and abs(entry['dimensions_m'][2]-5.8)<.001
    assert digest(ROOT/entry['source_entry']['export'])==entry['source_export_sha256']
    validated.append(dict(asset_id=entry['asset_id'],triangles=entry['triangles'],sections=entry['sections'],windows=entry['window_count'],roundtrip_pass=True,nonwindow_preserved=entry['nonwindow_source_preserved']))
# The roof-light beacons in six stations/two hospitals are excluded from
# architectural pane selection and covered by exact nonwindow loop checks.
assert len({e['mesh_destination'] for e in doc['assets']})==51
result=dict(task='D91',assets=validated,source_verified=True,exports=51,normal=50,damaged_scale=1,architectural_panes=1641,production_inputs_preserved=len(doc['protected']),beacon_and_vehicle_geometry_rgba_uv_material_preserved=True,other_damaged_assets_preserved=True,unreal_imported=False)
(ROOT/'Artifacts/D91/Buildings/SourceVerification.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D91_BUILDING_WINDOWS_SOURCE_PASS 51 exports;1641 panes;production source/assets preserved')
