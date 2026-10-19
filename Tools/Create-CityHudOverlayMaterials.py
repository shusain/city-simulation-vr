"""HUD-only standard-material variants; preserve depth-tested world markers."""
import unreal

for source, name in (
        ('/Game/CitySim/UI/M_CityTextUnlit', 'M_CityHudText'),
        ('/Game/CitySim/UI/M_CityUiBar', 'M_CityHudBar'),
        ('/Game/CitySim/UI/Icons/M_CityUiIcon', 'M_CityHudIcon'),
        ('/Game/CitySim/UI/M_CityHeatMap', 'M_CityHudHeatMap')):
    original = unreal.load_asset(source)
    if not isinstance(original, unreal.Material):
        raise RuntimeError(f'Missing source material: {source}')
    path = '/Game/CitySim/UI/' + name
    material = unreal.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            name, '/Game/CitySim/UI', original)
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f'Cannot create HUD material: {path}')
    # Reuse the distance-field glyph/icon mask as smooth opacity. Bar/heatmap
    # variants have full opacity; changing blend mode alone keeps it at one.
    mask = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_OPACITY_MASK)
    if mask:
        output = 'A' if name == 'M_CityHudIcon' else ''
        if not unreal.MaterialEditingLibrary.connect_material_property(
                mask, output, unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError(f'Cannot connect HUD opacity: {path}')
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('disable_depth_test', True)
    material.set_editor_property('translucency_pass', unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f'Cannot save HUD material: {path}')
    unreal.log(f'CITYSIM_HUD_OVERLAY_MATERIAL_READY Path={path}')
