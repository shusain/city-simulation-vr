"""Focused D66 foundation import: reuse accepted lawn/concrete materials only.

Run via Unreal Editor Python after reviewing the Blender render and exporting.
Success requires CITYSIM_RESIDENTIAL_GROUND_IMPORT_COMPLETE plus the measured
D66-ResidentialGround-Import.json; editor exit code alone is insufficient.
"""
from pathlib import Path
import json, traceback, unreal

ROOT=Path(__file__).resolve().parents[1]
REPORT_DIR=ROOT/'Artifacts/AssetImport'
REPORT_DIR.mkdir(parents=True,exist_ok=True)
REPORT=REPORT_DIR/'D66-ResidentialGround-Import.json'
ERROR=REPORT_DIR/'D66-ResidentialGround-Import.error.txt'
try:
    manifest=json.loads((ROOT/'ArtSource/Manifests/city_residential_ground.json').read_text(encoding='utf-8'))
    assert manifest['schema_version']==1 and len(manifest['assets'])==1
    entry=manifest['assets'][0]
    assert entry['name']=='SM_Building_LotFoundation_A'
    assert entry['triangles']==12 and entry['dimensions_m']==[1,1,1]
    assert entry['material_slots']=={'Lawn':'/Game/CitySim/Art/Materials/MI_CityPark',
                                    'Concrete':'/Game/CitySim/Art/Materials/MI_CityTraffic'}
    assert (ROOT/entry['source']).is_file() and (ROOT/entry['export']).is_file()
    materials={slot:unreal.load_asset(path) for slot,path in entry['material_slots'].items()}
    assert all(materials.values()),'Accepted lawn/concrete material missing'
    task=unreal.AssetImportTask()
    task.filename=str(ROOT/entry['export'])
    task.destination_path='/Game/CitySim/Art/Meshes';task.destination_name=entry['name']
    task.automated=True;task.replace_existing=True;task.save=True
    options=unreal.FbxImportUI()
    options.import_mesh=True;options.import_materials=False;options.import_textures=False
    options.import_as_skeletal=False
    options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
    options.static_mesh_import_data.set_editor_property('combine_meshes',True)
    options.static_mesh_import_data.set_editor_property('convert_scene_unit',True)
    task.options=options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(entry['mesh_destination'])
    assert mesh,'Foundation mesh import failed'
    slots=[str(slot.get_editor_property('imported_material_slot_name')) for slot in mesh.get_editor_property('static_materials')]
    assert set(slots)==set(entry['material_slots']) and len(slots)==2,slots
    editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for index,slot in enumerate(slots):
        mesh.set_material(index,materials[slot])
        editor.enable_section_collision(mesh,False,0,index)
    editor.remove_collisions(mesh)
    bounds=mesh.get_bounding_box()
    minimum=[bounds.min.x,bounds.min.y,bounds.min.z]
    maximum=[bounds.max.x,bounds.max.y,bounds.max.z]
    assert all(abs(value+50)<.01 for value in minimum),minimum
    assert all(abs(value-50)<.01 for value in maximum),maximum
    assert mesh.get_num_triangles(0)==12 and mesh.get_num_sections(0)==2
    assert all(not editor.is_section_collision_enabled(mesh,0,index) for index in range(2))
    assignments=[mesh.get_material(index).get_path_name().split('.')[0] for index in range(2)]
    assert assignments==[entry['material_slots'][slot] for slot in slots]
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    report=dict(errors=[],name=entry['name'],triangles=12,material_slots=slots,materials=assignments,
        bounds_min_cm=minimum,bounds_max_cm=maximum,pivot='centered',collision='none')
    REPORT.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    ERROR.unlink(missing_ok=True)
    print('CITYSIM_RESIDENTIAL_GROUND_IMPORT_COMPLETE',json.dumps(report))
except Exception:
    ERROR.write_text(traceback.format_exc(),encoding='utf-8')
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
