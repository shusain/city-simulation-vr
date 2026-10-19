"""Author original sprite masks and native weather assets; no map/service changes.

Run with bundled Python --textures to regenerate the tiny PNG source masks.
Then run in Unreal's Python editor to import masks/materials and create emitters.
"""
from pathlib import Path
import math, struct, sys, zlib, json

root = Path(__file__).resolve().parents[1]

def masks():
    source = root / 'ArtSource/Textures/Weather'
    source.mkdir(parents=True, exist_ok=True)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    for name in ('Drop', 'Splash'):
        pixels = bytearray()
        for y in range(32):
            pixels.append(0)
            for x in range(32):
                radius = math.hypot((x + .5) / 16 - 1, (y + .5) / 16 - 1)
                alpha = max(0, min(1, (1 - radius) * 4)) if name == 'Drop' else max(0, 1 - abs(radius - .72) * 10)
                pixels.extend((255, 255, 255, round(alpha * 255)))
        header = struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)
        (source / ('T_CityWeather' + name + '.png')).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))

if '--textures' in sys.argv:
    masks()
else:
    import unreal
    masks()
    path = '/Game/CitySim/Art/Weather'
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    for name in ('Drop', 'Splash'):
        task = unreal.AssetImportTask()
        task.filename = str(root / ('ArtSource/Textures/Weather/T_CityWeather' + name + '.png'))
        task.destination_path = path
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(path + '/T_CityWeather' + name)
        texture.set_editor_property('srgb', False)
        texture.set_editor_property('never_stream', True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        material_name = 'M_CityWeather' + name
        material = unreal.load_asset(path + '/' + material_name)
        existing = material is not None
        material = material or tools.create_asset(material_name, path, unreal.Material, unreal.MaterialFactoryNew())
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('used_with_particle_sprites', True)
        # CDO references root these materials. Repair the known graph in place:
        # deleting its expressions during startup asserts on rooted objects.
        if existing:
            color = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            opacity = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY)
            inputs = lib.get_inputs_for_material_expression(material, opacity)
            mask = inputs[1]
            if not isinstance(color, unreal.MaterialExpressionParticleColor) or not isinstance(opacity, unreal.MaterialExpressionMultiply) or not isinstance(mask, unreal.MaterialExpressionTextureSample):
                raise RuntimeError('Unrecognized weather material graph: ' + material_name)
        else:
            color = lib.create_material_expression(material, unreal.MaterialExpressionParticleColor, -400, 0)
            mask = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 200)
            opacity = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, 200)
        mask.set_editor_property('texture', texture)
        connections = [lib.connect_material_expressions(color, 'A', opacity, 'A'),
                       lib.connect_material_expressions(mask, 'A', opacity, 'B'),
                       lib.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                       lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)]
        if not all(connections):
            raise RuntimeError('Weather sprite material input connection failed: ' + material_name)
        lib.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    if not unreal.CityWeatherParticleComponent.create_weather_assets():
        raise RuntimeError('Native particle asset creation failed')
    manifest = {'provenance': 'Original project procedural 32x32 radial/ring alpha masks; native Engine sprite particles',
                'systems': {'P_CityRain': {'drops': 320, 'splashes': 48}, 'P_CitySnow': {'flakes': 320}},
                'simulation': 'CPU packed buffers; asynchronous immutable terrain snapshot; no mesh particles, lights, scene collision, sorting or warmup',
                'materials': 'Unlit translucent, depth tested, two triangles per sprite; small masks; no custom shader/plugin'}
    (root / 'ArtSource/Manifests/city_weather_particles.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('CITYSIM_WEATHER_PARTICLES_CREATED')
