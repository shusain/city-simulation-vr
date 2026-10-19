"""Standard depth-tested translucent vertex-color overlay for water placement."""
import unreal

path = '/Game/CitySim/UI/M_CityWaterPlacementOverlay'
material = unreal.EditorAssetLibrary.load_asset(path)
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_CityWaterPlacementOverlay', '/Game/CitySim/UI', unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('Cannot create water placement material')
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexColor, -200, 0)
    if not unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Cannot connect overlay color')
    if not unreal.MaterialEditingLibrary.connect_material_property(color, 'A', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Cannot connect overlay alpha')
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('two_sided', True)
material.set_editor_property('disable_depth_test', False)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError('Cannot save water placement material')
unreal.log_warning('CITYSIM_WATER_PLACEMENT_MATERIAL_READY')
