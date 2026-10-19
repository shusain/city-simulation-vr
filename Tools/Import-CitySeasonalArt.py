"""Focused D66 import/measurement; never reimports previous non-seasonal art.

Run using the installed Unreal Editor Python pipeline after Blender review and
export. -DryRun validates source contracts only. A successful import writes
D66-SeasonalImport.json and CITYSIM_SEASONAL_IMPORT_COMPLETE;
-SeasonalFollowupOnly imports just the six refinement meshes and writes
D66-Refinement-SeasonalImport.json, preserving original meshes/materials; exit code alone
does not prove Python succeeded in Unreal. -SeasonalFollowupOnly imports only
the six refinement assets and writes D66-Refinement-SeasonalImport.json.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import json, traceback, unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityToolPaths import repository_root
from Assets.Manifests.CityArtManifest import load_seasonal_manifest

ROOT = repository_root(__file__)
REPORT_DIR = ROOT / 'Artifacts/AssetImport'
REPORT_DIR.mkdir(parents=True, exist_ok=True)
FOLLOWUP = '-SeasonalFollowupOnly' in unreal.SystemLibrary.get_command_line()
RESIDENTIAL_GROUND = any(flag in unreal.SystemLibrary.get_command_line()
    for flag in ('-ResidentialGroundOnly','--residential-ground-only'))
REPORT = REPORT_DIR / ('D66-ResidentialGround-HomeImport.json' if RESIDENTIAL_GROUND else 'D66-Refinement-SeasonalImport.json' if FOLLOWUP else 'D66-SeasonalImport.json')
ERROR = REPORT.with_suffix('.error.txt')
DRY_RUN = '-DryRun' in unreal.SystemLibrary.get_command_line()


try:
    manifest = load_seasonal_manifest(ROOT)
    entries = manifest['assets']
    measurements = []
    if not DRY_RUN:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        parent = unreal.load_asset(manifest['material_parent'])
        assert parent, 'Missing existing standard lit color parent'
        materials = {}
        for path, color in manifest['material_colors'].items():
            material = unreal.load_asset(path)
            # Existing environment instances are shared accepted assets. Only
            # create/update this kit's MI_Seasonal instances.
            if '/MI_Seasonal' in path and (not (FOLLOWUP or RESIDENTIAL_GROUND) or not material):
                name = path.rsplit('/', 1)[1]
                material = material or tools.create_asset(name, path.rsplit('/', 1)[0],
                    unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                assert material, path
                unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                    material, 'CityColor', unreal.LinearColor(*color, 1))
                unreal.EditorAssetLibrary.save_loaded_asset(material)
            assert material, path
            materials[path] = material
        for entry in entries:
            for path in entry['material_slots'].values():
                if path not in materials:
                    materials[path] = unreal.load_asset(path)
                    assert materials[path], path
        for entry in entries:
            if RESIDENTIAL_GROUND and entry['name']!='SM_Building_Residential_SeasonalSpace_A':
                continue
            if FOLLOWUP and entry['name'] not in {
                'SM_Seasonal_LeafPile_A','SM_Seasonal_WreathAutumn_A','SM_Seasonal_WreathWinter_A',
                'SM_Building_Residential_SeasonalSpace_A','SM_Building_Park_December_A',
                'SM_Building_NeighborhoodPark_December_A'}:
                continue
            task = unreal.AssetImportTask()
            existing=unreal.load_asset(entry['mesh_destination'])
            if existing:
                existing_data=existing.get_editor_property('asset_import_data')
                if isinstance(existing_data,unreal.FbxStaticMeshImportData):existing_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
            task.filename = str(ROOT / entry['export'])
            task.destination_path = '/Game/CitySim/Art/Meshes'
            task.destination_name = entry['name']
            task.automated = True
            task.replace_existing = True
            task.replace_existing_settings = True
            task.save = True
            task.factory = unreal.FbxFactory()
            options = unreal.FbxImportUI()
            options.set_editor_property('reset_to_fbx_on_material_conflict',True)
            options.import_mesh = True
            options.import_materials = False
            options.import_textures = False
            options.import_as_skeletal = False
            options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
            options.static_mesh_import_data.set_editor_property('combine_meshes', True)
            options.static_mesh_import_data.set_editor_property('convert_scene_unit', True)
            options.static_mesh_import_data.set_editor_property('convert_scene', True)
            options.static_mesh_import_data.set_editor_property('import_uniform_scale', 1.0)
            options.static_mesh_import_data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            task.options = options
            tools.import_asset_tasks([task])
            mesh = unreal.load_asset(entry['mesh_destination'])
            assert mesh, entry['name']
            imported_data=mesh.get_editor_property('asset_import_data')
            if isinstance(imported_data,unreal.FbxStaticMeshImportData) and imported_data.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
                imported_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE);tools.import_asset_tasks([task]);mesh=unreal.load_asset(entry['mesh_destination'])
            slots = [str(slot.get_editor_property('imported_material_slot_name'))
                     for slot in mesh.get_editor_property('static_materials')]
            assert len(slots) == len(entry['material_slots']) and set(slots) == set(entry['material_slots']), slots
            for index, slot in enumerate(slots):
                mesh.set_material(index, materials[entry['material_slots'][slot]])
                mesh_editor.enable_section_collision(mesh, False, 0, index)
            if has_capability(entry, 'paired_park_lods'):
                assert mesh_editor.import_lod(mesh,1,str(ROOT/entry['native_lod_export']))==1
                assert mesh.get_num_triangles(1)==entry['native_lod_triangles']
                mesh_editor.set_lod_screen_sizes(mesh,[1,.025]);mesh.set_minimum_lod_for_platform('Android',1)
            mesh_editor.remove_collisions(mesh)
            bounds = mesh.get_bounding_box()
            extent = bounds.max - bounds.min
            actual = [extent.x/100, extent.y/100, extent.z/100]
            assert all(abs(a-b) < .02 for a,b in zip(sorted(actual[:2])+actual[2:],
                sorted(entry['dimensions_m'][:2])+entry['dimensions_m'][2:])), (entry['name'], actual)
            expected_minimum_cm=-20 if entry['name']=='SM_Building_Residential_SeasonalSpace_A' else 0
            assert abs(bounds.min.z-expected_minimum_cm) < .1, (entry['name'], bounds.min.z)
            triangles = mesh.get_num_triangles(0)
            assert triangles == entry['triangles'] and triangles <= (entry['triangles'] if has_capability(entry, 'paired_park_lods') else entry['triangle_limit'])
            assert mesh.get_num_sections(0) == len(slots)
            assert all(not mesh_editor.is_section_collision_enabled(mesh, 0, index) for index in range(len(slots)))
            assignments = [mesh.get_material(index).get_path_name().split('.')[0] for index in range(len(slots))]
            assert assignments == [entry['material_slots'][slot] for slot in slots]
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            measurements.append(dict(name=entry['name'],triangles=triangles,dimensions_m=actual,
                material_slots=slots,materials=assignments,collision='none',
                bounds_min_cm=[bounds.min.x,bounds.min.y,bounds.min.z],
                bounds_max_cm=[bounds.max.x,bounds.max.y,bounds.max.z]))
    REPORT.write_text(json.dumps(dict(task='D66',dry_run=DRY_RUN,errors=[],
        assets=len(entries),measurements=measurements),indent=2)+'\n',encoding='utf-8')
    ERROR.unlink(missing_ok=True)
    print('CITYSIM_SEASONAL_IMPORT_COMPLETE', json.dumps(dict(assets=len(entries),imported=len(measurements),dry_run=DRY_RUN)))
except Exception:
    ERROR.write_text(traceback.format_exc(), encoding='utf-8')
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
