"""Reimport eight factory meshes, then read back actual native mechanical normals."""
from pathlib import Path
import hashlib,json,unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/CylinderShading'
doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text())
source=json.loads((OUT/'Source.json').read_text());expected={r['name']:r for r in source['meshes']};rows=[]
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
assert hashlib.sha256((ROOT/doc['source']).read_bytes()).hexdigest()==doc['source_sha256']
for e in doc['assets']:
    if e['category']!='Factory':continue
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['export_sha256']
    task=unreal.AssetImportTask();package,name=e['mesh_destination'].rsplit('/',1)
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False
    ui.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
    task.options=ui;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination']);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert set(slots)==set(e['slots'])
    for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(e['material_bindings'][slot]))
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
    size=mesh.get_bounding_box().max-mesh.get_bounding_box().min
    assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),e['dimensions_m']))
    vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,slots.index('IndustrialMechanical'))
    exp=expected[e['name']]['expected_mechanical_normals'];worst=1
    for v,n in zip(vertices,normals):
        # Blender's native +Y maps to Unreal -Y in the established FBX bridge.
        r=min(exp,key=lambda r:(r['position_m'][0]*100-v.x)**2+(-r['position_m'][1]*100-v.y)**2+(r['position_m'][2]*100-v.z)**2)
        pos=r['position_m'];assert (pos[0]*100-v.x)**2+(-pos[1]*100-v.y)**2+(pos[2]*100-v.z)**2<.001
        # Native render-buffer normals use packed quantization; normalize the
        # decoded vector before comparing directions to float source normals.
        length=(n.x*n.x+n.y*n.y+n.z*n.z)**.5
        dot=max((n.x*x+n.y*(-y)+n.z*z)/length for x,y,z in r['normals']);worst=min(worst,dot)
        assert dot>.999,(e['name'],v,n,dot)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    rows.append(dict(name=e['name'],triangles=len(triangles)//3,mechanical_vertices=len(vertices),minimum_normal_dot=worst,geometry_materials_unchanged=True))
(OUT/'Import.json').write_text(json.dumps(dict(meshes=rows,source_sha256=doc['source_sha256'],normal_readback=True,catalog_changed=False,materials_changed=False),indent=2)+'\n')
unreal.log('CITYSIM_D104_CYLINDER_NATIVE_PASS eight imported meshes; actual native corner normals verified')
