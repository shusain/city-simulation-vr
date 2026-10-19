import unreal

path = "/Game/CitySim/UI/M_CityHeatMap"
material = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_CityHeatMap", "/Game/CitySim/UI", unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexColor, -200, 0)
    if not unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Cannot connect heat-map vertex colors")
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property("two_sided", True)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Cannot save heat-map material")
unreal.log("CITYSIM_HEATMAP_MATERIAL_READY")
