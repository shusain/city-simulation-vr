"""Import only the five approved D90 technology buildings and explicit native materials."""
from pathlib import Path
import unreal,json,hashlib,sys,runpy
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingAdoptionMaterials import create,save,node,link,prop,scalar,color,multiply,finish
OUT=ROOT/'Artifacts/D90/TechnologyIntegration';OUT.mkdir(parents=True,exist_ok=True)
doc=json.loads((ROOT/'ArtSource/Manifests/city_technology_building_integration.json').read_text(encoding='utf-8'))
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for row in doc['assets']:
 folder=ROOT/row['sourceDirectory'];assert sha(folder/'approval.json')==row['approvalSha256']
 assert sha(folder/'asset.fbx')==row['exportSha256'] and sha(folder/'asset.blend')==row['sourceBlendSha256']
lib=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();scope='/Game/CitySim/Art/Technology/Materials'
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
def parameter(m,name,value,vector=False):
 n=node(m,unreal.MaterialExpressionVectorParameter if vector else unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name)
 n.set_editor_property('default_value',unreal.LinearColor(*value,1) if vector else value);return n
def master(name):
 m=create(scope+'/M_'+name,unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m)
 m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE);return m
solid=master('TechnologySolid');prop(parameter(solid,'Color',[.5,.5,.5],True),unreal.MaterialProperty.MP_BASE_COLOR);prop(parameter(solid,'Roughness',.6),unreal.MaterialProperty.MP_ROUGHNESS);prop(parameter(solid,'Metallic',0.),unreal.MaterialProperty.MP_METALLIC);solidStats=finish(solid)
mapped=master('TechnologyMapped');samples={}
for suffix,sampler in [('BaseColor',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),('Normal',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),('ORM',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]:
 n=node(mapped,unreal.MaterialExpressionTextureSampleParameter2D);n.set_editor_property('parameter_name',suffix);n.set_editor_property('texture',unreal.load_asset('/Game/CitySim/Art/Textures/T_CityBrick_'+suffix));n.set_editor_property('sampler_type',sampler);samples[suffix]=n
prop(samples['BaseColor'],unreal.MaterialProperty.MP_BASE_COLOR)
rg=node(mapped,unreal.MaterialExpressionComponentMask)
for c in ['r','g','b','a']:rg.set_editor_property(c,c in ['r','g'])
link(samples['Normal'],rg,'');scaled=multiply(mapped,rg,parameter(mapped,'NormalStrength',1.));blue=node(mapped,unreal.MaterialExpressionComponentMask)
for c in ['r','g','b','a']:blue.set_editor_property(c,c=='b')
link(samples['Normal'],blue,'')
joined=node(mapped,unreal.MaterialExpressionAppendVector);link(scaled,joined,'A');link(blue,joined,'B');normal=node(mapped,unreal.MaterialExpressionNormalize);link(joined,normal,'VectorInput');prop(normal,unreal.MaterialProperty.MP_NORMAL)
prop(samples['ORM'],unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R');prop(samples['ORM'],unreal.MaterialProperty.MP_ROUGHNESS,'G');prop(samples['ORM'],unreal.MaterialProperty.MP_METALLIC,'B');mappedStats=finish(mapped)
materialRecords=[]
for row in doc['assets']:
 for slot,d in row['material_bindings'].items():
  parent=unreal.load_asset(d['parent']) if d['kind']=='civic' else mapped if d['kind']=='raw-maps' else solid
  assert parent and parent.get_editor_property('used_with_instanced_static_meshes')
  mi=create(d['destination'],unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());lib.set_material_instance_parent(mi,parent)
  if d['kind']=='civic':lib.set_material_instance_vector_parameter_value(mi,'CityColor',unreal.LinearColor(*d['tint'],1))
  elif d['kind']=='raw-maps':
   for suffix in ['BaseColor','Normal','ORM']:
    t=unreal.load_asset('/Game/CitySim/Art/Textures/T_'+d['prefix']+'_'+suffix);assert t
    assert t.get_editor_property('srgb')==(suffix=='BaseColor')
    if suffix=='Normal':assert t.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP
    lib.set_material_instance_texture_parameter_value(mi,suffix,t)
   lib.set_material_instance_scalar_parameter_value(mi,'NormalStrength',d['normal_strength'])
  else:
   lib.set_material_instance_vector_parameter_value(mi,'Color',unreal.LinearColor(*d['color'],1))
   for key in ['Roughness','Metallic']:lib.set_material_instance_scalar_parameter_value(mi,key,d[key.lower()])
  save(mi);materialRecords.append({'destination':d['destination'],'kind':d['kind'],'parent':parent.get_path_name()})
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
records=[]
for row in doc['assets']:
 package,name=row['mesh_destination'].rsplit('/',1);task=unreal.AssetImportTask()
 for k,v in dict(filename=str(ROOT/row['sourceDirectory']/'asset.fbx'),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
 task.factory=unreal.FbxFactory();options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False;options.set_editor_property('reset_to_fbx_on_material_conflict',True)
 for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,import_uniform_scale=1.,generate_lightmap_u_vs=False,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
 task.options=options;tools.import_asset_tasks([task]);mesh=unreal.load_asset(row['mesh_destination']);assert mesh
 slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')];assert set(slots)==set(row['slots']) and len(slots)==5,(row['definition_id'],slots)
 for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(row['material_bindings'][slot]['destination']))
 build=mesh_editor.get_lod_build_settings(mesh,0)
 build.set_editor_property('use_full_precision_u_vs',True)
 mesh_editor.set_lod_build_settings(mesh,0,build)
 assert mesh.get_num_triangles(0)==row['triangles'] and mesh.get_num_sections(0)==5
 bounds=mesh.get_bounding_box();size=bounds.max-bounds.min
 assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),row['dimensions_m'])),(row['definition_id'],size)
 assert abs(bounds.min.z)<.2 and mesh_editor.get_num_uv_channels(mesh,0)>0
 save(mesh);records.append({'definition':row['definition_id'],'mesh':row['mesh_destination'],'triangles':mesh.get_num_triangles(0),'sections':mesh.get_num_sections(0),'uv_channels':mesh_editor.get_num_uv_channels(mesh,0),'full_precision_uvs':mesh_editor.get_lod_build_settings(mesh,0).get_editor_property('use_full_precision_u_vs'),'dimensions_cm':list(size.to_tuple()),'slots':slots,'materials':[mesh.get_material(i).get_path_name() for i in range(5)]})
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog');assert catalog
# Resolve the catalog merge from accepted main, then append the existing D90 pilot definitions.
# Never replace main's newer health/waste/catalog data with the older D90 binary asset.
if not any(str(d.definition_id)=='AIInstitute' for d in catalog.get_editor_property('definitions')):
 runpy.run_path(str(ROOT/'Tools/Update-CityAiCatalog.py'))
 catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
definitions=list(catalog.get_editor_property('definitions'));before={str(d.definition_id):d.export_text() for d in definitions}
for row in doc['assets']:
 d=next(d for d in definitions if str(d.definition_id)==row['definition_id']);assert [d.footprint.width,d.footprint.height]==row['footprint']
 d.set_editor_property('presentation_mesh',unreal.load_asset(row['mesh_destination']));d.set_editor_property('logical_height_cm',row['logical_height_cm']);d.set_editor_property('entrance_sides',row['entrance_sides']);d.set_editor_property('presentation_yaw_offset_quarter_turns',2)
catalog.set_editor_property('definitions',definitions);save(catalog)
changed={r['definition_id'] for r in doc['assets']}
for d in catalog.get_editor_property('definitions'):
 if str(d.definition_id) not in changed:assert d.export_text()==before[str(d.definition_id)],str(d.definition_id)
report={'status':'passed','assets':records,'materials':materialRecords,'masters':{'solid':solidStats,'mapped':mappedStats},'unrelated_definitions_preserved':True,'source_manifest_sha256':sha(ROOT/'ArtSource/Manifests/city_technology_building_integration.json')}
(OUT/'NativeImport.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('CITYSIM_TECHNOLOGY_INTEGRATION_READY')
