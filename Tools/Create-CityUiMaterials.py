import unreal


SOURCE_TEXT_MATERIAL = "/Engine/EngineMaterials/DefaultTextMaterialOpaque"
DESTINATION_PATH = "/Game/CitySim/UI"
DESTINATION_NAME = "M_CityTextUnlit"
DESTINATION_MATERIAL = f"{DESTINATION_PATH}/{DESTINATION_NAME}"
BAR_MASTER_NAME = "M_CityUiBar"
BAR_MASTER_PATH = f"{DESTINATION_PATH}/{BAR_MASTER_NAME}"
BAR_COLOR_PARAMETER = "BarColor"
BAR_COLORS = {
    "MI_CityUiPanel": (0.012, 0.018, 0.028),
    "MI_CityUiBarRail": (0.14, 0.16, 0.19),
    "MI_CityUiBarResidential": (0.22, 0.72, 0.32),
    "MI_CityUiBarCommercial": (0.12, 0.46, 0.92),
    "MI_CityUiBarIndustrial": (0.95, 0.78, 0.08),
}


def ensure_unlit_text_material():
    material = unreal.EditorAssetLibrary.load_asset(DESTINATION_MATERIAL)
    if material and not isinstance(material, unreal.Material):
        raise RuntimeError(f"Unexpected asset type at {DESTINATION_MATERIAL}")

    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            DESTINATION_NAME, DESTINATION_PATH, unreal.EditorAssetLibrary.load_asset(SOURCE_TEXT_MATERIAL))
        if not material:
            raise RuntimeError(f"Could not duplicate {SOURCE_TEXT_MATERIAL}")

    base_color_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_BASE_COLOR)
    if not base_color_node:
        raise RuntimeError(f"{SOURCE_TEXT_MATERIAL} has no base-color input to reuse")

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # RobotoDistanceField already supplies the soft glyph mask. On Quest's
    # mobile forward/MSAA path, retain partial edge coverage instead of a
    # binary per-pixel cutout. Keep the atlas, mask graph and depth test unchanged.
    material.set_editor_property("use_alpha_to_coverage", True)
    if not unreal.MaterialEditingLibrary.connect_material_property(
            base_color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError(f"Could not connect unlit emissive color on {DESTINATION_MATERIAL}")

    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {DESTINATION_MATERIAL}")
    unreal.log(f"CITYSIM_UI_MATERIAL_READY Path={DESTINATION_MATERIAL} Shading=Unlit")


def ensure_bar_materials():
    material = unreal.EditorAssetLibrary.load_asset(BAR_MASTER_PATH)
    if material and not isinstance(material, unreal.Material):
        raise RuntimeError(f"Unexpected asset type at {BAR_MASTER_PATH}")
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            BAR_MASTER_NAME, DESTINATION_PATH, unreal.Material, unreal.MaterialFactoryNew())
        if not material:
            raise RuntimeError(f"Could not create {BAR_MASTER_PATH}")
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -300, 0)
        color.set_editor_property("parameter_name", BAR_COLOR_PARAMETER)
        color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        if not unreal.MaterialEditingLibrary.connect_material_property(
                color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError(f"Could not connect emissive color on {BAR_MASTER_PATH}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Selection outlines use pooled ISMs. Persist this shader permutation before
    # cooking; editor auto-usage changes do not reach the packaged Quest asset.
    unreal.MaterialEditingLibrary.set_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {BAR_MASTER_PATH}")

    for name, color_value in BAR_COLORS.items():
        path = f"{DESTINATION_PATH}/{name}"
        instance = unreal.EditorAssetLibrary.load_asset(path)
        if instance and not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError(f"Unexpected asset type at {path}")
        if not instance:
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, DESTINATION_PATH, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
        unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(instance)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance, BAR_COLOR_PARAMETER, unreal.LinearColor(*color_value, 1.0))
        if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
            raise RuntimeError(f"Could not save {path}")
    unreal.log(
        f"CITYSIM_UI_BAR_MATERIALS_READY Master={BAR_MASTER_PATH} Variants={len(BAR_COLORS)}")


ensure_unlit_text_material()
# A focused text update must not resave unrelated bar/outline material assets.
import os
if os.environ.get("CITYSIM_UI_TEXT_ONLY") != "1":
    ensure_bar_materials()
