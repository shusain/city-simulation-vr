"""Import only city_environment_kit.json; never touch the service catalog/map."""
from pathlib import Path
import json, unreal
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'ArtSource/Manifests/city_environment_kit.json').read_text())
tools=unreal.AssetToolsHelpers.get_asset_tools()
master=unreal.load_asset('/Game/CitySim/Art/Materials/M_CityColor')
colors={'Bark':(.22,.12,.055),'Canopy':(.12,.32,.07),'Rock':(.30,.29,.26),'Rain':(.38,.58,.70),'Snow':(.86,.92,.97)}
materials={}
for name,color in colors.items():
    asset_name='MI_Environment'+name; path='/Game/CitySim/Art/Materials/'+asset_name
    mat=unreal.load_asset(path) or tools.create_asset(asset_name,'/Game/CitySim/Art/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mat,master)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat,'CityColor',unreal.LinearColor(*color,1))
    unreal.EditorAssetLibrary.save_loaded_asset(mat); materials[name]=mat
for entry in manifest['assets']:
    task=unreal.AssetImportTask(); task.filename=str(root/entry['export']);task.destination_path='/Game/CitySim/Art/Meshes';task.destination_name=entry['name'];task.automated=True;task.replace_existing=True;task.save=True
    options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
    options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
    options.static_mesh_import_data.set_editor_property('combine_meshes',True)
    options.static_mesh_import_data.set_editor_property('convert_scene_unit',True)
    task.options=options;tools.import_asset_tasks([task])
    mesh=unreal.load_asset(task.destination_path+'/'+entry['name'])
    if not mesh: raise RuntimeError('Import failed: '+entry['name'])
    for index,slot in enumerate(mesh.get_editor_property('static_materials')):
        mesh.set_material(index,materials[str(slot.get_editor_property('imported_material_slot_name'))])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    print('CITYSIM_ENVIRONMENT_IMPORTED',entry['name'],entry['triangles'])
# Add two ordinary material inputs to the existing terrain graph, preserving grid/detail.
terrain=unreal.load_asset('/Game/CitySim/Art/Materials/M_CityTerrain')
lib=unreal.MaterialEditingLibrary
if 'WeatherSnow' not in [str(name) for name in lib.get_scalar_parameter_names(terrain)]:
    # Locate the existing material output through the editor-only graph API below.
    surface=lib.get_material_property_input_node(terrain,unreal.MaterialProperty.MP_BASE_COLOR)
    base=lib.get_inputs_for_material_expression(terrain,surface)[1]
    wet=lib.create_material_expression(terrain,unreal.MaterialExpressionScalarParameter,1700,-300);wet.set_editor_property('parameter_name','WeatherWet');wet.set_editor_property('default_value',0)
    snow=lib.create_material_expression(terrain,unreal.MaterialExpressionScalarParameter,1700,-100);snow.set_editor_property('parameter_name','WeatherSnow');snow.set_editor_property('default_value',0)
    dark=lib.create_material_expression(terrain,unreal.MaterialExpressionMultiply,1900,-300)
    factor=lib.create_material_expression(terrain,unreal.MaterialExpressionConstant,1700,-400);factor.set_editor_property('r',.70)
    lib.connect_material_expressions(base,'',dark,'A');lib.connect_material_expressions(factor,'',dark,'B')
    wet_lerp=lib.create_material_expression(terrain,unreal.MaterialExpressionLinearInterpolate,2100,-200)
    lib.connect_material_expressions(base,'',wet_lerp,'A');lib.connect_material_expressions(dark,'',wet_lerp,'B');lib.connect_material_expressions(wet,'',wet_lerp,'Alpha')
    snow_color=lib.create_material_expression(terrain,unreal.MaterialExpressionConstant3Vector,2100,0);snow_color.set_editor_property('constant',unreal.LinearColor(.66,.74,.78,1))
    result=lib.create_material_expression(terrain,unreal.MaterialExpressionLinearInterpolate,2300,0)
    lib.connect_material_expressions(wet_lerp,'',result,'A');lib.connect_material_expressions(snow_color,'',result,'B');lib.connect_material_expressions(snow,'',result,'Alpha')
    lib.connect_material_expressions(result,'',surface,'B')
    lib.recompile_material(terrain);unreal.EditorAssetLibrary.save_loaded_asset(terrain)
# Repair the first import's output-level blend so the placement grid remains
# fully legible through snow. This is idempotent for the final graph.
output=lib.get_material_property_input_node(terrain,unreal.MaterialProperty.MP_BASE_COLOR)
inputs=lib.get_inputs_for_material_expression(terrain,output)
if len(inputs)==3 and isinstance(inputs[2],unreal.MaterialExpressionScalarParameter) and str(inputs[2].get_editor_property('parameter_name'))=='WeatherSnow':
    wet_lerp=inputs[0];wet_inputs=lib.get_inputs_for_material_expression(terrain,wet_lerp)
    surface=wet_inputs[0];base=lib.get_inputs_for_material_expression(terrain,surface)[1]
    lib.connect_material_expressions(base,'',wet_lerp,'A');lib.connect_material_expressions(base,'',wet_inputs[1],'A')
    lib.connect_material_expressions(output,'',surface,'B');lib.connect_material_property(surface,'',unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(terrain);unreal.EditorAssetLibrary.save_loaded_asset(terrain)
print('CITYSIM_ENVIRONMENT_IMPORT_COMPLETE')
