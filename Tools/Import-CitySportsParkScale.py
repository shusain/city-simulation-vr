"""D88 scale import: only two existing fields and two existing native balls."""
from pathlib import Path
import unreal,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale';REPORT=OUT/'Import.json'
assert not REPORT.exists(),'Preserve previous native evidence before a retry'
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json').read_text())
proof=json.loads((OUT/'Validation.json').read_text());assert proof['pass_checks'] and proof['source_sha256']==doc['source_sha256']
starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
assets=unreal.AssetToolsHelpers.get_asset_tools();editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
records=[]
for e in doc['statics']+doc['props']:
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
    destination=e['mesh_destination'];old=unreal.load_asset(destination);assert isinstance(old,unreal.StaticMesh)
    old_materials=[v.get_editor_property('material_interface') for v in old.get_editor_property('static_materials')]
    ui=unreal.FbxImportUI()
    for k,v in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False,
                    automated_import_should_detect_type=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH).items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('static_mesh_import_data')
    for k,v in dict(combine_meshes=True,convert_scene=True,convert_scene_unit=True,auto_generate_collision=False,
                    vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                    normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    folder,name=destination.rsplit('/',1);task=unreal.AssetImportTask()
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=folder,destination_name=name,automated=True,
                    replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',ui);assets.import_asset_tasks([task])
    mesh=unreal.load_asset(destination);assert isinstance(mesh,unreal.StaticMesh)
    if 'asset_id' in e:
        mapping=next(v['material_slots'] for v in starter['assets'] if v['asset_id']==e['asset_id'])
        for i,slot in enumerate(mesh.get_editor_property('static_materials')):
            role=str(slot.get_editor_property('imported_material_slot_name')).split('.')[0]
            material=unreal.load_asset(mapping[role]);assert material;mesh.set_material(i,material)
    else:
        assert old_materials and old_materials[0];mesh.set_material(0,old_materials[0])
    for lod in range(mesh.get_num_lods()):
        for section in range(mesh.get_num_sections(lod)):editor.enable_section_collision(mesh,False,lod,section)
    box=mesh.get_bounding_box();lo,hi=e['bounds_m']
    # FBX Y is reflected; X/Z and dimensions verify real import units.
    for a,b in ((box.min.x,lo[0]*100),(box.max.x,hi[0]*100),(box.min.z,lo[2]*100),(box.max.z,hi[2]*100)):
        assert abs(a-b)<.1,(destination,a,b)
    assert mesh.get_num_triangles(0)==e['triangles']
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(asset=destination,triangles=e['triangles'],bounds_cm=[[box.min.x,box.min.y,box.min.z],[box.max.x,box.max.y,box.max.z]],export_sha256=e['sha256']))
REPORT.write_text(json.dumps(dict(task='D88-Scale',assets=records,source_sha256=doc['source_sha256'],errors=[]),indent=2)+'\n')
print('CITYSIM_D88_SCALE_IMPORT_PASS 4 existing assets;no skeleton/clip/catalog/map saves')
