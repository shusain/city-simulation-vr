"""Import only the original factory's healthy/damaged hollow-flue meshes."""
from pathlib import Path
import json,hashlib,unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DynamicReflections'
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text());rows=[]
for e in doc['assets']:
    if e['name'] not in ('SM_LowDensity_Factory_A','SM_LowDensity_Factory_A_Damaged'):continue
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['export_sha256']
    task=unreal.AssetImportTask();package,name=e['mesh_destination'].rsplit('/',1)
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False
    ui.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
    task.options=ui;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination']);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')];assert set(slots)==set(e['slots'])
    for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(e['material_bindings'][slot]))
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
    size=mesh.get_bounding_box().max-mesh.get_bounding_box().min;assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),e['dimensions_m']))
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);rows.append(dict(name=e['name'],triangles=e['triangles'],dimensions_cm=list(size.to_tuple()),slots=slots))
(OUT/'Chimneys-Import.json').write_text(json.dumps(dict(meshes=rows,source_sha256=doc['source_sha256'],materials_catalog_unchanged=True),indent=2)+'\n')
unreal.log('CITYSIM_D104_ORIGINAL_HOLLOW_NATIVE_PASS')
