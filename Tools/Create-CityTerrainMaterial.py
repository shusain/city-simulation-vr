import unreal
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
READY = os.path.join(ROOT, "Artifacts", "Diagnostics", "D38-terrain-material-ready.txt")
if os.path.exists(READY): os.remove(READY)


DESTINATION_PATH = "/Game/CitySim/Art/Materials"
MASTER_NAME = "M_CityTerrain"
MASTER_MATERIAL = f"{DESTINATION_PATH}/{MASTER_NAME}"
INSTANCE_NAME = "MI_CityTerrain"
INSTANCE_MATERIAL = f"{DESTINATION_PATH}/{INSTANCE_NAME}"
GRID_COLOR_PARAMETER = "GridColor"
GRID_WIDTH_PARAMETER = "GridWidth"
WATER_MASTER_NAME = "M_CityWater"
WATER_MASTER_MATERIAL = f"{DESTINATION_PATH}/{WATER_MASTER_NAME}"
WATER_INSTANCE_NAME = "MI_CityWater"
WATER_INSTANCE_MATERIAL = f"{DESTINATION_PATH}/{WATER_INSTANCE_NAME}"


def expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y)


def connect(source, target, input_name, output_name=""):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output_name, target, input_name):
        raise RuntimeError(
            f"Could not connect {source.get_class().get_name()} to "
            f"{target.get_class().get_name()}.{input_name or '<first>'}")


# These assets are generated exclusively by this script. Recreate them so a
# previous template-grid parent cannot survive as stale compiled state.
for asset_path in (
        INSTANCE_MATERIAL, MASTER_MATERIAL,
        WATER_INSTANCE_MATERIAL, WATER_MASTER_MATERIAL):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError(f"Could not replace generated asset {asset_path}")

material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MASTER_NAME, DESTINATION_PATH, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError(f"Could not create {MASTER_MATERIAL}")

texcoord = expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, 80)
fraction = expression(material, unreal.MaterialExpressionFrac, -920, 80)
connect(texcoord, fraction, "")

fraction_u = expression(material, unreal.MaterialExpressionComponentMask, -740, -20)
fraction_u.set_editor_property("r", True)
fraction_u.set_editor_property("g", False)
fraction_u.set_editor_property("b", False)
fraction_u.set_editor_property("a", False)
connect(fraction, fraction_u, "")

fraction_v = expression(material, unreal.MaterialExpressionComponentMask, -740, 180)
fraction_v.set_editor_property("r", False)
fraction_v.set_editor_property("g", True)
fraction_v.set_editor_property("b", False)
fraction_v.set_editor_property("a", False)
connect(fraction, fraction_v, "")

one_minus_u = expression(material, unreal.MaterialExpressionOneMinus, -560, -80)
one_minus_v = expression(material, unreal.MaterialExpressionOneMinus, -560, 240)
connect(fraction_u, one_minus_u, "")
connect(fraction_v, one_minus_v, "")

edge_u = expression(material, unreal.MaterialExpressionMin, -380, -20)
edge_v = expression(material, unreal.MaterialExpressionMin, -380, 180)
connect(fraction_u, edge_u, "A")
connect(one_minus_u, edge_u, "B")
connect(fraction_v, edge_v, "A")
connect(one_minus_v, edge_v, "B")

edge = expression(material, unreal.MaterialExpressionMin, -200, 80)
connect(edge_u, edge, "A")
connect(edge_v, edge, "B")

grid_width = expression(material, unreal.MaterialExpressionScalarParameter, -200, 300)
grid_width.set_editor_property("parameter_name", GRID_WIDTH_PARAMETER)
grid_width.set_editor_property("default_value", 0.025)

normalized_edge = expression(material, unreal.MaterialExpressionDivide, 0, 100)
connect(edge, normalized_edge, "A")
connect(grid_width, normalized_edge, "B")
interior_mask = expression(material, unreal.MaterialExpressionSaturate, 180, 100)
connect(normalized_edge, interior_mask, "")

