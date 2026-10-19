"""Import only eight paired factory meshes and two new steel materials."""
from pathlib import Path
import json,hashlib,sys,unreal
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityIndustrialMetalMaterials import import_native_materials,STEEL_SLOT
OUT=ROOT/'Artifacts/D104/IndustrialMetal'
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text(encoding='utf-8'))
materials=import_native_materials(doc['industrial_metals']);rows=[]
for e in doc['assets']:
    if e['category']!='Factory':continue
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['export_sha256']
    task=unreal.AssetImportTask();package,name=e['mesh_destination'].rsplit('/',1)
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False
    ui.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
    task.options=ui;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination']);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')];assert set(slots)==set(e['slots']) and STEEL_SLOT in slots
    for i,slot in enumerate(slots):
        material=unreal.load_asset(e['material_bindings'][slot]);assert material;mesh.set_material(i,material)
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
    size=mesh.get_bounding_box().max-mesh.get_bounding_box().min;assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),e['dimensions_m']))
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    rows.append(dict(name=e['name'],triangles=e['triangles'],sections=e['sections'],dimensions_cm=list(size.to_tuple()),bindings=e['material_bindings']))
(OUT/'Import.json').write_text(json.dumps(dict(source_sha256=doc['source_sha256'],materials=materials,meshes=rows,catalog_unchanged=True),indent=2)+'\n',encoding='utf-8')
unreal.log('CITYSIM_INDUSTRIAL_METAL_NATIVE_PASS')
