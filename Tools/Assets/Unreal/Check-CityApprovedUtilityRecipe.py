"""Offline checks: frozen inputs, actual engine source API references and PV math.

Does not import unreal, launch Editor, compile shaders, create native assets,
change a CI manifest or schedule a job. Its receipt explicitly says so.
"""
from pathlib import Path
from collections import Counter
import ast,copy,hashlib,importlib.util,json,re,sys,math,subprocess
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Unreal.CityApprovedUtilityPlan import preflight,validate_graph,ROOT,PLAN,sha,write
from Assets.Unreal.CityApprovedUtilityTemplates import TEMPLATES,compile_graph,instance_parameters,evaluate_pv,material_policy
ENGINE=Path('C:/Program Files/Epic Games/UE_5.7/Engine')

def evaluate(graph,uv):
 nodes={n['name']:n for n in graph['nodes']};links={(l['to_node'],l['to_index']):(l['from_node'],l['from_index']) for l in graph['links']}
 def socket(n,i):return output(*links[(n['name'],i)]) if (n['name'],i) in links else n['inputs'][i]['default']
 def output(name,index):
  n=nodes[name];kind=n['type']
  if kind=='ShaderNodeTexCoord':assert n['outputs'][index]=='UV';return uv
  if kind=='ShaderNodeSeparateXYZ':return socket(n,0)[index]
  if kind=='ShaderNodeMath':
   a=socket(n,0);op=n['operation']
   if op=='FRACT':return a-math.floor(a)
   b=socket(n,1)
   return a*b if op=='MULTIPLY' else max(a,b) if op=='MAXIMUM' else float(a<b)
  if kind=='ShaderNodeMixRGB':
   f,a,b=socket(n,0),socket(n,1),socket(n,2);return [x*(1-f)+y*f for x,y in zip(a,b)]
  raise AssertionError(kind)
 bsdf=next(n for n in nodes.values() if n['type']=='ShaderNodeBsdfPrincipled')
 return socket(bsdf,next(x['index'] for x in bsdf['inputs'] if x['name']=='Base Color'))

def source_properties(graph,uv,textures):
 """Independent arithmetic reference over source links, using synthetic texels.

No fake Unreal execution: this checks parameter/UV/channel/tint mappings only.
 """
 nodes={n['name']:n for n in graph['nodes']};links={(l['to_node'],l['to_index']):(l['from_node'],l['from_index']) for l in graph['links']}
 def inp(n,index):return out(*links[(n['name'],index)]) if (n['name'],index) in links else n['inputs'][index]['default']
 def out(name,index):
  n=nodes[name];kind=n['type']
  if kind=='ShaderNodeTexCoord':return list(uv)
  if kind=='ShaderNodeVectorMath':return [x*inp(n,3) for x in inp(n,0)]
  if kind=='ShaderNodeTexImage':
   coords=inp(n,0) if (name,0) in links else uv
   return texel(n['image_sha256'],coords)
  if kind=='ShaderNodeSeparateColor':return inp(n,0)[index]
  if kind=='ShaderNodeMixRGB':
   factor,a,b=inp(n,0),inp(n,1),inp(n,2)
   if isinstance(b,(float,int)):b=[b]*len(a)
   return [x*(1-factor)+(x*y if n['blend_type']=='MULTIPLY' else y)*factor for x,y in zip(a,b)]
  raise AssertionError(kind)
 bsdf=next(n for n in graph['nodes'] if n['type']=='ShaderNodeBsdfPrincipled')
 return {name:inp(bsdf,next(s['index'] for s in bsdf['inputs'] if s['name']==name)) for name in ('Base Color','Roughness','Metallic')}

def texel(sha,uv):
 seed=int(sha[:4],16)/65535
 return [(uv[0]*.31+seed)%1,(uv[1]*.23+seed*.7)%1,.2+seed*.5,1.]
def template_properties(compiled,uv):
 p=compiled['parameters'];v=lambda name:p[name]['value'];template=compiled['template']
 if template=='Solid':return {'Base Color':v('BaseColor'),'Roughness':v('Roughness'),'Metallic':v('Metallic')}
 if template=='PV':return {'Base Color':evaluate_pv(instance_parameters(compiled),uv),'Roughness':v('Roughness'),'Metallic':v('Metallic')}
 sample=lambda prefix:texel(v(prefix+'Texture')['sha256'],[x*v(prefix+'UVScale') for x in uv])
 base,orm=sample('BaseColor'),sample('ORM');tint=v('SourceTintColor');factor=v('SourceTintWeight');ao=v('SourceAOWeight')
 return {'Base Color':[(x*(1-factor)+x*t*factor)*(1-ao+orm[0]*ao) for x,t in zip(base,tint)],'Roughness':orm[1]*v('RoughnessScale'),'Metallic':orm[2]*v('MetallicScale')}