vertex_color = expression(material, unreal.MaterialExpressionVertexColor, 0, -180)
grid_color = expression(material, unreal.MaterialExpressionVectorParameter, 0, -40)
grid_color.set_editor_property("parameter_name", GRID_COLOR_PARAMETER)
grid_color.set_editor_property("default_value", unreal.LinearColor(0.07, 0.12, 0.055, 1.0))

# One seamless, mipmapped linear mask spans one eighth of a logical cell (1 m).
# Global board UVs keep its phase identical at mesh/chunk boundaries.
task = unreal.AssetImportTask()
task.set_editor_property("filename", os.path.join(ROOT, "ArtSource", "Textures", "CityTerrain.png"))
task.set_editor_property("destination_path", "/Game/CitySim/Art/Textures")
task.set_editor_property("destination_name", "T_CityTerrain")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.EditorAssetLibrary.load_asset("/Game/CitySim/Art/Textures/T_CityTerrain")
texture.set_editor_property("srgb", False)
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
terrain_uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, -500)
terrain_uv.set_editor_property("u_tiling", 8.0)
terrain_uv.set_editor_property("v_tiling", 8.0)
sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -500)
sample.set_editor_property("parameter_name", "TerrainVariation")
sample.set_editor_property("texture", texture)
sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
connect(terrain_uv, sample, "")
variation = expression(material, unreal.MaterialExpressionMultiply, -650, -500)
variation.set_editor_property("const_b", 2.0)
connect(sample, variation, "A", "R")
strength = expression(material, unreal.MaterialExpressionScalarParameter, -650, -650)
strength.set_editor_property("parameter_name", "TerrainDetailStrength")
strength.set_editor_property("default_value", 0.4)
muted = expression(material, unreal.MaterialExpressionLinearInterpolate, -400, -500)
muted.set_editor_property("const_a", 1.0)
connect(variation, muted, "B")
connect(strength, muted, "Alpha")
terrain_color = expression(material, unreal.MaterialExpressionMultiply, -180, -400)
connect(vertex_color, terrain_color, "A")
connect(muted, terrain_color, "B")

# D65: UV1 is a seeded, terrain-derived cosmetic pair (snow retention, wet
# retention). BoardSurface bakes it once with the existing half-cell mesh.
# Ordinary graph expressions add no texture fetches or custom/mobile shaders.
weather_uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, -1100)
weather_uv.set_editor_property("coordinate_index", 1)
snow_retention = expression(material, unreal.MaterialExpressionComponentMask, -900, -1200)
snow_retention.set_editor_property("r", True)
snow_retention.set_editor_property("g", False)
snow_retention.set_editor_property("b", False)
snow_retention.set_editor_property("a", False)
wet_retention = expression(material, unreal.MaterialExpressionComponentMask, -900, -1000)
wet_retention.set_editor_property("r", False)
wet_retention.set_editor_property("g", True)
wet_retention.set_editor_property("b", False)
wet_retention.set_editor_property("a", False)
connect(weather_uv, snow_retention, "")
connect(weather_uv, wet_retention, "")
snow = expression(material, unreal.MaterialExpressionScalarParameter, -900, -1400)
snow.set_editor_property("parameter_name", "WeatherSnow")
snow.set_editor_property("default_value", 0.0)
wet = expression(material, unreal.MaterialExpressionScalarParameter, -900, -800)
wet.set_editor_property("parameter_name", "WeatherWet")
wet.set_editor_property("default_value", 0.0)
snow_input = expression(material, unreal.MaterialExpressionSaturate, -730, -1400)
connect(snow, snow_input, "")
snow_growth = expression(material, unreal.MaterialExpressionMultiply, -560, -1400)
snow_growth.set_editor_property("const_b", 1.25)
connect(snow_input, snow_growth, "A")
snow_threshold = expression(material, unreal.MaterialExpressionOneMinus, -730, -1200)
connect(snow_retention, snow_threshold, "")
snow_remaining = expression(material, unreal.MaterialExpressionSubtract, -380, -1300)
connect(snow_growth, snow_remaining, "A")
connect(snow_threshold, snow_remaining, "B")
snow_spread = expression(material, unreal.MaterialExpressionMultiply, -200, -1300)
snow_spread.set_editor_property("const_b", 1.4)
connect(snow_remaining, snow_spread, "A")
snow_clamp = expression(material, unreal.MaterialExpressionSaturate, 0, -1300)
connect(snow_spread, snow_clamp, "")
snow_alpha = expression(material, unreal.MaterialExpressionMultiply, 180, -1300)
snow_alpha.set_editor_property("const_b", 0.82)
connect(snow_clamp, snow_alpha, "A")
wet_variation = expression(material, unreal.MaterialExpressionMultiply, -560, -1000)
wet_variation.set_editor_property("const_b", 0.65)
connect(wet_retention, wet_variation, "A")
wet_base = expression(material, unreal.MaterialExpressionAdd, -380, -1000)
wet_base.set_editor_property("const_b", 0.35)
connect(wet_variation, wet_base, "A")
wet_input = expression(material, unreal.MaterialExpressionSaturate, -560, -800)
connect(wet, wet_input, "")
wet_alpha = expression(material, unreal.MaterialExpressionMultiply, -200, -1000)
connect(wet_base, wet_alpha, "A")
connect(wet_input, wet_alpha, "B")
wet_dark = expression(material, unreal.MaterialExpressionMultiply, 0, -800)
wet_dark.set_editor_property("const_b", 0.7)
connect(terrain_color, wet_dark, "A")
wet_color = expression(material, unreal.MaterialExpressionLinearInterpolate, 180, -800)
connect(terrain_color, wet_color, "A")
connect(wet_dark, wet_color, "B")
connect(wet_alpha, wet_color, "Alpha")
snow_color = expression(material, unreal.MaterialExpressionConstant3Vector, 0, -600)
snow_color.set_editor_property("constant", unreal.LinearColor(0.66, 0.74, 0.78, 1))
snow_detail = expression(material, unreal.MaterialExpressionMultiply, 180, -600)
connect(snow_color, snow_detail, "A")
connect(muted, snow_detail, "B")
weather_color = expression(material, unreal.MaterialExpressionLinearInterpolate, 380, -500)
connect(wet_color, weather_color, "A")
connect(snow_detail, weather_color, "B")
connect(snow_alpha, weather_color, "Alpha")

