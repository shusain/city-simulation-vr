"""Import exact approved #201 v2 without changing approved source bytes."""
from pathlib import Path
import hashlib
import json
import sys
import unreal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityBuildingAdoptionMaterials import create, save

# Full bundle/approval provenance was verified once at source intake. Unreal
# consumes this FBX; pin its approved bytes independently of JSON formatting.
source = ROOT / 'ArtSource/Generated/blender/issue-201/v2'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
fbx = source / 'asset.fbx'
assert fbx.stat().st_size == 19084
assert sha(fbx) == 'bf669abc5e43c367c81b2ec1c51e214cb772d31d253e69aaf2afe9dc71900c6f'
assert Path(unreal.Paths.project_dir()).resolve() == (ROOT / 'CitySimVR').resolve()

material_path = '/Game/CitySim/Art/Beach/Materials/MI_BeachCurledKelp_Kelp'
mesh_path = '/Game/CitySim/Art/Beach/Meshes/SM_Scenery_CurledKelp_A'
material = create(material_path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
lib = unreal.MaterialEditingLibrary
lib.set_material_instance_parent(material, unreal.load_asset('/Game/CitySim/Art/Technology/Materials/M_TechnologySolid'))
# Exact approved recipe's Kelp base colour and roughness.
lib.set_material_instance_vector_parameter_value(material, 'Color', unreal.LinearColor(.19, .175, .065, 1))
lib.set_material_instance_scalar_parameter_value(material, 'Roughness', .78)
lib.set_material_instance_scalar_parameter_value(material, 'Metallic', 0)
save(material)

task = unreal.AssetImportTask()
folder, name = mesh_path.rsplit('/', 1)
for key, value in dict(filename=str(source / 'asset.fbx'), destination_path=folder, destination_name=name,
                       automated=True, replace_existing=True, replace_existing_settings=True, save=False).items():
    task.set_editor_property(key, value)
task.factory = unreal.FbxFactory()
options = unreal.FbxImportUI()
for key, value in dict(import_mesh=True, import_materials=False, import_textures=False, import_animations=False,
                       import_as_skeletal=False, mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH).items():
    options.set_editor_property(key, value)
options.set_editor_property('reset_to_fbx_on_material_conflict', True)
for key, value in dict(combine_meshes=True, vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                       convert_scene_unit=True, convert_scene=True, remove_degenerates=False,
                       auto_generate_collision=False, generate_lightmap_u_vs=False,
                       import_uniform_scale=1., normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
    options.static_mesh_import_data.set_editor_property(key, value)
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(mesh_path)
assert mesh and mesh.get_num_triangles(0) == 124 and mesh.get_num_sections(0) == 1
assert str(mesh.get_editor_property('static_materials')[0].get_editor_property('imported_material_slot_name')) == 'Kelp'
mesh.set_material(0, material)
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
build = editor.get_lod_build_settings(mesh, 0)
build.set_editor_property('use_full_precision_u_vs', True)
build.set_editor_property('recompute_normals', False)
editor.set_lod_build_settings(mesh, 0, build)
bounds = mesh.get_bounding_box()
dimensions = list((bounds.max - bounds.min).to_tuple())
assert all(abs(a - b) < .2 for a, b in zip(dimensions, [70, 35, 10])), dimensions
assert abs(bounds.min.z) < .2, bounds.min.z
assert editor.get_num_uv_channels(mesh, 0) > 0
assert not mesh.get_editor_property('nanite_settings').get_editor_property('enabled')
save(mesh)
out = ROOT / 'Artifacts/CIAssetPreparation'
out.mkdir(parents=True, exist_ok=True)
files = ['CitySimVR/Content/' + p.removeprefix('/Game/') + '.uasset' for p in [material_path, mesh_path]]
(out / 'Files.txt').write_text('\n'.join(files) + '\n')
(out / 'CurledKelp201.json').write_text(json.dumps(dict(status='passed', issue=201, version=2,
    files=files, triangles=124, dimensions_cm=dimensions, ground_min_cm=bounds.min.z,
    source_fbx_sha256=sha(source / 'asset.fbx')), indent=2))
print('CITYSIM_CURLED_KELP_IMPORT_PASS', len(files), 'packages')
