"""D64 palm-only import; existing environment meshes and terrain graph stay intact.
Run through the installed editor Python pipeline after reviewing the Blender render.
"""
from pathlib import Path
import json, traceback, unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/AssetImport'
out.mkdir(parents=True, exist_ok=True)
try:
    manifest = json.loads((root / 'ArtSource/Manifests/city_environment_kit.json').read_text(encoding='utf-8'))
    entry = next(asset for asset in manifest['assets'] if asset['name'] == 'SM_Scenery_Palm_A')
    assert entry['triangles'] <= 400 and entry['material_slots'] == ['Bark', 'Canopy']
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    task = unreal.AssetImportTask()
    task.filename = str(root / entry['export'])
    task.destination_path = '/Game/CitySim/Art/Meshes'
    task.destination_name = entry['name']
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    options.static_mesh_import_data.set_editor_property('combine_meshes', True)
    options.static_mesh_import_data.set_editor_property('convert_scene_unit', True)
    task.options = options
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(task.destination_path + '/' + entry['name'])
    if not mesh:
        raise RuntimeError('Palm import failed')
    slots = []
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        name = str(slot.get_editor_property('imported_material_slot_name'))
        assert name in entry['material_slots'], name
        material = unreal.load_asset('/Game/CitySim/Art/Materials/MI_Environment' + name)
        assert material, name
        mesh.set_material(index, material)
        slots.append(name)
    assert slots == entry['material_slots'], slots
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    bounds = mesh.get_bounds()
    assert bounds.box_extent.z > 250 and bounds.box_extent.z < 450, str(bounds)
    report = {'name': entry['name'], 'triangles': entry['triangles'], 'material_slots': slots,
              'collision': 'none', 'bounds_extent_cm': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}
    (out / 'D64-SceneryImport.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('CITYSIM_SCENERY_IMPORT_COMPLETE', json.dumps(report))
except Exception:
    (out / 'D64-SceneryImport.error.txt').write_text(traceback.format_exc(), encoding='utf-8')
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
