"""D121 import one approved walk and one stock idle/walk blend; no mesh edits."""
from pathlib import Path
import unreal,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D121/Integration'
assert not (OUT/'Import.json').exists(),'Preserve existing import receipt'
assert '**Native window OWNED BY D121' in (ROOT/'docs/DEVELOPMENT_TASKS.md').read_text(encoding='utf-8-sig')
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_walk_native.json').read_text())
assert hashlib.sha256((ROOT/doc['export']).read_bytes()).hexdigest()==doc['sha256']
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
skeleton=unreal.load_asset('/Game/CitySim/Art/Parks/Meshes/SK_Park_Skate_Casual_Skeleton');assert skeleton
idle=unreal.load_asset('/Game/CitySim/Art/Parks/Animations/AS_Park_Idle');assert idle
dest='/Game/CitySim/Art/Citizens/AS_Citizen_Walk'
assert not unreal.EditorAssetLibrary.does_asset_exist(dest),'Existing native walk preserved'
ui=unreal.FbxImportUI();ui.set_editor_property('automated_import_should_detect_type',False)
for k,v in dict(import_mesh=False,import_animations=True,import_as_skeletal=True,import_materials=False,
 import_textures=False,skeleton=skeleton,mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION,
 original_import_type=unreal.FBXImportType.FBXIT_ANIMATION).items():ui.set_editor_property(k,v)
data=ui.get_editor_property('anim_sequence_import_data');data.set_editor_property('use_default_sample_rate',False);data.set_editor_property('custom_sample_rate',24)
task=unreal.AssetImportTask()
for k,v in dict(filename=str(ROOT/doc['export']),destination_path=dest.rsplit('/',1)[0],destination_name=dest.rsplit('/',1)[1],
 automated=True,replace_existing=False,save=False).items():task.set_editor_property(k,v)
task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);walk=unreal.load_asset(dest);assert walk
assert unreal.CityParkAssetLibrary.bind_matching_park_animation(walk,skeleton)
assert abs(walk.get_editor_property('sequence_length')-doc['seconds'])<.002
walk.set_editor_property('enable_root_motion',False)
assert unreal.EditorAssetLibrary.save_loaded_asset(walk)
blend=unreal.CityCitizenWalkAssetLibrary.create_walk_blend(idle,walk);assert blend
assert unreal.EditorAssetLibrary.save_loaded_asset(blend)
(OUT/'Import.json').write_text(json.dumps(dict(task='D121.2',source_sha256=doc['source_sha256'],assets=[walk.get_path_name(),blend.get_path_name()],
 reused_skeleton=skeleton.get_path_name(),seconds=walk.get_editor_property('sequence_length'),mesh_or_skeleton_saved=False),indent=2)+'\n')
unreal.log('CITYSIM_D121_IMPORT_PASS one sequence;one blend;existing meshes and skeleton retained')
