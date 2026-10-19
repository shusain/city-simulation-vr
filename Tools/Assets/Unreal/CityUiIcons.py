"""Shared UI icon import/material operations; importing performs no asset writes."""
import os
import unreal


ASSET_PATH = "/Game/CitySim/UI/Icons"
MASTER_PATH = f"{ASSET_PATH}/M_CityUiIcon"
TEXTURE_PARAMETER = "IconTexture"
TINT_PARAMETER = "IconTint"
DEFAULT_ICON_NAMES = ("download", "upload", "play_arrow", "restart_alt", "arrow_back", "close", "settings", "bar_chart", "build", "construction", "local_police")
CONDITION_ICON_NAMES = ("build", "construction", "local_police")


def import_textures(source_root, icon_names):
    tasks = []
    for name in icon_names:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source_root, f"{name}.png"))
        task.set_editor_property("destination_path", ASSET_PATH)
        task.set_editor_property("destination_name", f"T_Icon_{name}")
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for task in tasks:
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError(f"Icon import failed: {task.get_editor_property('filename')}")


def ensure_master():
    material = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if material and not isinstance(material, unreal.Material):
        raise RuntimeError(f"Unexpected asset type at {MASTER_PATH}")
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_CityUiIcon", ASSET_PATH, unreal.Material, unreal.MaterialFactoryNew())
        texture = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
        texture.set_editor_property("parameter_name", TEXTURE_PARAMETER)
        tint = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -500, 180)
        tint.set_editor_property("parameter_name", TINT_PARAMETER)
        tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(
            tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(
            texture, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {MASTER_PATH}")
    return material


def ensure_instances(master, icon_names):
    for name in icon_names:
        path = f"{ASSET_PATH}/MI_Icon_{name}"
        instance = unreal.EditorAssetLibrary.load_asset(path)
        if not instance:
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                f"MI_Icon_{name}", ASSET_PATH, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
        texture = unreal.EditorAssetLibrary.load_asset(f"{ASSET_PATH}/T_Icon_{name}")
        if not texture:
            raise RuntimeError(f"Missing imported texture for {name}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, master)
        unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(instance)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            instance, TEXTURE_PARAMETER, texture)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance, TINT_PARAMETER, unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
            raise RuntimeError(f"Could not save {path}")
