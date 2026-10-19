"""Import three accepted D71 palettes and their authored simpler LODs; no map/save edits."""
from pathlib import Path
import hashlib,json
import unreal

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/D79-MetalD71';out.mkdir(parents=True,exist_ok=True)
report_path=out/'citizen-import.json';report_path.unlink(missing_ok=True)
manifest=json.loads((root/'ArtSource/Manifests/city_citizen_presentation.json').read_text())
assert len(manifest['assets'])==6 and all(e['roundtrip_pass'] for e in manifest['assets'])
for e in manifest['assets']:
    assert hashlib.sha256((root/e['export']).read_bytes()).hexdigest()==e['sha256']
    assert hashlib.sha256((root/e['source_export']).read_bytes()).hexdigest()==e['source_sha256']
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
assert not unreal.SystemLibrary.get_console_variable_bool_value('Interchange.FeatureFlags.Import.FBX')
assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);assert subsystem
folder='/Game/CitySim/Art/PC'
material_path=folder+'/Materials/M_CityCitizenPalette'
material=unreal.EditorAssetLibrary.load_asset(material_path)
if not material:material=assets.create_asset('M_CityCitizenPalette',folder+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(material)
material.set_editor_property('used_with_instanced_static_meshes',True)
vertex=lib.create_material_expression(material,unreal.MaterialExpressionVertexColor,0,0)
linear=lib.create_material_expression(material,unreal.MaterialExpressionPower,200,0)
linear.set_editor_property('const_exponent',2.2)
assert lib.connect_material_expressions(vertex,'',linear,'Base')
assert lib.connect_material_property(linear,'',unreal.MaterialProperty.MP_BASE_COLOR)
rough=lib.create_material_expression(material,unreal.MaterialExpressionConstant,0,200);rough.set_editor_property('r',.78)
assert lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
lib.layout_material_expressions(material);lib.recompile_material(material)
stats=lib.get_statistics(material);assert stats.get_editor_property('num_pixel_shader_instructions')>0
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
baseline=unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Art/Meshes/SM_Citizen_A')
baseline_bounds=baseline.get_bounding_box()
records=[]
for style in ('Casual','Office','Worker'):
    entries=[e for e in manifest['assets'] if e['name'].removesuffix('_LOD1')=='SM_Citizen_PC_'+style]
    lod0=next(e for e in entries if not e['name'].endswith('_LOD1'));lod1=next(e for e in entries if e['name'].endswith('_LOD1'))
    name=lod0['name'];task=unreal.AssetImportTask()
    for key,value in dict(filename=str(root/lod0['export']),destination_path=folder+'/Meshes',destination_name=name,
                          automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(key,value)
    task.set_editor_property('factory',unreal.FbxFactory())
    ui=unreal.FbxImportUI();ui.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for key,value in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False).items():ui.set_editor_property(key,value)
    data=ui.get_editor_property('static_mesh_import_data')
    for key,value in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                          convert_scene=True,convert_scene_unit=True,auto_generate_collision=False).items():data.set_editor_property(key,value)
    data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task.set_editor_property('options',ui);assets.import_asset_tasks([task])
    mesh=unreal.EditorAssetLibrary.load_asset(folder+'/Meshes/'+name);assert mesh
    mesh.get_editor_property('asset_import_data').set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
    assert subsystem.import_lod(mesh,1,str(root/lod1['export']))==1
    assert subsystem.set_lod_screen_sizes(mesh,manifest['lod_screen_sizes'])
    mesh.set_material(0,material)
    for index,entry in enumerate((lod0,lod1)):
        assert mesh.get_num_triangles(index)==entry['triangles']
        assert mesh.get_num_sections(index)==1
        subsystem.enable_section_collision(mesh,False,index,0)
    bounds=mesh.get_bounding_box()
    assert abs(bounds.min.z-baseline_bounds.min.z)<.02 and abs(bounds.max.z-baseline_bounds.max.z)<.02,(name,bounds,baseline_bounds)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    records.append(dict(mesh=mesh.get_path_name(),triangles=[lod0['triangles'],lod1['triangles']],sections=[1,1],
                        min_z_cm=bounds.min.z,max_z_cm=bounds.max.z,screen_sizes=list(subsystem.get_lod_screen_sizes(mesh)),
                        material=material.get_path_name(),collision_enabled=False))
report_path.write_text(json.dumps(dict(task='D71',meshes=records,baseline_min_z_cm=baseline_bounds.min.z,
    baseline_max_z_cm=baseline_bounds.max.z,source_colors_retained=True,errors=[]),indent=2)+'\n')
unreal.log('CITYSIM_CITIZEN_PRESENTATION_IMPORT_PASS 3')
