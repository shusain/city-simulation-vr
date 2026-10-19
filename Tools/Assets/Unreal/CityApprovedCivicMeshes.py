# Civic-scoped import; utility recipe remains frozen.
import math
from collections import defaultdict
import unreal
from Assets.Unreal.CityApprovedCivicPlan import ROOT
from Assets.Unreal.CityApprovedUtilityNative import object_path

def values(vector):return [vector.x,vector.y,vector.z]
def norm(vector):
 p=values(vector);length=math.sqrt(sum(x*x for x in p));assert length>0
 return [x/length for x in p]
def check_mesh(mesh,record,bindings,subsystem):
 slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
 assert len(slots)==len(record['material_slots']) and set(slots)==set(record['material_slots']),('Material slots differ',object_path(mesh),slots)
 assert subsystem.get_lod_count(mesh)==len(record['lods'])
 assert all(object_path(mesh.get_material(i))==bindings[slot] for i,slot in enumerate(slots))
 results=[]
 for lod in record['lods']:
  index=lod['lod'];facts=lod['facts'];expected=facts['triangles_by_material'];actual={};all_vertices=[];surfaces={};sections=[]
  assert mesh.get_num_triangles(index)==lod['triangles']
  # Empty imported slots may be retained by FBX; nonempty sections are exact.
  count=mesh.get_num_sections(index);assert len(expected)<=count<=len(slots)
  assert subsystem.get_num_uv_channels(mesh,index)==facts['uv_channels']
  for section in range(count):
   materialindex=subsystem.get_lod_material_slot(mesh,index,section);assert 0<=materialindex<len(slots)
   name=slots[materialindex]
   vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,index,section)
   assert len(triangles)%3==0 and len(vertices)==len(normals)==len(uvs)==len(tangents)
   if not triangles:continue
   assert name not in actual and name in expected
   actual[name]=len(triangles)//3;assert actual[name]==expected[name]
   assert all(0<=i<len(vertices) for i in triangles)
   assert all(math.isfinite(x) for p in vertices for x in values(p))
   assert all(math.isfinite(x) for uv in uvs for x in (uv.x,uv.y))
   assert all(math.isfinite(x) for n in normals for x in values(n))
   all_vertices.extend(vertices);spatial=defaultdict(list)
   for vertex,normal,uv in zip(vertices,normals,uvs):
    point=values(vertex);normalvalues=values(normal);length=math.sqrt(sum(x*x for x in normalvalues))
    spatial[tuple(math.floor(x/.25) for x in point)].append((point,[x/length for x in normalvalues] if length>1e-6 else [0,0,0],(uv.x,uv.y)))
   surfaces[name]=spatial
   sections.append(dict(section=section,material_slot=name,triangles=len(triangles)//3,vertices=len(vertices)))
  assert actual==expected and len(sections)==len(expected)
  source=facts['bounds_m'];predicted=dict(min=[source['min'][0]*100,-source['max'][1]*100,source['min'][2]*100],max=[source['max'][0]*100,-source['min'][1]*100,source['max'][2]*100])
  measured=dict(min=[min(values(v)[k] for v in all_vertices) for k in range(3)],max=[max(values(v)[k] for v in all_vertices) for k in range(3)])
  error=max(abs(x-y) for side in ('min','max') for x,y in zip(predicted[side],measured[side]));assert error<.2,('Signed bounds/pivot/basis differ',record['target'],index,predicted,measured)
  sample_results=[]
  for region,samples in facts['samples'].items():
   spatial=surfaces[region]
   for sample in samples:
    sourcepoint=sample['position_m'];target=[sourcepoint[0]*100,-sourcepoint[1]*100,sourcepoint[2]*100]
    expecteduv=[sample['uv0'][0],1.-sample['uv0'][1]]
    expectednormal=[sample['normal'][0],-sample['normal'][1],sample['normal'][2]];source_length=math.sqrt(sum(x*x for x in expectednormal))
    options=[]
    cell=[math.floor(x/.25) for x in target]
    candidates=[candidate for dx in (-1,0,1) for dy in (-1,0,1) for dz in (-1,0,1) for candidate in spatial.get((cell[0]+dx,cell[1]+dy,cell[2]+dz),())]
    for vertex,normal,uv in candidates:
     distance=math.sqrt(sum((x-y)**2 for x,y in zip(vertex,target)))
     if distance>.2:continue
     uverror=max(abs(uv[0]-expecteduv[0]),abs(uv[1]-expecteduv[1]))
     if uverror>.0002:continue
     nlength=math.sqrt(sum(x*x for x in normal))
     dot=sum(x*y for x,y in zip(normal,expectednormal))/source_length if source_length>1e-6 and nlength>1e-6 else 1. if source_length<=1e-6 else -1.
     if dot>.99999:options.append((distance,uverror,dot))
    assert options,('Native surface probe differs',record['target'],index,region,sample)
    distance,uv_error,dot=min(options,key=lambda x:(x[0],x[1],-x[2]));sample_results.append(dict(region=region,position_error_cm=distance,uv_error=uv_error,normal_dot=dot,source_normal_defined=source_length>1e-6))
  build=subsystem.get_lod_build_settings(mesh,index)
  assert not build.get_editor_property('recompute_normals') and build.get_editor_property('use_full_precision_u_vs')
  assert not build.get_editor_property('generate_lightmap_u_vs')
  results.append(dict(lod=index,triangles=lod['triangles'],sections=sections,total_sections_including_empty=count,material_triangles=actual,bounds_native_cm=measured,signed_bounds_error_cm=error,uv_channels=facts['uv_channels'],surface_probes=sample_results))
 return dict(native=object_path(mesh),source_pivot_m=record['source_pivot_m'],native_pivot_policy='Unrecentered source-local origin; signed source-to-native bounds and actual surface probes verified for each LOD',slots=slots,lods=results)

def import_mesh(record,bindings,subsystem,*,scope_path,save_asset):
 path=scope_path(record['target']);folder,name=path.rsplit('/',1)
 task=unreal.AssetImportTask()
 for k,v in dict(filename=str(ROOT/record['lods'][0]['export']),destination_path=folder,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
 task.factory=unreal.FbxFactory();options=unreal.FbxImportUI()
 for k,v in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False,reset_to_fbx_on_material_conflict=True).items():options.set_editor_property(k,v)
 for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene=True,convert_scene_unit=True,import_uniform_scale=1.,import_translation=unreal.Vector(0,0,0),import_rotation=unreal.Rotator(0,0,0),transform_vertex_to_absolute=True,bake_pivot_in_vertex=False,generate_lightmap_u_vs=False,build_nanite=False,auto_generate_collision=False,remove_degenerates=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
 task.options=options;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(path);assert isinstance(mesh,unreal.StaticMesh)
 imported=mesh.get_editor_property('asset_import_data');assert isinstance(imported,unreal.FbxStaticMeshImportData)
 for flag in ('convert_scene','convert_scene_unit','transform_vertex_to_absolute'):assert imported.get_editor_property(flag)
 assert not imported.get_editor_property('bake_pivot_in_vertex') and abs(imported.get_editor_property('import_uniform_scale')-1.)<1e-6
 assert all(abs(v)<1e-6 for v in values(imported.get_editor_property('import_translation')))
 rotation=imported.get_editor_property('import_rotation');assert all(abs(v)<1e-6 for v in (rotation.pitch,rotation.yaw,rotation.roll))
 assert imported.get_editor_property('normal_import_method')==unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
 for lod in record['lods'][1:]:assert subsystem.import_lod(mesh,lod['lod'],str(ROOT/lod['export']))==lod['lod']
 for lod in record['lods']:
  settings=subsystem.get_lod_build_settings(mesh,lod['lod'])
  for k,v in dict(recompute_normals=False,recompute_tangents=True,use_full_precision_u_vs=True,use_high_precision_tangent_basis=True,generate_lightmap_u_vs=False,remove_degenerates=False,build_scale3d=unreal.Vector(1,1,1)).items():settings.set_editor_property(k,v)
  subsystem.set_lod_build_settings(mesh,lod['lod'],settings)
 slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
 assert set(slots)==set(bindings) and len(slots)==len(bindings)
 for i,slot in enumerate(slots):
  material=unreal.load_asset(bindings[slot]);assert isinstance(material,unreal.MaterialInterface);mesh.set_material(i,material)
 if len(record['lods'])==3:assert subsystem.set_lod_screen_sizes(mesh,[1.,.12,.025])
 result=check_mesh(mesh,record,bindings,subsystem);save_asset(mesh);return mesh,result