def shared_template_checks(plan):
 mappings=[];schemas={};legacy=set();negative=[];stability=[]
 for asset in plan['assets']:
  for graph in asset['graphs']:
   compiled=compile_graph(graph,plan['textures']);template=compiled['template'];params=compiled['parameters']
   schema={name:(p['type'],p['value'].get('sampler') if p['type']=='texture' else None) for name,p in params.items()}
   assert template not in schemas or schema==schemas[template];schemas[template]=schema
   # Every source-linked parameter traces back to its original indexed socket or
   # source image hash; identity parameters are explicitly marked as such.
   for name,p in params.items():
    origin=p['source']
    if not isinstance(origin,dict):continue
    node=next(n for n in graph['nodes'] if n['name']==origin['node'])
    if 'input_index' in origin:
     expected=node['inputs'][origin['input_index']]['default'];actual=p['value']
     if p['type']=='vector':expected=expected[:3]+[1.]
     assert actual==expected,(asset['key'],graph['region'],name,actual,expected)
    elif p['type']=='texture':assert node['image_sha256']==p['value']['sha256']
   healthy,damaged=instance_parameters(compiled),instance_parameters(compiled,True)
   unchanged=[name for name in params if name not in ('SurfaceTint','DamageSolidWeight','DamageRoughnessWeight')]
   assert all(healthy[name]==damaged[name] for name in unchanged)
   # Compare all current scalar/vector colour properties with an independent
   # source-link evaluation. PV has its own indexed source evaluator below.
   for uv in ((.031,.071),(.28,.39),(.7,.11)):
    expected=evaluate(graph,uv) if template=='PV' else source_properties(graph,uv,plan['textures'])
    actual=template_properties(compiled,uv)
    if template=='PV':expected={'Base Color':expected,'Roughness':params['Roughness']['value'],'Metallic':params['Metallic']['value']}
    for prop in expected:
     a,b=actual[prop],expected[prop]
     assert max(abs(x-y) for x,y in zip(a[:3],b[:3]))<1e-10 if isinstance(a,list) else abs(a-b)<1e-10,(asset['key'],graph['region'],prop,a,b)
   renamed=copy.deepcopy(graph);names={n['name']:'Renamed_'+str(i) for i,n in enumerate(renamed['nodes'])}
   for n in renamed['nodes']:n['name']=names[n['name']]
   for link in renamed['links']:link['from_node']=names[link['from_node']];link['to_node']=names[link['to_node']]
   renamed['region']='ArbitraryRegionAndAssetName';other=compile_graph(renamed,plan['textures'])
   assert other['parent']==compiled['parent'] and instance_parameters(other)==healthy
   payload=dict(nodes=graph['nodes'],links=graph['links'],uv_policy='fbx_v_flip_compensated_v1',generated_bounds=asset['source_bounds_m'] if any(n['type']=='ShaderNodeTexNoise' for n in graph['nodes']) else None)
   legacy.add(hashlib.sha256(json.dumps(payload,sort_keys=True).encode()).hexdigest()[:20])
   mappings.append(dict(key=asset['key'],issue=asset['issue'],region=graph['region'],compiled=compiled,healthy_defaults=healthy,damaged_defaults=damaged))
 assert len(legacy)==46 and set(schemas)==set(TEMPLATES) and len(mappings)==51
 omitted=[m for m in mappings if m['compiled']['omitted_source_detail']];assert len(omitted)==4 and all(m['compiled']['template']=='Solid' for m in omitted)
 # Value/path changes must change instances, never parent identity.
 solid=next(g for a in plan['assets'] for g in a['graphs'] if compile_graph(g,plan['textures'])['template']=='Solid');changed=copy.deepcopy(solid)
 bsdf=next(n for n in changed['nodes'] if n['type']=='ShaderNodeBsdfPrincipled');next(s for s in bsdf['inputs'] if s['name']=='Base Color')['default']=[.27,.47,.69,1.]
 assert compile_graph(changed,plan['textures'])['parent']==compile_graph(solid,plan['textures'])['parent'];stability.append('solid colour values')
 pbr=next(g for a in plan['assets'] for g in a['graphs'] if any(n['type']=='ShaderNodeVectorMath' for n in g['nodes']));changed=copy.deepcopy(pbr)
 next(n for n in changed['nodes'] if n['type']=='ShaderNodeVectorMath')['inputs'][3]['default']=.73
 assert compile_graph(changed,plan['textures'])['parent']==compile_graph(pbr,plan['textures'])['parent'];stability.append('UV scale values')
 changed=copy.deepcopy(pbr);image=next(n for n in changed['nodes'] if n['type']=='ShaderNodeTexImage' and any(t['sha256']==n['image_sha256'] and t['kind']=='color' for t in plan['textures']))
 image['image_sha256']=next(t['sha256'] for t in plan['textures'] if t['kind']=='color' and t['sha256']!=image['image_sha256'])
 assert compile_graph(changed,plan['textures'])['parent']==compile_graph(pbr,plan['textures'])['parent'];stability.append('texture identity/filepath')
 pv=next(g for a in plan['assets'] for g in a['graphs'] if compile_graph(g,plan['textures'])['template']=='PV');compiled=compile_graph(pv,plan['textures'])
 assert compiled['parameters']['PVColumns']['value']==12 and compiled['parameters']['PVRows']['value']==8
 for name in ('PVLineWidthU','PVLineWidthV'):assert abs(compiled['parameters'][name]['value']-.018)<1e-9
 probes=[(i/71,j/67) for i in range(1,40) for j in range(1,19)]+[(0.,0.),(.018/12-1e-7,.047),(.018/12+1e-7,.047)]
 for uv in probes:assert max(abs(a-b) for a,b in zip(evaluate(pv,uv),evaluate_pv(instance_parameters(compiled),uv)))<1e-10
 changed=copy.deepcopy(pv);next(n for n in changed['nodes'] if n['type']=='ShaderNodeMath' and n['operation']=='MULTIPLY')['inputs'][1]['default']=24.
 assert compile_graph(changed,plan['textures'])['parent']==compiled['parent'];stability.append('PV frequencies')
 # Invalid graph/sampler/grid cases fail extraction; no fallback flat material.
 bad=copy.deepcopy(pv);next(n for n in bad['nodes'] if n['type']=='ShaderNodeMath' and n['operation']=='MULTIPLY')['inputs'][1]['default']=0.
 cases=[('PV zero frequency',bad)]
 bad=copy.deepcopy(pv);next(n for n in bad['nodes'] if n['type']=='ShaderNodeMath' and n['operation']=='LESS_THAN')['inputs'][1]['default']=-.01;cases.append(('PV invalid width',bad))
 bad=copy.deepcopy(pbr);normalhashes={t['sha256'] for t in plan['textures'] if t['kind']=='normal'};image=next(n for n in bad['nodes'] if n['type']=='ShaderNodeTexImage' and n['image_sha256'] in normalhashes);image['image_sha256']=next(t['sha256'] for t in plan['textures'] if t['kind']=='color');cases.append(('normal sampler mismatch',bad))
 bad=copy.deepcopy(pbr);bsdf=next(n for n in bad['nodes'] if n['type']=='ShaderNodeBsdfPrincipled');ri=next(s['index'] for s in bsdf['inputs'] if s['name']=='Roughness');bad['links']=[l for l in bad['links'] if not(l['to_node']==bsdf['name'] and l['to_index']==ri)];cases.append(('PBR missing roughness map',bad))
 for label,bad in cases:
  try:compile_graph(bad,plan['textures'])
  except AssertionError:negative.append(label)
  else:raise AssertionError('Invalid source accepted: '+label)
 representatives={}
 for m in mappings:representatives.setdefault(m['compiled']['template'],dict(key=m['key'],region=m['region'],defaults=m['healthy_defaults']))
 return dict(previous_value_sensitive_parent_count=len(legacy),template_count=len(schemas),template_structures=TEMPLATES,template_parameter_schemas=schemas,representative_template_defaults=representatives,
  graph_mappings=mappings,graph_counts=dict(Counter(m['compiled']['template'] for m in mappings)),omitted_concrete_graphs=[(m['key'],m['region']) for m in omitted],
  all51_source_parameter_origins_verified=True,healthy_damaged_source_values_retained=True,name_stability_graphs=51,value_and_texture_stability_cases=stability,
  pv_source_reference_probes=len(probes),negative_cases_rejected=negative,material_policy=material_policy(plan),native_instance_readbacks='Implemented in native recipe; not executed offline')

