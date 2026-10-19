"""Author non-emitting parked lenses and complementary response beacons."""
import unreal
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ready = root / 'Artifacts/Diagnostics/D45-civic-material-ready.txt'
ready.unlink(missing_ok=True)
path = '/Game/CitySim/Art/Materials/M_CityCivicDetail'
lib = unreal.MaterialEditingLibrary
m = unreal.EditorAssetLibrary.load_asset(path)
if not m:
    folder, name = path.rsplit('/', 1)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

def connect(source, output, target, pin):
    if not lib.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f'Civic connection failed: {source.get_name()}.{output} -> {target.get_name()}.{pin}')

def output(source, prop):
    if not lib.connect_material_property(source, '', prop):
        raise RuntimeError(f'Civic material output failed: {prop}')

strength = lib.get_material_property_input_node(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if strength:
    if strength.get_editor_property('desc') != 'CityCivicBrightness':
        old = strength
        old_output = lib.get_material_property_input_node_output_name(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        strength = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, 120, 220)
        connect(old, old_output, strength, 'A')
    glow = lib.get_inputs_for_material_expression(m, strength)[0]
    mask = lib.get_inputs_for_material_expression(m, glow)[1]
    vertex = lib.get_inputs_for_material_expression(m, mask)[0]
    base = lib.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR)
    if not (isinstance(glow, unreal.MaterialExpressionMultiply) and
            isinstance(mask, unreal.MaterialExpressionOneMinus) and
            isinstance(vertex, unreal.MaterialExpressionVertexColor) and
            isinstance(base, unreal.MaterialExpressionLinearInterpolate)):
        raise RuntimeError('Unexpected civic material graph; inspect before changing its rooted nodes')
else:
    # Retain any existing expressions: loaded mesh/particle references may root them.
    color = lib.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property('parameter_name', 'CityColor')
    color.set_editor_property('default_value', unreal.LinearColor(.10,.14,.19,1))
    vertex = lib.create_material_expression(m, unreal.MaterialExpressionVertexColor, -500, 200)
    mask = lib.create_material_expression(m, unreal.MaterialExpressionOneMinus, -300, 200)
    base = lib.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -100, 0)
    glow = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, 220)
    strength = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, 120, 220)
    connect(color, '', base, 'A')

known = {}
def remember(n):
    if not n or n.get_name() in known: return
    known[n.get_name()] = n
    for child in lib.get_inputs_for_material_expression(m, n): remember(child)
remember(strength)
remember(base)
tagged = {n.get_editor_property('desc'): n for n in known.values() if n.get_editor_property('desc')}
def pulse_node(kind, tag, legacy_tag=None):
    n = tagged.get(tag) or tagged.get(legacy_tag)
    if n and not isinstance(n, kind): raise RuntimeError(f'Unexpected civic pulse node: {tag}')
    if not n: n = lib.create_material_expression(m, kind)
    n.set_editor_property('desc', tag)
    return n

# VertexColor's main output is unnamed in Unreal; 'RGB' silently fails to connect.
# Repair both existing color wires, as well as authoring them correctly on first run.
connect(vertex, 'A', mask, '')
connect(vertex, '', base, 'B')
connect(mask, '', base, 'Alpha')
connect(vertex, '', glow, 'A')
connect(mask, '', glow, 'B')
connect(glow, '', strength, 'A')
strength.set_editor_property('desc', 'CityCivicBrightness')
strength.set_editor_property('const_b', 4.0)

# A slow two-second alternating cycle applies only to moving response materials.
# Parked decorative vehicles keep BeaconPulse=0 and emit nothing.
# Static-mesh vertex buffers retain sRGB bytes; decode lamp RGB before emission.
linear_color = pulse_node(unreal.MaterialExpressionPower, 'CityCivicLampLinearColor')
linear_color.set_editor_property('const_exponent', 2.2)
connect(vertex, '', linear_color, 'Base')
lens = pulse_node(unreal.MaterialExpressionMultiply, 'CityCivicLampLensTint')
lens.set_editor_property('const_b', .18)
connect(linear_color, '', lens, 'A')
connect(lens, '', base, 'B')
connect(linear_color, '', glow, 'A')
clock = pulse_node(unreal.MaterialExpressionTime, 'CityCivicBeaconTime')
frequency = pulse_node(unreal.MaterialExpressionMultiply, 'CityCivicBeaconFrequency')
frequency.set_editor_property('const_b', .5)
connect(clock, '', frequency, 'A')
side = pulse_node(unreal.MaterialExpressionSubtract, 'CityCivicBeaconSide')
side.set_editor_property('const_b', .25)
connect(vertex, 'B', side, 'A')
select = pulse_node(unreal.MaterialExpressionMultiply, 'CityCivicBeaconSelect')
select.set_editor_property('const_b', 4.0)
connect(side, '', select, 'A')
clamp = pulse_node(unreal.MaterialExpressionSaturate, 'CityCivicBeaconClamp')
connect(select, '', clamp, '')
# Evaluate one wave and its exact complement, rather than offsetting two waves.
# Every instant has constant combined brightness, including the crossovers.
sine = pulse_node(unreal.MaterialExpressionSine, 'CityCivicBeaconSine')
sine.set_editor_property('period', 1.0)
connect(frequency, '', sine, '')
amplitude = pulse_node(unreal.MaterialExpressionMultiply, 'CityCivicBeaconAmplitude')
amplitude.set_editor_property('const_b', .5)
connect(sine, '', amplitude, 'A')
red = pulse_node(unreal.MaterialExpressionAdd, 'CityCivicBeaconLevel')
red.set_editor_property('const_b', .5)
connect(amplitude, '', red, 'A')
opposite = pulse_node(unreal.MaterialExpressionOneMinus, 'CityCivicBeaconOpposite')
connect(red, '', opposite, '')
pair = pulse_node(unreal.MaterialExpressionLinearInterpolate, 'CityCivicBeaconPair')
connect(red, '', pair, 'A')
connect(opposite, '', pair, 'B')
connect(clamp, '', pair, 'Alpha')
level = pulse_node(unreal.MaterialExpressionMultiply, 'CityCivicBeaconOutputScale', 'CityCivicBeaconPhase')
level.set_editor_property('const_b', 4.0)
connect(pair, '', level, 'A')
enabled = pulse_node(unreal.MaterialExpressionScalarParameter, 'CityCivicBeaconEnabled')
enabled.set_editor_property('parameter_name', 'BeaconPulse')
enabled.set_editor_property('default_value', 0.0)
blend = pulse_node(unreal.MaterialExpressionLinearInterpolate, 'CityCivicBeaconBlend')
blend.set_editor_property('const_a', 0.0)
connect(level, '', blend, 'B')
connect(enabled, '', blend, 'Alpha')
connect(blend, '', strength, 'B')
m.set_editor_property('used_with_instanced_static_meshes', True)
output(base, unreal.MaterialProperty.MP_BASE_COLOR)
output(strength, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.layout_material_expressions(m)
lib.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m):
    raise RuntimeError('Civic detail material save failed')
for service in ['Police', 'Fire']:
    response_path = f'/Game/CitySim/Art/Materials/MI_City{service}ResponseDetail'
    instance = unreal.EditorAssetLibrary.load_asset(response_path)
    if not instance:
        folder, name = response_path.rsplit('/', 1)
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance, m)
    lib.set_material_instance_scalar_parameter_value(instance, 'BeaconPulse', 1.0)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
        raise RuntimeError(f'Civic response material save failed: {service}')
ready.write_text('ready', encoding='utf-8')