# Reveal grid only near the current placement ray, with a soft outer quarter.
center = expression(material, unreal.MaterialExpressionVectorParameter, 0, 500)
center.set_editor_property("parameter_name", "GridCenter")
center.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
center_xy = expression(material, unreal.MaterialExpressionComponentMask, 180, 500)
center_xy.set_editor_property("r", True)
center_xy.set_editor_property("g", True)
connect(center, center_xy, "")
distance = expression(material, unreal.MaterialExpressionDistance, 360, 500)
connect(texcoord, distance, "A")
connect(center_xy, distance, "B")
radius = expression(material, unreal.MaterialExpressionScalarParameter, 180, 700)
radius.set_editor_property("parameter_name", "GridRadius")
radius.set_editor_property("default_value", 3.0)
normalized = expression(material, unreal.MaterialExpressionDivide, 540, 500)
connect(distance, normalized, "A")
connect(radius, normalized, "B")
falloff = expression(material, unreal.MaterialExpressionOneMinus, 720, 500)
connect(normalized, falloff, "")
feather = expression(material, unreal.MaterialExpressionMultiply, 900, 500)
feather.set_editor_property("const_b", 4.0)
connect(falloff, feather, "A")
clamped = expression(material, unreal.MaterialExpressionSaturate, 1080, 500)
connect(feather, clamped, "")
enabled = expression(material, unreal.MaterialExpressionScalarParameter, 900, 700)
enabled.set_editor_property("parameter_name", "GridReveal")
enabled.set_editor_property("default_value", 0.0)
reveal = expression(material, unreal.MaterialExpressionMultiply, 1260, 500)
connect(clamped, reveal, "A")
connect(enabled, reveal, "B")
lines = expression(material, unreal.MaterialExpressionOneMinus, 360, 300)
connect(interior_mask, lines, "")
local_lines = expression(material, unreal.MaterialExpressionMultiply, 1440, 300)
connect(lines, local_lines, "A")
connect(reveal, local_lines, "B")
local_interior = expression(material, unreal.MaterialExpressionOneMinus, 1620, 300)
connect(local_lines, local_interior, "")

