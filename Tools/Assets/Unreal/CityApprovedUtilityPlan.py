"""Pure-Python strict inputs and import plan, safe to run outside Unreal."""
from pathlib import Path
import hashlib,json,re,sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
SCOPE='/Game/CitySim/Art/Utilities/ApprovedBatch'
PLAN=ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json'
CONCRETE='/Game/CitySim/Art/HomeGroundPilot/M_Concrete'
PINNED_INPUTS={
 'ArtSource/Manifests/city_utility_integration.json':'ee3b1272d3266199480262c05e45c6d0b07173174521b407c56ba808cc0c77df',
 'ArtSource/Manifests/city_utility_preparation_verification.json':'3a1cf8dd833b172334fd58ff3ec7fe54fd78662b49c293e81713525e2852f88f',
 'ArtSource/Manifests/city_utility_ground_wind_preparation.json':'3431115155901dadf4e0dedda905f56685bd603e137415088bd1e2177bab3a9e',
 'ArtSource/Manifests/city_utility_ground_wind_verification.json':'d3d6eb4016b4a7a7590d4a764f5c6b773336d18d5d598ea436267f1059c1406e'}
NODE_TYPES={'ShaderNodeBsdfPrincipled','ShaderNodeOutputMaterial','ShaderNodeTexCoord','ShaderNodeVectorMath','ShaderNodeTexImage','ShaderNodeNormalMap','ShaderNodeSeparateColor','ShaderNodeSeparateXYZ','ShaderNodeMath','ShaderNodeMixRGB','ShaderNodeTexNoise','ShaderNodeBump'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_bytes())
def write(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
def checked_path(relative):
 p=(ROOT/relative).resolve();assert p.is_relative_to(ROOT) and p.is_file(),relative
 return p
def native_bounds(bounds):return dict(min=[bounds['min'][0]*100,-bounds['max'][1]*100,bounds['min'][2]*100],max=[bounds['max'][0]*100,-bounds['min'][1]*100,bounds['max'][2]*100])
def validate_graph(graph):
 nodes={n['name']:n for n in graph['nodes']};assert len(nodes)==len(graph['nodes'])
 assert all(n['type'] in NODE_TYPES for n in nodes.values()),graph['region']
 assert sum(n['type']=='ShaderNodeBsdfPrincipled' for n in nodes.values())==1
 for node in nodes.values():
  assert [i['index'] for i in node['inputs']]==list(range(len(node['inputs'])))
  if node['type']=='ShaderNodeMath':assert node['operation'] in ('MULTIPLY','FRACT','LESS_THAN','MAXIMUM')
  if node['type']=='ShaderNodeMixRGB':assert node['blend_type'] in ('MIX','MULTIPLY')
  if node['type']=='ShaderNodeVectorMath':assert node['operation']=='SCALE'
  if node['type']=='ShaderNodeNormalMap':assert node.get('space')=='TANGENT' and not node.get('uv_map')
  if node['type']=='ShaderNodeSeparateColor':assert node['outputs'][:3]==['Red','Green','Blue']
  if node['type']=='ShaderNodeTexNoise':
   values={s['name']:s['default'] for s in node['inputs']}
   assert node.get('noise_dimensions')=='3D' and node.get('noise_type','FBM')=='FBM'
   assert abs(values['Roughness']-.5)<1e-6 and abs(values['Lacunarity']-2)<1e-6 and values['Distortion']==0
   assert not any(l['to_node']==node['name'] and node['inputs'][l['to_index']]['name'] in ('Detail','Roughness','Lacunarity','Distortion') for l in graph['links'])
  if node['type']=='ShaderNodeBump':assert not any(l['to_node']==node['name'] and node['inputs'][l['to_index']]['name']=='Normal' for l in graph['links'])
  if node['type']=='ShaderNodeBsdfPrincipled':
   values={i['name']:i['default'] for i in node['inputs']}
   assert values['Alpha']==1 and values.get('Transmission Weight',0)==0 and values.get('Emission Strength',0)==0
 for link in graph['links']:
  a,b=nodes[link['from_node']],nodes[link['to_node']]
  assert 0<=link['from_index']<len(a['outputs']) and 0<=link['to_index']<len(b['inputs'])
 # Cycle detection covers even disconnected source nodes.
 visiting=set();done=set()
 def visit(name):
  assert name not in visiting,('Graph cycle',name)
  if name in done:return
  visiting.add(name)
  for link in graph['links']:
   if link['to_node']==name:visit(link['from_node'])
  visiting.remove(name);done.add(name)
 for name in nodes:visit(name)

def preflight():
 for path,pin in PINNED_INPUTS.items():assert sha(checked_path(path))==pin,('Pinned manifest differs',path)
 base=read(ROOT/'ArtSource/Manifests/city_utility_integration.json');extension=read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json')
 for path,pin in extension['protected_original_and_reference_files'].items():assert sha(checked_path(path))==pin,('Original/reference differs',path)
 for a in base['assets']:
  verified_export(ROOT/a['export_directory'],a['issue'],a['version'],a['approval_sha256'],kind='blender')
  for lod in a['lods']:assert sha(checked_path(lod['export']))==lod['export_sha256']
 for a in extension['foundation']['assets']:
  for group in a['groups']:assert sha(checked_path(group['export']))==group['export_sha256']
 for role in extension['wind']['roles']:assert sha(checked_path(role['export']))==role['export_sha256']
 for group in (extension['foundation'],extension['wind']):assert sha(checked_path(group['source']))==group['source_sha256']
 plan=read(PLAN);assert plan['input_head']=='33c7f223c59a8f7af7c9419df458e160a26e42f0' and plan['scope']==SCOPE
 assert plan['pinned_inputs']==PINNED_INPUTS and plan['stock_concrete']==CONCRETE
 assert len(plan['whole_meshes'])==11 and len(plan['wind_role_meshes'])==3 and len(plan['foundation_meshes'])==12
 for a in plan['assets']:
  original=next(x for x in base['assets'] if x['key']==a['key'])
  assert (a['issue'],a['version'],a['approval_sha256'],a['source_head'])==(original['issue'],original['version'],original['approval_sha256'],original['head'])
  assert [g['region'] for g in a['graphs']]==original['materials']['semantic_slot_order']
  for graph in a['graphs']:validate_graph(graph)
 for mesh in plan['whole_meshes']+plan['wind_role_meshes']+plan['foundation_meshes']:
  assert mesh['target'].startswith(SCOPE+'/') and '..' not in mesh['target']
  for lod in mesh['lods']:
   assert sha(checked_path(lod['export']))==lod['export_sha256']
   assert sum(lod['facts']['triangles_by_material'].values())==lod['triangles']
   assert lod['facts']['render_uv_channel']==0
 for record in plan['whole_meshes']:
  original=next(a for a in base['assets'] if a['key']==record['key'])
  assert [(l['export'],l['export_sha256']) for l in record['lods']]==[(l['export'],l['export_sha256']) for l in original['lods']]
 for record in plan['wind_role_meshes']:
  roles=sorted((r for r in extension['wind']['roles'] if r['role']==record['role']),key=lambda r:r['lod'])
  assert [(l['export'],l['export_sha256']) for l in record['lods']]==[(l['export'],l['export_sha256']) for l in roles]
  assert record['source_pivot_m']==roles[0]['source_pivot_m']
 for record in plan['foundation_meshes']:
  group=next(g for a in extension['foundation']['assets'] for g in a['groups'] if g['object']==record['key'])
  assert record['lods'][0]['export_sha256']==group['export_sha256'] and record['source_support_top_z_m']==group['source_support_top_z_m']
 for texture in plan['textures']:
  assert sha(checked_path(texture['source']))==texture['sha256']
  assert hashlib.md5(checked_path(texture['source']).read_bytes()).hexdigest()==texture['md5']
  assert any(t['sha256']==texture['sha256'] and t['candidate_native_texture']==texture['native'] and texture['source'] in t['reuse_source_paths'] for a in base['assets'] for t in a['materials']['textures'])
 return plan

def make_plan(inspection):
 base=read(ROOT/'ArtSource/Manifests/city_utility_integration.json');ext=read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json')
 assets=[];whole=[];textures={}
 for a in base['assets']:
  facts=next(x for x in inspection['assets'] if x['issue']==a['issue'])
  assets.append(dict(key=a['key'],issue=a['issue'],version=a['version'],approval_sha256=a['approval_sha256'],source_head=a['head'],graphs=facts['graphs'],source_bounds_m=a['lods'][0]['bounds_m']))
  for t in a['materials']['textures']:
   source=t['reuse_source_paths'][0];data=checked_path(source).read_bytes();record=dict(source=source,sha256=t['sha256'],md5=hashlib.md5(data).hexdigest(),native=t['candidate_native_texture'],colorspace=t['colorspace'],kind='normal' if '_Normal' in source else 'orm' if '_ORM' in source else 'color')
   assert record['native'] not in textures or textures[record['native']]==record;textures[record['native']]=record
  whole.append(dict(key=a['key'],kind='whole',issue=a['issue'],version=a['version'],target=SCOPE+'/Meshes/SM_'+a['key']+'_v'+str(a['version']),
   material_asset_key=a['key'],material_slots=a['materials']['semantic_slot_order'],source_pivot_m=[0,0,0],
   lods=[dict(lod=l['lod'],export=l['export'],export_sha256=l['export_sha256'],triangles=l['triangles'],facts=facts['lod_facts'][l['lod']]) for l in a['lods']],damaged_target=SCOPE+'/Meshes/SM_'+a['key']+'_v'+str(a['version'])+'_Damaged'))
 wind=[]
 for role in ('Stationary','YawNacelle','SpinRotor'):
  selected=sorted([r for r in ext['wind']['roles'] if r['role']==role],key=lambda r:r['lod'])
  wind.append(dict(key='wind_'+role,kind='wind_role',role=role,issue=168,version=1,target=SCOPE+'/Wind/SM_Wind_'+role+'_v1',
   material_asset_key='wind_turbine',material_slots=whole[next(i for i,a in enumerate(whole) if a['key']=='wind_turbine')]['material_slots'],source_pivot_m=selected[0]['source_pivot_m'],
   lods=[dict(lod=r['lod'],export=r['export'],export_sha256=r['export_sha256'],triangles=r['triangles'],facts=next(x for x in inspection['wind_roles'] if x['object']==r['object'])) for r in selected]))
 foundations=[]
 for a in ext['foundation']['assets']:
  for g in a['groups']:
   facts=next(x for x in inspection['foundations'] if x['object']==g['object'])
   foundations.append(dict(key=g['object'],kind='foundation',issue=a['issue'],version=a['version'],target=SCOPE+'/Foundations/'+g['object'],
    material_asset_key=None,material_slots=['Concrete'],source_pivot_m=[0,0,0],source_support_top_z_m=g['source_support_top_z_m'],
    lods=[dict(lod=0,export=g['export'],export_sha256=g['export_sha256'],triangles=g['triangles'],facts=facts)]))
 return dict(scope=SCOPE,input_head='33c7f223c59a8f7af7c9419df458e160a26e42f0',pinned_inputs=PINNED_INPUTS,assets=assets,textures=sorted(textures.values(),key=lambda t:t['native']),whole_meshes=whole,wind_role_meshes=wind,foundation_meshes=foundations,
  stock_concrete=CONCRETE,ci_wrappers=['Tools/Import-CityApprovedUtilityMaterials.py','Tools/Import-CityApprovedUtilityMeshes.py'],
  proposed_visual_limitations=[
   'Stock UE3D gradient noise/derivative surface-gradient bump approximates Blender fBM/bump. Scale/detail/distance/strength and generated bounds are retained; noise kernels/filtering are not pixel-equivalent. Native review required, especially nonuniform scale/mobile derivatives.',
   'Opaque UE default-lit replaces Blender Principled BRDF; source base/roughness/metallic/normal/tint/PV math links are translated, but specular lobe/lighting differences remain for native review.',
   'Texture tangent normal strength is reconstructed with decoded XY weighting and normalization; shared texture flip-green=False follows existing project policy and still needs native light-direction review.',
   'ORM.R receives no extra ambient-darkening where source leaves it disconnected; existing source AO tint links are retained.',
   'Initial screen sizes [1,.12,.025] mirror nuclear only as a proposal; no Android minimum/performance or headset acceptance.',
   'Damage retains exact body geometry/LODs: detailed base/PV uses SurfaceTint(.16,.13,.11); solid surfaces use char(.065,.055,.045),roughness.95,matching nuclear policy. No fracture/collapse derivative.'
  ],offline_only_recipe=True,native_execution_performed=False)

if __name__=='__main__':
 if '--build-plan' in sys.argv:
  inspection=read(ROOT/'Artifacts/UtilitySourcePreparation/NativeRecipeInspection.json');write(PLAN,make_plan(inspection))
 plan=preflight();print('UTILITY_NATIVE_PLAN_PREFLIGHT_PASS',len(plan['whole_meshes']),'whole',len(plan['wind_role_meshes']),'wind',len(plan['foundation_meshes']),'foundations; no Unreal execution')
