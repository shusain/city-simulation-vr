import unreal


MATERIAL_PATH = "/Game/CitySim/Art/Materials"
MASTER_NAME = "M_CityOverlayFade"
MASTER_PATH = f"{MATERIAL_PATH}/{MASTER_NAME}"
COLOR_PARAMETER = "CityColor"
FADE_OPACITY = 0.25
MATERIAL_COLORS = {
    "MI_CityDefault_OverlayFade": (0.35, 0.38, 0.42),
    "MI_CityFacade_OverlayFade": (0.22, 0.52, 0.78),
    "MI_CityResidential_OverlayFade": (0.22, 0.58, 0.32),
    "MI_CityCommercial_OverlayFade": (0.12, 0.42, 0.78),
    "MI_CityPower_OverlayFade": (0.70, 0.22, 0.08),
    "MI_CityService_OverlayFade": (0.08, 0.52, 0.58),
    "MI_CityRoof_OverlayFade": (0.10, 0.14, 0.19),
    "MI_CityRoad_OverlayFade": (0.055, 0.065, 0.08),
    "MI_CityTrim_OverlayFade": (0.95, 0.62, 0.16),
}


def ensure_master():
    material = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if material and not isinstance(material, unreal.Material):
        raise RuntimeError(f"Unexpected asset type at {MASTER_PATH}")
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MASTER_NAME, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
        if not material:
            raise RuntimeError(f"Could not create {MASTER_PATH}")
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -300, 0)
        color.set_editor_property("parameter_name", COLOR_PARAMETER)
        color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        if not unreal.MaterialEditingLibrary.connect_material_property(
                color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError(f"Could not connect emissive color on {MASTER_PATH}")

        opacity = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant, -300, 160)
        opacity.set_editor_property("r", FADE_OPACITY)
        if not unreal.MaterialEditingLibrary.connect_material_property(
                opacity, "", unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError(f"Could not connect opacity on {MASTER_PATH}")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {MASTER_PATH}")
    return material


def ensure_instance(name, parent, color):
    path = f"{MATERIAL_PATH}/{name}"
    instance = unreal.EditorAssetLibrary.load_asset(path)
    if instance and not isinstance(instance, unreal.MaterialInstanceConstant):
        raise RuntimeError(f"Unexpected asset type at {path}")
    if not instance:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_PATH, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(instance)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, COLOR_PARAMETER, unreal.LinearColor(*color, 1.0))
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
        raise RuntimeError(f"Could not save {path}")


master = ensure_master()
for material_name, material_color in MATERIAL_COLORS.items():
    ensure_instance(material_name, master, material_color)
unreal.log(
    f"CITYSIM_OVERLAY_FADE_READY Master={MASTER_PATH} Opacity={FADE_OPACITY} "
    f"Variants={len(MATERIAL_COLORS)}")