surface = expression(material, unreal.MaterialExpressionLinearInterpolate, 380, 0)
connect(grid_color, surface, "A")
connect(weather_color, surface, "B")
connect(local_interior, surface, "Alpha")
if not unreal.MaterialEditingLibrary.connect_material_property(
        surface, "", unreal.MaterialProperty.MP_BASE_COLOR):
    raise RuntimeError(f"Could not connect terrain base color on {MASTER_MATERIAL}")

wet_roughness = expression(material, unreal.MaterialExpressionLinearInterpolate, 380, -1000)
wet_roughness.set_editor_property("const_a", 0.92)
wet_roughness.set_editor_property("const_b", 0.70)
connect(wet_alpha, wet_roughness, "Alpha")
roughness = expression(material, unreal.MaterialExpressionLinearInterpolate, 570, -1000)
roughness.set_editor_property("const_b", 0.94)
connect(wet_roughness, roughness, "A")
connect(snow_alpha, roughness, "Alpha")
if not unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS):
    raise RuntimeError(f"Could not connect terrain roughness on {MASTER_MATERIAL}")

material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError(f"Could not save {MASTER_MATERIAL}")

instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    INSTANCE_NAME,
    DESTINATION_PATH,
    unreal.MaterialInstanceConstant,
    unreal.MaterialInstanceConstantFactoryNew())
if not instance:
    raise RuntimeError(f"Could not create {INSTANCE_MATERIAL}")
unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
    instance, GRID_COLOR_PARAMETER, unreal.LinearColor(0.07, 0.12, 0.055, 1.0))
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
    instance, GRID_WIDTH_PARAMETER, 0.025)
if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
    raise RuntimeError(f"Could not save {INSTANCE_MATERIAL}")

water_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    WATER_MASTER_NAME,
    DESTINATION_PATH,
    unreal.Material,
    unreal.MaterialFactoryNew())
if not water_material:
    raise RuntimeError(f"Could not create {WATER_MASTER_MATERIAL}")
water_material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
water_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
water_material.set_editor_property("two_sided", True)
water_vertex_color = expression(
    water_material, unreal.MaterialExpressionVertexColor, -220, -20)
if not unreal.MaterialEditingLibrary.connect_material_property(
        water_vertex_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError(f"Could not connect water color on {WATER_MASTER_MATERIAL}")
water_opacity = expression(
    water_material, unreal.MaterialExpressionConstant, -220, 160)
water_opacity.set_editor_property("r", 0.72)
if not unreal.MaterialEditingLibrary.connect_material_property(
        water_opacity, "", unreal.MaterialProperty.MP_OPACITY):
    raise RuntimeError(f"Could not connect water opacity on {WATER_MASTER_MATERIAL}")
unreal.MaterialEditingLibrary.recompile_material(water_material)
if not unreal.EditorAssetLibrary.save_loaded_asset(water_material):
    raise RuntimeError(f"Could not save {WATER_MASTER_MATERIAL}")

water_instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    WATER_INSTANCE_NAME,
    DESTINATION_PATH,
    unreal.MaterialInstanceConstant,
    unreal.MaterialInstanceConstantFactoryNew())
if not water_instance:
    raise RuntimeError(f"Could not create {WATER_INSTANCE_MATERIAL}")
unreal.MaterialEditingLibrary.set_material_instance_parent(
    water_instance, water_material)
if not unreal.EditorAssetLibrary.save_loaded_asset(water_instance):
    raise RuntimeError(f"Could not save {WATER_INSTANCE_MATERIAL}")

unreal.log(
    f"CITYSIM_TERRAIN_MATERIAL_READY Master={MASTER_MATERIAL} "
    f"Instance={INSTANCE_MATERIAL} GridWidth=0.025 Source=LogicalUV VertexColor=Terrain "
    f"Water={WATER_INSTANCE_MATERIAL} Weather=UV1Retention SeededCoverage WetSnowGradual")

with open(READY, "w", encoding="utf-8") as stream:
    stream.write("ready")
