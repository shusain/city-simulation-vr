"""#125 CI preparation of pathless commercial meshes and themed material copies only."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[3]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    assert Path(unreal.Paths.project_dir()).resolve() == (ROOT / 'CitySimVR').resolve()
    doc = json.loads((ROOT / 'ArtSource/Manifests/city_commercial_ground.json').read_text())
    assert digest(ROOT / doc['source']) == doc['source_sha256']
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    files, results = [], []
    for entry in doc['assets']:
        assert digest(ROOT / entry['derived_export']) == entry['derived_export_sha256']
        assert entry['target'].startswith('/Game/CitySim/Art/CommercialGround/')
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(ROOT / entry['derived_export']), destination_path='/Game/CitySim/Art/CommercialGround',
                               destination_name=entry['target_name'], automated=True, replace_existing=True, save=False).items():
            task.set_editor_property(key, value)
        task.factory = unreal.FbxFactory()
        options = unreal.FbxImportUI()
        options.import_mesh, options.import_materials, options.import_textures, options.import_as_skeletal = True, False, False, False
        for key, value in dict(combine_meshes=True, vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                               convert_scene_unit=True, convert_scene=True, import_uniform_scale=1., generate_lightmap_u_vs=True,
                               auto_generate_collision=False, normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
            options.static_mesh_import_data.set_editor_property(key, value)
        task.options = options
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(entry['target'])
        assert mesh and mesh.get_num_triangles(0) == entry['triangles']
        slots = [str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        assert set(slots) == set(entry['bindings']) and len(slots) == len(entry['bindings']), (entry['target'], slots)
        for index, slot in enumerate(slots):
            material = unreal.load_asset(entry['bindings'][slot])
            assert material
            mesh.set_material(index, material)
        bounds = mesh.get_bounding_box()
        size = bounds.max - bounds.min
        actual = [size.x, size.y, size.z]
        assert max(abs(a - b * 100) for a, b in zip(actual, entry['dimensions_m'])) < .05
        assert unreal.CityEditorAssetLibrary.repoint_imported_source(mesh, str(ROOT / entry['derived_export']), 0)
        imported = list(unreal.CityEditorAssetLibrary.get_imported_source_files(mesh))
        assert len(imported) == 1 and Path(imported[0]).resolve() == (ROOT / entry['derived_export']).resolve(), imported
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        relative = 'CitySimVR/Content/CitySim/Art/CommercialGround/' + entry['target_name'] + '.uasset'
        files.append(relative)
        results.append(dict(target=entry['target'], file=relative, sha256=digest(ROOT / relative), bytes=(ROOT / relative).stat().st_size,
                            triangles=mesh.get_num_triangles(0), slots=slots, dimensions_cm=actual,
                            entry_native_cm=entry['entry_native_cm'], landing_cm=entry['landing_cm'],
                            ground_reserve_cm=entry['ground_reserve_cm'], source_sha256=entry['derived_export_sha256']))
        print('COMMERCIAL_GROUND_NATIVE', entry['target'], mesh.get_num_triangles(0))
    out = ROOT / 'Artifacts/CIAssetPreparation'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'Files.txt').write_text('\n'.join(files) + '\n', encoding='utf-8')
    (out / 'CommercialGround.json').write_text(json.dumps(dict(issue=125, files=results), indent=2), encoding='utf-8')
    print('COMMERCIAL_GROUND_IMPORT_PASS', len(results))

if __name__ == '__main__':
    main()
