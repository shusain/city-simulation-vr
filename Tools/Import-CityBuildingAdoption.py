"""Exact D91 reviewed asset import into the main project; no catalog/map writes."""
from pathlib import Path
import json,hashlib,sys,traceback,unreal
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingAdoptionMaterials import build
OUT=ROOT/'Artifacts/D91/Integration';OUT.mkdir(parents=True,exist_ok=True)
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_adoption.json').read_text());assert len(doc['assets'])==61
surfaces=json.loads((ROOT/'ArtSource/Manifests/city_civic_surface_study.json').read_text())
for e in doc['assets']:assert digest(ROOT/e['export'])==e['export_sha256']
for info in doc['textures'].values():
    for t in info['maps'].values():assert digest(ROOT/t['file'])==t['sha256']
assert digest(ROOT/doc['glass_texture'])==doc['glass_sha256']
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
materials=build(ROOT,doc,surfaces);records=[]
tools=unreal.AssetToolsHelpers.get_asset_tools();subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
for e in doc['assets']:
    # Legacy atomic reimport reads the persisted mesh policy, overriding the
    # task UI. Replace old painted colors just as the established D79 pipeline does.
    existing=unreal.load_asset(e['mesh_destination'])
    if existing:
        old_data=existing.get_editor_property('asset_import_data')
        if isinstance(old_data,unreal.FbxStaticMeshImportData):old_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
    task=unreal.AssetImportTask();package,name=e['mesh_destination'].rsplit('/',1)
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
    options.set_editor_property('reset_to_fbx_on_material_conflict',True)
    data=options.static_mesh_import_data
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,import_uniform_scale=1.,generate_lightmap_u_vs=True,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    task.options=options;tools.import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination']);assert mesh
    imported_data=mesh.get_editor_property('asset_import_data');assert isinstance(imported_data,unreal.FbxStaticMeshImportData)
    if imported_data.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
        imported_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE);tools.import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination'])
    assert mesh.get_editor_property('asset_import_data').get_editor_property('vertex_color_import_option')==unreal.VertexColorImportOption.REPLACE
    imported=[str(m.get_editor_property('imported_material_slot_name')) for m in mesh.get_editor_property('static_materials')]
    assert len(imported)==len(e['material_slots']) and set(imported)==set(e['material_slots']),(e['asset_id'],imported)
    for i,slot in enumerate(imported):
        mat=unreal.load_asset(e['material_slots'][slot]);assert mat;mesh.set_material(i,mat)
    if e.get('native_lod_export'):
        assert digest(ROOT/e['native_lod_export'])==e['native_lod_export_sha256']
        assert subsystem.import_lod(mesh,1,str(ROOT/e['native_lod_export']))==1
        subsystem.set_lod_screen_sizes(mesh,[1,.025]);mesh.set_minimum_lod_for_platform('Android',1)
        assert mesh.get_num_triangles(1)==e['native_lod_triangles'] and mesh.get_num_sections(1)==e['sections']
        assert len(mesh.get_editor_property('static_materials'))==len(imported),'LOD must reuse the same semantic materials'
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections'],e['asset_id']
    b=mesh.get_bounding_box();size=b.max-b.min
    assert all(abs(a-v*100)<.2 for a,v in zip((size.x,size.y,size.z),e['dimensions_m'])),e['asset_id']
    expected_z=e.get('bounds_min_m',[0,0,0])[2]*100
    assert abs(b.min.z-expected_z)<.1,(e['asset_id'],b.min.z,expected_z)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    assignments=[mesh.get_material(i).get_path_name().split('.')[0] for i in range(len(imported))]
    assert assignments==[e['material_slots'][s] for s in imported]
    records.append(dict(asset_id=e['asset_id'],destination=e['mesh_destination'],export=e['export'],export_sha256=e['export_sha256'],triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),dimensions_cm=[size.x,size.y,size.z],minimum_z_cm=b.min.z,slots=imported,materials=assignments,lod1_triangles=mesh.get_num_triangles(1) if e.get('native_lod_export') else None,vertex_color_policy='REPLACE'))
    unreal.log('CITYSIM_D91_BUILDING_IMPORTED '+e['asset_id'])
(OUT/'Import.json').write_text(json.dumps(dict(task='D91',production_import=True,meshes=records,materials=materials,physical_device_verified=False),indent=2)+'\n')
unreal.log('CITYSIM_D91_BUILDING_IMPORT_PASS 61 meshes;opaque shared materials;paired pavilion LOD')
