"""Import the native HDR cube used by the single baked tabletop reflection."""
from pathlib import Path
import json
import unreal

root = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(root / 'ArtSource/Textures/BuildingSurfaces/city_room_reflection.hdr')
task.destination_path = '/Game/CitySim/Art/Textures'
task.destination_name = 'T_CityRoomReflection'
task.automated = True
task.replace_existing = True
task.save = True
task.factory = unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
cube = unreal.load_asset('/Game/CitySim/Art/Textures/T_CityRoomReflection')
assert isinstance(cube, unreal.TextureCube), str(cube)
cube.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
cube.set_editor_property('srgb', False)
cube.set_editor_property('max_texture_size', 64)
unreal.EditorAssetLibrary.save_loaded_asset(cube)
out = root / 'Artifacts/D79-MetalD71/reflection-import.json'
out.write_text(json.dumps(dict(asset=cube.get_path_name(), native_class=cube.get_class().get_name(),
    hdr=True, srgb=False, max_cube_size=64), indent=2) + '\n')
unreal.log('CITYSIM_ROOM_REFLECTION_IMPORTED')
