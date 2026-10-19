"""Persist the existing UI material's ISM permutation without regenerating other assets.
Run UnrealEditor-Cmd -ExecutePythonScript=ABSOLUTE_SCRIPT_PATH.
Add -CityUiInstancingVerifyOnly to check the saved flag in a fresh process.
"""
import unreal

path = "/Game/CitySim/UI/M_CityUiBar"
material = unreal.EditorAssetLibrary.load_asset(path)
if not isinstance(material, unreal.Material):
    raise RuntimeError(f"Missing UI master material: {path}")
before = material.get_editor_property("used_with_instanced_static_meshes")
verify_only = "-CityUiInstancingVerifyOnly" in unreal.SystemLibrary.get_command_line()
if not verify_only:
    unreal.MaterialEditingLibrary.set_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {path}")
if not material.get_editor_property("used_with_instanced_static_meshes"):
    raise RuntimeError("UI master lacks the instanced static mesh shader permutation")
unreal.log(f"CITYSIM_UI_ISM_VERIFIED Path={path} Before={before} VerifyOnly={verify_only}")
