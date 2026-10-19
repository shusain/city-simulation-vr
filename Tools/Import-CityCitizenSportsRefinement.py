"""D88 import only three paired field rigs/fallbacks and five refined clips."""
from pathlib import Path
import unreal,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88';REPORT=OUT/'Import.json'
assert not REPORT.exists(),'Retain an existing import report before retrying'
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_native.json').read_text())
for e in doc['clips']+doc['rigs']:
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
    if 'static' in e:assert hashlib.sha256((ROOT/e['static']['export']).read_bytes()).hexdigest()==e['static']['sha256']
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
assets=unreal.AssetToolsHelpers.get_asset_tools();folder='/Game/CitySim/Art/Parks'
skeleton=unreal.load_asset(folder+'/Meshes/SK_Park_Skate_Casual_Skeleton');assert skeleton
material=unreal.load_asset(folder+'/Materials/M_ParkCitizenPalette');assert material
skeletal=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem) or unreal.SkeletalMeshEditorSubsystem
static=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
records=[]
def imported(path,destination,ui):
    parent,name=destination.rsplit('/',1);task=unreal.AssetImportTask()
    for k,v in dict(filename=str(ROOT/path),destination_path=parent,destination_name=name,automated=True,
                    replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',ui)
    assets.import_asset_tasks([task]);obj=unreal.load_asset(destination)
    assert obj,(destination,task.get_editor_property('imported_object_paths'))
    return obj
def static_ui():
    ui=unreal.FbxImportUI()
    for k,v in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False,
                    reset_to_fbx_on_material_conflict=True).items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('static_mesh_import_data')
    for k,v in dict(combine_meshes=True,convert_scene=True,convert_scene_unit=True,auto_generate_collision=False,
                    vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                    normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    return ui
for e in [v for v in doc['rigs'] if v['lod']==0]:
    ui=unreal.FbxImportUI();ui.set_editor_property('automated_import_should_detect_type',False)
    for k,v in dict(import_mesh=True,import_animations=False,import_as_skeletal=True,import_materials=False,
                    import_textures=False,create_physics_asset=False,skeleton=skeleton,
                    mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
                    original_import_type=unreal.FBXImportType.FBXIT_SKELETAL_MESH).items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('skeletal_mesh_import_data')
    for k,v in dict(convert_scene=True,convert_scene_unit=True,
                    vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                    normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    mesh=imported(e['export'],folder+'/Meshes/'+e['name'],ui)
    assert unreal.CityParkAssetLibrary.bind_matching_park_skeleton(mesh,skeleton),'Existing20-bone bind differs'
    mesh.set_editor_property('physics_asset',None)
    low=next(v for v in doc['rigs'] if v['name']==e['name']+'_LOD1')
    assert skeletal.import_lod(mesh,1,str(ROOT/low['export']))==1
    slots=list(mesh.get_editor_property('materials'))
    for slot in slots:slot.set_editor_property('material_interface',material)
    mesh.set_editor_property('materials',slots)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(asset=mesh.get_path_name(),type='skeletal',lod_triangles=[754,420],skeleton=skeleton.get_path_name()))
    fallback=imported(e['static']['export'],folder+'/Meshes/'+e['static']['name'],static_ui())
    assert static.import_lod(fallback,1,str(ROOT/low['static']['export']))==1
    fallback.set_material(0,material);static.set_lod_screen_sizes(fallback,[1,.025]);fallback.set_minimum_lod_for_platform('Android',1)
    for lod in (0,1):
        for section in range(fallback.get_num_sections(lod)):static.enable_section_collision(fallback,False,lod,section)
    assert unreal.EditorAssetLibrary.save_loaded_asset(fallback)
    records.append(dict(asset=fallback.get_path_name(),type='static',lod_triangles=[754,420]))
for e in doc['clips']:
    ui=unreal.FbxImportUI();ui.set_editor_property('automated_import_should_detect_type',False)
    for k,v in dict(import_mesh=False,import_animations=True,import_as_skeletal=True,import_materials=False,
                    import_textures=False,skeleton=skeleton,mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION,
                    original_import_type=unreal.FBXImportType.FBXIT_ANIMATION).items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('anim_sequence_import_data');data.set_editor_property('use_default_sample_rate',False);data.set_editor_property('custom_sample_rate',24)
    animation=imported(e['export'],folder+'/Animations/AS_Park_'+e['name'],ui)
    assert unreal.CityParkAssetLibrary.bind_matching_park_animation(animation,skeleton)
    length=animation.get_editor_property('sequence_length');assert abs(length-e['period']/24)<.002,(e['name'],length)
    animation.set_editor_property('enable_root_motion',False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(animation)
    records.append(dict(asset=animation.get_path_name(),type='animation',seconds=length,skeleton=skeleton.get_path_name()))
assert len(records)==11
REPORT.write_text(json.dumps(dict(task='D88',assets=records,errors=[],shared_skeleton_saved=False,maps_saved=False),indent=2)+'\n')
unreal.log('CITYSIM_D88_NATIVE_IMPORT_PASS 11 assets')
