"""D83 focused native import; source manifests/maps/saves are never rewritten."""
from pathlib import Path
import unreal,json,hashlib,shutil
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration';OUT.mkdir(parents=True,exist_ok=True)
report=OUT/'Import.json';report.unlink(missing_ok=True)
doc=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text())
for collection in ('rigs','clips','props'):
    for e in doc[collection]:assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
for e in doc['statics']:
    for lod in e['lods']:assert hashlib.sha256((ROOT/lod['export']).read_bytes()).hexdigest()==lod['sha256']
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
assets=unreal.AssetToolsHelpers.get_asset_tools()
static_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
skeletal_editor=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem) or unreal.SkeletalMeshEditorSubsystem
folder='/Game/CitySim/Art/Parks'
records=[]
def import_asset(path,destination,ui):
    parent,name=destination.rsplit('/',1);task=unreal.AssetImportTask()
    for k,v in dict(filename=str(ROOT/path),destination_path=parent,destination_name=name,automated=True,
                    replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',ui)
    assets.import_asset_tasks([task])
    obj=unreal.EditorAssetLibrary.load_asset(destination)
    if not obj:
        found=[unreal.load_asset(p) for p in task.get_editor_property('imported_object_paths')]
        candidates=[v for v in found if isinstance(v,unreal.AnimSequence)]
        assert len(candidates)==1,(destination,[v.get_path_name() for v in found])
        assert unreal.EditorAssetLibrary.rename_asset(candidates[0].get_path_name(),destination)
        obj=unreal.EditorAssetLibrary.load_asset(destination)
    assert obj,destination
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
for e in doc['statics']:
    destination=e['mesh_destination'];relative='CitySimVR/Content/'+destination.removeprefix('/Game/')+'.uasset'
    old=ROOT/relative;backup=OUT/'BeforeContent'/relative
    if old.exists() and not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old,backup)
    mesh=import_asset(e['lods'][0]['export'],destination,static_ui())
    assert static_editor.import_lod(mesh,1,str(ROOT/e['lods'][1]['export']))==1
    assert static_editor.set_lod_screen_sizes(mesh,[1,.025])
    mesh.set_minimum_lod_for_platform('Android',1)
    slots=mesh.get_editor_property('static_materials')
    for i,slot in enumerate(slots):
        name=str(slot.get_editor_property('imported_material_slot_name')).split('.')[0]
        material=unreal.load_asset(e['materials'][name]);assert material,(name,e['materials'])
        mesh.set_material(i,material)
    b=mesh.get_bounding_box();expected=e['lods'][0]['bounds_m']
    assert abs(b.min.z-expected[0][2]*100)<.2 and abs(b.max.z-expected[1][2]*100)<.2,(destination,b,expected)
    for lod in (0,1):
        assert mesh.get_num_triangles(lod)==e['lods'][lod]['triangles'],(destination,lod,mesh.get_num_triangles(lod))
        for section in range(mesh.get_num_sections(lod)):static_editor.enable_section_collision(mesh,False,lod,section)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(asset=destination,triangles=[l['triangles'] for l in e['lods']],type='static',grounded=True))

material_path=folder+'/Materials/M_ParkCitizenPalette'
material=unreal.load_asset(material_path)
if not material:
    source=unreal.load_asset('/Game/CitySim/Art/PC/Materials/M_CityCitizenPalette');assert source
    material=assets.duplicate_asset('M_ParkCitizenPalette',folder+'/Materials',source)
material.set_editor_property('used_with_skeletal_mesh',True)
material.set_editor_property('used_with_instanced_static_meshes',True)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
skeleton=None
for e in [v for v in doc['rigs'] if v['lod']==0]:
    name=e['name'];destination=folder+'/Meshes/'+name
    ui=unreal.FbxImportUI()
    ui.set_editor_property('automated_import_should_detect_type',False)
    for k,v in dict(import_mesh=True,import_animations=False,import_as_skeletal=True,import_materials=False,
                    import_textures=False,create_physics_asset=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
                    original_import_type=unreal.FBXImportType.FBXIT_SKELETAL_MESH).items():ui.set_editor_property(k,v)
    if skeleton:ui.set_editor_property('skeleton',skeleton)
    data=ui.get_editor_property('skeletal_mesh_import_data')
    for k,v in dict(convert_scene=True,convert_scene_unit=True,
                    vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                    normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    mesh=import_asset(e['export'],destination,ui)
    skeleton=skeleton or mesh.get_editor_property('skeleton')
    assert skeleton,(destination,'missing skeleton')
    assert unreal.CityParkAssetLibrary.bind_matching_park_skeleton(mesh,skeleton),(destination,'rest skeleton mismatch')
    assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton)
    mesh.set_editor_property('physics_asset',None)
    low=next(v for v in doc['rigs'] if v['name']==name+'_LOD1')
    assert skeletal_editor.import_lod(mesh,1,str(ROOT/low['export']))==1
    materials=list(mesh.get_editor_property('materials'))
    for m in materials:m.set_editor_property('material_interface',material)
    mesh.set_editor_property('materials',materials)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(asset=destination,triangles=[e['triangles'],low['triangles']],type='skeletal',skeleton=skeleton.get_path_name()))
    # Bounded animation fallback pools use the same citizens/stance, never capsules.
    name=name.replace('SK_','SM_');static=import_asset(e['static']['export'],folder+'/Meshes/'+name,static_ui())
    assert static_editor.import_lod(static,1,str(ROOT/low['static']['export']))==1
    static.set_material(0,material);static_editor.set_lod_screen_sizes(static,[1,.025])
    static.set_minimum_lod_for_platform('Android',1)
    assert unreal.EditorAssetLibrary.save_loaded_asset(static)
for e in doc['clips']:
    destination=folder+'/Animations/AS_Park_'+e['name']
    ui=unreal.FbxImportUI()
    ui.set_editor_property('automated_import_should_detect_type',False)
    for k,v in dict(import_mesh=False,import_animations=True,import_as_skeletal=True,import_materials=False,
                    import_textures=False,skeleton=skeleton,mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION,
                    original_import_type=unreal.FBXImportType.FBXIT_ANIMATION).items():ui.set_editor_property(k,v)
    data=ui.get_editor_property('anim_sequence_import_data')
    data.set_editor_property('use_default_sample_rate',False);data.set_editor_property('custom_sample_rate',24)
    anim=import_asset(e['export'],destination,ui)
    assert unreal.CityParkAssetLibrary.bind_matching_park_animation(anim,skeleton),(e['name'],'clip rest skeleton mismatch')
    assert anim.get_editor_property('skeleton')==skeleton,(e['name'],'skeleton binding differs')
    length=anim.get_editor_property('sequence_length')
    assert abs(length-e['period']/24)<.002,(e['name'],length,e['period'])
    assert unreal.EditorAssetLibrary.save_loaded_asset(anim)
    records.append(dict(asset=anim.get_path_name(),type='animation',seconds=length,source_pose_error_m=e['roundtrip_pose_error_m']))
for e in doc['props']:
    mesh=import_asset(e['export'],folder+'/Meshes/'+e['name'],static_ui())
    for i in range(len(mesh.get_editor_property('static_materials'))):mesh.set_material(i,material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(asset=mesh.get_path_name(),type='prop'))
report.write_text(json.dumps(dict(task='D83',assets=records,errors=[],maps_saved=False),indent=2)+'\n')
unreal.log('CITYSIM_D83_NATIVE_IMPORT_PASS')
