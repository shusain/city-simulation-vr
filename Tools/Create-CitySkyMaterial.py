"""Rebuild the generated, texture-free Quest sky material using standard material nodes."""
import unreal

path = "/Game/CitySim/Art/Materials/M_CitySky"
material = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_CitySky", "/Game/CitySim/Art/Materials", unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError("Cannot create sky material")
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

def node(kind, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, kind, x, y)

def connect(source, target, pin):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(source, "", target, pin):
        raise RuntimeError("Cannot connect sky material node")

# Camera vector points toward the viewer. Negate its Z to get elevation of the view ray.
view = node(unreal.MaterialExpressionCameraVectorWS, -800, 200)
z = node(unreal.MaterialExpressionComponentMask, -640, 200)
for key, value in {"r": False, "g": False, "b": True, "a": False}.items():
    z.set_editor_property(key, value)
connect(view, z, "")
negate = node(unreal.MaterialExpressionMultiply, -480, 200)
negate.set_editor_property("const_b", -1.0)
connect(z, negate, "A")
clamp = node(unreal.MaterialExpressionSaturate, -320, 200)
connect(negate, clamp, "")
horizon = node(unreal.MaterialExpressionVectorParameter, -320, -120)
horizon.set_editor_property("parameter_name", "HorizonColor")
horizon.set_editor_property("default_value", unreal.LinearColor(0.35, 0.60, 0.80, 1.0))
zenith = node(unreal.MaterialExpressionVectorParameter, -320, 20)
zenith.set_editor_property("parameter_name", "ZenithColor")
zenith.set_editor_property("default_value", unreal.LinearColor(0.045, 0.18, 0.42, 1.0))
blend = node(unreal.MaterialExpressionLinearInterpolate, 0, 0)
connect(horizon, blend, "A")
connect(zenith, blend, "B")
connect(clamp, blend, "Alpha")
if not unreal.MaterialEditingLibrary.connect_material_property(blend, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Cannot connect sky emissive")
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property("two_sided", True)
material.set_editor_property("is_sky", True)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Cannot save sky material")
unreal.log("CITYSIM_SKY_MATERIAL_READY")
