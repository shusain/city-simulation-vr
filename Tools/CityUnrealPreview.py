"""Renderer-independent camera and manifest helpers for paired asset studies."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
OFFSETS = {'iso': (23, -32, 21), 'front': (0, -40, 8),
           'back': (-23, 32, 21), 'entrance': (2, -18, 4)}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def camera(record, view):
    category = record['category']
    if view == 'entrance':
        aim = (0 if category == 'Shop' else .25, -7.7, 1.85)
        width = 7.1 if category == 'Shop' else 13.5
    else:
        aim = (0, -.9, {'Home': 2.2, 'Shop': 5, 'Factory': 7.4}[category])
        width = {'Home': 20, 'Shop': 28, 'Factory': 32}[category]
    return dict(target_m=list(aim), position_m=[a+b for a, b in zip(aim, OFFSETS[view])],
                ortho_width_m=width)


def unreal_cm(point):
    # Legacy FBX importer: Blender -Y front becomes Unreal +Y.
    return [point[0]*100, -point[1]*100, point[2]*100]


def load_batch(filename):
    batch = json.loads(Path(filename).read_text(encoding='utf-8'))
    assert batch['schema_version'] == 1 and batch['renders']
    project = (ROOT/batch['project']).resolve()
    assert project.is_relative_to((ROOT/'Artifacts').resolve()), 'Use an isolated artifact project'
    assert (ROOT/batch['output']).resolve().is_relative_to((ROOT/'Artifacts').resolve()), 'Keep preview outputs in Artifacts'
    for entry in batch['renders']:
        for key in ('key', 'group', 'view'):
            value = entry[key]
            assert value not in ('.', '..') and Path(value).name == value and '/' not in value and '\\' not in value, 'Preview keys/groups/views must be plain names'
    assert digest(ROOT/batch['source']) == batch['source_sha256'], 'Study source changed; prepare batch again'
    names = [item['key'] for item in batch['renders']]
    assert len(set(names)) == len(names), 'Duplicate render keys'
    return batch


def environment(batch, refresh=False):
    """Resolve/import only a study's original HDR reflection cube in isolated UE."""
    import unreal
    record = batch.get('preview_environment')
    if not record:
        return unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
    assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/batch['project']).parent.resolve()
    assert record['native'].startswith('/Game/CityPreview/Textures/')
    path = ROOT/record['file']; assert digest(path) == record['sha256']
    texture = unreal.load_asset(record['native']) if unreal.EditorAssetLibrary.does_asset_exist(record['native']) else None
    if texture is None or refresh:
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(path), destination_path=record['native'].rsplit('/', 1)[0],
                destination_name=record['native'].rsplit('/', 1)[1], automated=True, replace_existing=True, save=False).items():
            task.set_editor_property(key, value)
        task.factory = unreal.TextureFactory(); unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(record['native']); assert isinstance(texture, unreal.TextureCube), 'HDR must import as a long-lat cube'
        texture.set_editor_property('srgb', False)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
        assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture
