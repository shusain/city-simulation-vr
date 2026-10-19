"""Import the refined shared room cube and persist only reflection capture data."""
from pathlib import Path
import json,hashlib,traceback
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/LowDensity/Integration'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
try:
    assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
    source=ROOT/'ArtSource/Textures/BuildingSurfaces/city_room_reflection.hdr'
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(source),destination_path='/Game/CitySim/Art/Textures',destination_name='T_CityRoomReflection',automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.TextureFactory();unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    cube=unreal.load_asset('/Game/CitySim/Art/Textures/T_CityRoomReflection');assert isinstance(cube,unreal.TextureCube)
    cube.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_HDR);cube.set_editor_property('srgb',False);cube.set_editor_property('max_texture_size',64)
    assert unreal.EditorAssetLibrary.save_loaded_asset(cube)
    assert unreal.CityMetalReflectionAssetsCommandlet.bake_room_reflection()==0
    paths=['CitySimVR/Content/CitySim/Maps/CitySandbox.umap','CitySimVR/Content/CitySim/Maps/CitySandbox_BuiltData.uasset','CitySimVR/Content/CitySim/Art/Textures/T_CityRoomReflection.uasset']
    (OUT/'ReflectionBake.json').write_text(json.dumps(dict(task='D104',source=str(source.relative_to(ROOT)),source_sha256=digest(source),cube=cube.get_path_name(),cube_max_size=64,lightmass_built=False,live_recapture=False,reflection_build_data_saved=True,
        files=[dict(path=p,sha256=digest(ROOT/p)) for p in paths]),indent=2)+'\n')
    unreal.log('CITYSIM_D104_REFLECTION_BAKE_PASS')
except Exception:
    (OUT/'BakeError.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
