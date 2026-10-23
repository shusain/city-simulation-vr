"""Explicit ten-source #51 CI import; retains generated packages for commit/review."""
from pathlib import Path
import hashlib
import json
import sys
import unreal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityBuildingAdoptionMaterials import create, save, node, link, prop, scalar, finish

assert Path(unreal.Paths.project_dir()).resolve() == (ROOT / 'CitySimVR').resolve()
doc = json.loads((ROOT / 'ArtSource/Manifests/city_beach_refinement51.json').read_bytes())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for row in doc['assets']:
    source = ROOT / row['source_directory']
    assert sha(source / 'approval.json') == row['approval_sha256']
    assert sha(source / 'asset.fbx') == row['source_fbx_sha256']
    assert sha(source / 'asset.blend') == row['source_blend_sha256']
    assert sha(ROOT / row['import_file']) == row['import_sha256']

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
solid = unreal.load_asset(doc['solid_master'])
assert solid and solid.get_editor_property('used_with_instanced_static_meshes')
changed = []

# The approved umbrella uses four PanelColor vertex colors in only two slots.
# Reuse the project's vertex-color gamma convention; no texture or custom shader.
vertex = create(doc['vertex_master'], unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(vertex)
vertex.set_editor_property('used_with_instanced_static_meshes', True)
vertex.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
colors = node(vertex, unreal.MaterialExpressionVertexColor)
linear = node(vertex, unreal.MaterialExpressionPower)
linear.set_editor_property('const_exponent', 2.2)
link(colors, linear, 'Base')
prop(linear, unreal.MaterialProperty.MP_BASE_COLOR)
roughness = node(vertex, unreal.MaterialExpressionScalarParameter)
roughness.set_editor_property('parameter_name', 'Roughness')
roughness.set_editor_property('default_value', .88)
prop(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
prop(scalar(vertex, .25), unreal.MaterialProperty.MP_SPECULAR)
finish(vertex)
changed.append(doc['vertex_master'])
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
results = []
for row in doc['assets']:
    bindings = []
    for material in row['materials']:
        instance = create(material['destination'], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, vertex if material['vertex_color'] else solid)
        if not material['vertex_color']:
            lib.set_material_instance_vector_parameter_value(instance, 'Color', unreal.LinearColor(*material['color']))
            lib.set_material_instance_scalar_parameter_value(instance, 'Metallic', 0.)
        lib.set_material_instance_scalar_parameter_value(instance, 'Roughness', material['roughness'])
        save(instance)
        bindings.append((material, instance))
        changed.append(material['destination'])
    folder, name = row['mesh_destination'].rsplit('/', 1)
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(ROOT / row['import_file']), destination_path=folder, destination_name=name,
                           automated=True, replace_existing=True, replace_existing_settings=True, save=False).items():
        task.set_editor_property(key, value)
    task.factory = unreal.FbxFactory()
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.set_editor_property('reset_to_fbx_on_material_conflict', True)
    for key, value in dict(combine_meshes=True, vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                           convert_scene_unit=True, convert_scene=True, import_uniform_scale=1., generate_lightmap_u_vs=False,
                           auto_generate_collision=False, remove_degenerates=False,
                           normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
        options.static_mesh_import_data.set_editor_property(key, value)
    task.options = options
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(row['mesh_destination'])
    assert mesh
    slots = [str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert len(slots) == len(bindings), (row['name'], slots)
    for index, slot in enumerate(slots):
        matched = [(m, instance) for m, instance in bindings if slot in m['aliases']]
        assert len(matched) == 1, (row['name'], slot)
        mesh.set_material(index, matched[0][1])
    build = editor.get_lod_build_settings(mesh, 0)
    build.set_editor_property('use_full_precision_u_vs', True)
    build.set_editor_property('recompute_normals', False)
    editor.set_lod_build_settings(mesh, 0, build)
    bounds = mesh.get_bounding_box()
    dimensions = bounds.max - bounds.min
    assert mesh.get_num_triangles(0) == row['triangles']
    assert mesh.get_num_sections(0) == len(bindings)
    assert all(abs(a - b * 100) < .2 for a, b in zip(dimensions.to_tuple(), row['dimensions_m'])), (row['name'], dimensions)
    assert abs(bounds.min.z) < .2, (row['name'], bounds.min.z)
    assert editor.get_num_uv_channels(mesh, 0) > 0
    assert not mesh.get_editor_property('nanite_settings').get_editor_property('enabled')
    save(mesh)
    changed.append(row['mesh_destination'])
    results.append(dict(issue=row['issue'], mesh=row['mesh_destination'], triangles=mesh.get_num_triangles(0),
                        slots=slots, dimensions_cm=list(dimensions.to_tuple()), ground_min_cm=bounds.min.z,
                        uv_channels=editor.get_num_uv_channels(mesh, 0), import_sha256=row['import_sha256']))

out = ROOT / 'Artifacts/CIAssetPreparation'
out.mkdir(parents=True, exist_ok=True)
files = ['CitySimVR/Content/' + path.removeprefix('/Game/') + '.uasset' for path in changed]
(out / 'Files.txt').write_text('\n'.join(files) + '\n')
(out / 'Beach51.json').write_text(json.dumps(dict(result='passed', assets=results, files=files,
                                                input_manifest_sha256=sha(ROOT / 'ArtSource/Manifests/city_beach_refinement51.json')), indent=2) + '\n')
print('CITYSIM_BEACH51_IMPORT_PASS', len(results), 'meshes', len(files), 'packages')
