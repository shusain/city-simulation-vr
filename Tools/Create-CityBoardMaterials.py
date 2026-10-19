"""Author the enclosed board's earth material and shadow-free sandbox floor."""
from pathlib import Path
import json
import unreal

report = Path(__file__).resolve().parents[1] / 'Artifacts/D67-TerrainBase-Material.json'
report.unlink(missing_ok=True)
path = '/Game/CitySim/Art/Materials/M_CityRoomFloor'
material = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_CityRoomFloor', '/Game/CitySim/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError('Cannot create room floor material')
color = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if not color:
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -250, 0)
assert isinstance(color, unreal.MaterialExpressionConstant3Vector), 'Unexpected floor graph; preserve it for review'
color.set_editor_property('constant', unreal.LinearColor(.12, .12, .12, 1))
assert unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
base_path = '/Game/CitySim/Art/Materials/MI_CityTerrainBase'
base = unreal.EditorAssetLibrary.load_asset(base_path) if unreal.EditorAssetLibrary.does_asset_exist(base_path) else None
if not base:
    base = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MI_CityTerrainBase', '/Game/CitySim/Art/Materials', unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
parent = unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Art/Materials/M_CityColor')
assert base and parent
unreal.MaterialEditingLibrary.set_material_instance_parent(base, parent)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(base, 'CityColor', unreal.LinearColor(.16, .11, .065, 1))
assert unreal.EditorAssetLibrary.save_loaded_asset(base)
report.write_text(json.dumps(dict(asset=path, shading='Unlit', color=[.12, .12, .12],
    baseAsset=base_path, baseParent=parent.get_path_name(), baseColor=[.16, .11, .065], saved=True), indent=2))
unreal.log('CITYSIM_ROOM_FLOOR_MATERIAL_READY')
