"""D108 approved beach FBX import; source-only prerequisites pinned by manifest."""
from pathlib import Path
import unreal,json,hashlib,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingAdoptionMaterials import create,save
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
doc=json.loads((ROOT/'ArtSource/Manifests/city_beach_integration.json').read_text())
out=ROOT/'Artifacts/D108/BeachIntegration';out.mkdir(parents=True,exist_ok=True)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
lib=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
parent=unreal.load_asset(doc['master']);assert parent and parent.get_editor_property('used_with_instanced_static_meshes')
assert parent.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_OPAQUE
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
results=[]
for r in doc['assets']:
    source=ROOT/r['sourceDirectory']
    assert sha(source/'approval.json')==r['approvalSha256'] and sha(source/'asset.fbx')==r['fbxSha256'] and sha(source/'asset.blend')==r['blendSha256']
    mi=create(r['materialDestination'],unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi,parent)
    lib.set_material_instance_vector_parameter_value(mi,'Color',unreal.LinearColor(*r['linearColor']))
    lib.set_material_instance_scalar_parameter_value(mi,'Roughness',r['roughness'])
    lib.set_material_instance_scalar_parameter_value(mi,'Metallic',0.)
    save(mi)
    folder,name=r['meshDestination'].rsplit('/',1);task=unreal.AssetImportTask()
    for k,v in dict(filename=str(source/'asset.fbx'),destination_path=folder,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();options=unreal.FbxImportUI()
    options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
    options.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,import_uniform_scale=1.,generate_lightmap_u_vs=False,auto_generate_collision=False,remove_degenerates=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
    task.options=options;tools.import_asset_tasks([task]);mesh=unreal.load_asset(r['meshDestination']);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert slots==[r['materialSlot']],(r['name'],slots)
    mesh.set_material(0,mi)
    build=editor.get_lod_build_settings(mesh,0);build.set_editor_property('use_full_precision_u_vs',True);build.set_editor_property('recompute_normals',False);editor.set_lod_build_settings(mesh,0,build)
    assert mesh.get_num_triangles(0)==r['triangles'] and mesh.get_num_sections(0)==1
    bounds=mesh.get_bounding_box();size=bounds.max-bounds.min
    assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),r['dimensions'])),(r['name'],size)
    assert abs(bounds.min.z)<.2 and editor.get_num_uv_channels(mesh,0)>0
    assert not mesh.get_editor_property('nanite_settings').get_editor_property('enabled')
    save(mesh)
    results.append(dict(issue=r['issue'],version=r['version'],mesh=r['meshDestination'],material=r['materialDestination'],triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),dimensionsCm=list(size.to_tuple()),boundsMinCm=list(bounds.min.to_tuple()),uvChannels=editor.get_num_uv_channels(mesh,0),fullPrecisionUVs=editor.get_lod_build_settings(mesh,0).get_editor_property('use_full_precision_u_vs'),importedSlots=slots,sourceFBXSha256=r['fbxSha256'],approvalSha256=r['approvalSha256']))
report=dict(status='passed',assets=results,masterReused=doc['master'],totalTriangles=sum(x['triangles'] for x in results),manifestSha256=sha(ROOT/'ArtSource/Manifests/city_beach_integration.json'),scope='Five new static meshes/five instances only; no catalog/default/player edits.')
(out/'NativeImport.json').write_text(json.dumps(report,indent=2)+'\n');unreal.log('CITYSIM_BEACH_IMPORT_READY')