def main():
 plan=preflight();files=[*Path(ROOT/'Tools/Assets/Unreal').glob('CityApprovedUtility*.py'),Path(__file__),ROOT/'Tools/Assets/Blender/Inspect-CityUtilityNativeRecipe.py',*[ROOT/p for p in plan['ci_wrappers']]]
 trees={p:ast.parse(p.read_text(encoding='utf-8')) for p in files}
 assert all(re.fullmatch(r'Tools/[A-Za-z0-9_-]+\.py',p) for p in plan['ci_wrappers'])
 ci=(ROOT/'Tools/CI/Invoke-CityCI.ps1').read_text();assert 'WaitForExit(900000)' in ci and "'^Tools/[A-Za-z0-9_-]+\\.py$'" in ci
 meshrecords=plan['whole_meshes']+plan['wind_role_meshes']+plan['foundation_meshes'];targets=[m['target'] for m in meshrecords]+[m['damaged_target'] for m in plan['whole_meshes']]
 assert len(targets)==len(set(targets))==37 and all(p.startswith(plan['scope']+'/') for p in targets)
 assert sum(len(m['lods']) for m in meshrecords)==54
 graphs=[g for a in plan['assets'] for g in a['graphs']];assert len(graphs)==51 and len(plan['textures'])==6
 # Negative checks exercise the validator without touching any source bytes.
 wrong=copy.deepcopy(graphs[0]);wrong['nodes'][0]['type']='UnapprovedCustomShader'
 try:validate_graph(wrong)
 except AssertionError:pass
 else:raise AssertionError('Unsupported source graph accepted')
 wrong=copy.deepcopy(graphs[0]);wrong['links'].append(dict(from_node=wrong['nodes'][0]['name'],from_index=0,to_node=wrong['nodes'][0]['name'],to_index=0))
 try:validate_graph(wrong)
 except AssertionError:pass
 else:raise AssertionError('Graph cycle accepted')
 solar=next(a for a in plan['assets'] if a['key']=='solar_farm');pv=next(g for g in solar['graphs'] if 'PV' in g['region'])
 mathnodes=[n for n in pv['nodes'] if n['type']=='ShaderNodeMath' and n['operation']=='MULTIPLY']
 assert sorted(n['inputs'][1]['default'] for n in mathnodes)==[8.,12.]
 # Independently check source interior/border colours and exact grid periods.
 base=evaluate(pv,(.031,.071));border=evaluate(pv,(0.,.071));assert base!=border
 for uv in [(.031,.071),(.051,.081),(.001,.001),(.1,.5)]:
  value=evaluate(pv,uv)
  assert max(abs(a-b) for a,b in zip(value,evaluate(pv,(uv[0]+1/12,uv[1]))))<1e-5
  assert max(abs(a-b) for a,b in zip(value,evaluate(pv,(uv[0],uv[1]+1/8))))<1e-5
 audited={}
 references={
  'Source/Editor/UnrealEd/Private/MaterialGraphNode.cpp':['TEXT("UVs")','TEXT("A > B")','TEXT("A == B")','TEXT("A < B")'],
  'Source/Editor/UnrealEd/Private/Fbx/FbxStaticMeshImport.cpp':['1.0f - static_cast<float>(UVVector[1])'],
  'Source/Editor/StaticMeshEditor/Public/StaticMeshEditorSubsystem.h':['GetLodBuildSettings','SetLodBuildSettings','ImportLOD','GetLODMaterialSlot','GetNumUVChannels','SetLodScreenSizes'],
  'Plugins/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h':['GetSectionFromStaticMesh','ScriptName="ProceduralMeshLibrary"'],
  'Source/Runtime/Engine/Private/EditorFramework/AssetImportData.cpp':['FileMD5','SourceData.ToJson()'],
  'Source/Runtime/Engine/Private/Texture.cpp':['SourceFileTagName()', 'AssetImportData->GetSourceData().ToJson()'],
  'Source/Runtime/CoreUObject/Private/UObject/Obj.cpp':['const FName& UObject::SourceFileTagName()', 'static const FName SourceFilePathName("AssetImportData")'],
  'Source/Editor/MaterialEditor/Public/MaterialEditingLibrary.h':['SetMaterialInstanceTextureParameterValue','GetMaterialInstanceTextureParameterValue','GetMaterialInstanceScalarParameterValue','GetMaterialInstanceVectorParameterValue']}
 for path,symbols in references.items():
  text=(ENGINE/path).read_text(encoding='utf-8-sig');assert all(symbol in text for symbol in symbols),(path,[s for s in symbols if s not in text])
  audited[path]=sha(ENGINE/path)
 kinds=set()
 for tree in trees.values():
  for node in ast.walk(tree):
   if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and node.func.attr in ('node','binary','unary') and node.args and isinstance(node.args[0],ast.Constant) and isinstance(node.args[0].value,str):kinds.add(node.args[0].value)
   if isinstance(node,ast.Attribute) and node.attr.startswith('MaterialExpression'):kinds.add(node.attr.removeprefix('MaterialExpression'))
 for kind in kinds:
  path=ENGINE/'Source/Runtime/Engine/Public/Materials'/('MaterialExpression'+kind+'.h');assert path.is_file(),kind;audited[str(path.relative_to(ENGINE)).replace('\\','/')]=sha(path)
 assert not kinds.intersection({'Noise','DDX','DDY','Transform','LocalPosition','VertexNormalWS','CrossProduct','DotProduct'}),'No procedural concrete implementation in first pass'
 shared=shared_template_checks(plan)
 # Preserve original source/graph/probe validators rather than editing evidence
 # to fit a material-policy revision. Compare function ASTs with reviewed45c.
 for filename,names in [('CityApprovedUtilityPlan.py',('preflight','validate_graph')),('CityApprovedUtilityMeshes.py',('check_mesh','import_mesh'))]:
  path='Tools/Assets/Unreal/'+filename;before=subprocess.run(['git','show','45c19563e88dfa9e12139b2215968660aa2abd63:'+path],cwd=ROOT,capture_output=True,check=True).stdout.decode()
  original=ast.parse(before);current=ast.parse((ROOT/path).read_text())
  for name in names:assert ast.dump(next(n for n in original.body if isinstance(n,ast.FunctionDef) and n.name==name))==ast.dump(next(n for n in current.body if isinstance(n,ast.FunctionDef) and n.name==name)),('Validator changed',name)
 uproject=json.loads((ROOT/'CitySimVR/CitySimVR.uproject').read_text());assert any(p['Name']=='ProceduralMeshComponent' and p.get('Enabled') for p in uproject['Plugins'])
 base=read_json(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json')
 assert all(sha(ROOT/p)==pin for p,pin in base['protected_original_and_reference_files'].items())
 receipt=dict(task=157,input_head=plan['input_head'],plan_sha256=sha(PLAN),python_files_parsed=[p.relative_to(ROOT).as_posix() for p in files],graph_count=51,whole_healthy_and_damaged_assets=22,wind_role_assets=3,foundation_assets=12,planned_lod_imports=54,
  ci_wrapper_paths_match_current_rule=True,current_per_script_cap_seconds=900,actual_phase_seconds=None,shared_texture_hashes_verified=6,indexed_pv12x8_math_checked=True,invalid_graph_and_cycle_rejected=True,
  installed_engine_api_source_hashes=audited,stock_expression_headers_verified=sorted(kinds),all_preparation_inputs_unchanged=True,
  actual_unreal_import_shader_compile_or_asset_execution=False,shared_template_report=shared,source_and_probe_validators_unchanged_vs45c=True,proposed_visual_limitations=material_policy(plan)['proposed_visual_limitations'],status='offline shared-template/source/API/plan checks pass; NOT a native execution pass')
 output=ROOT/'Artifacts/UtilitySourcePreparation/SharedTemplates-OfflineVerification.json';output.parent.mkdir(parents=True,exist_ok=True);write(output,receipt)
 print('UTILITY_SHARED_TEMPLATES_OFFLINE_PASS 3 templates/51 graphs,37 mesh assets,54LOD inputs; frozen proofs unchanged; NO Unreal execution')
def read_json(path):return json.loads(Path(path).read_bytes())
if __name__=='__main__':main()
