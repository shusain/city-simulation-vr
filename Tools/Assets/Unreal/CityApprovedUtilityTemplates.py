"""Pure structural material extraction for the first native utility pass.

Three explicit stock graphs: Solid, PBR and PV. Source names, node names,
texture filenames/hashes and numeric values never select a parent. All source
graphs still pass the original validator before this narrower extraction.
"""
import hashlib,json,math
from Assets.Unreal.CityApprovedUtilityPlan import SCOPE,validate_graph
TEMPLATES={
 'Solid':dict(compile_time='Opaque constant base/roughness/metallic, no mapped normal',samplers={}),
 'PBR':dict(compile_time='Opaque color/normal/ORM; per-map UV scale, uniform tint/AO weights, tangent normal strength',samplers={'BaseColorTexture':'color','NormalTexture':'normal','ORMTexture':'orm'}),
 'PV':dict(compile_time='Opaque indexed UV cell grid: Frac/strict LessThan/Max/Lerp; no texture samplers',samplers={})}
CONCRETE_POLICY='First native pass explicitly OMITS source concrete 3D noise and derivative bump, matching nuclear solid policy. Foundation units still bind existing textured M_Concrete. Not source visual parity.'
def material_policy(plan):
 return dict(version=2,parent_policy='Three structural templates; all source values/textures assigned through instances',concrete_source_noise_bump='omitted_for_first_native_pass',foundation_concrete=plan['stock_concrete'],
  superseded_frozen_plan_note=plan['proposed_visual_limitations'][0],
  proposed_visual_limitations=[CONCRETE_POLICY]+plan['proposed_visual_limitations'][1:])
def parent_path(template):assert template in TEMPLATES;return SCOPE+'/Materials/M_Template_'+template
def structural_signature(template):return hashlib.sha256(json.dumps(TEMPLATES[template],sort_keys=True).encode()).hexdigest()

class Source:
 def __init__(self,graph,texture_records):
  validate_graph(graph)
  self.graph=graph;self.nodes={n['name']:n for n in graph['nodes']};self.links={(l['to_node'],l['to_index']):(l['from_node'],l['from_index']) for l in graph['links']}
  self.textures={t['sha256']:t for t in texture_records};self.params={};self.omitted=[]
  self.bsdf=next(n for n in self.nodes.values() if n['type']=='ShaderNodeBsdfPrincipled')
 def index(self,node,name):return next(i['index'] for i in node['inputs'] if i['name']==name)
 def ref(self,node,index):
  assert (node['name'],index) in self.links,('Expected linked source input',node['name'],index)
  return self.links[(node['name'],index)]
 def linked(self,node,index):return self.links.get((node['name'],index))
 def constant(self,node,index):
  assert (node['name'],index) not in self.links,('Expected uniform source constant',node['name'],index)
  return node['inputs'][index]['default']
 def typed(self,ref,kind,output=None):
  node=self.nodes[ref[0]];assert node['type']==kind,(node['name'],node['type'],kind)
  if output is not None:assert node['outputs'][ref[1]]==output,(node['name'],node['outputs'],output)
  return node
 def add(self,name,kind,value,origin='structural identity'):
  assert name not in self.params
  if kind=='scalar':value=float(value);assert math.isfinite(value)
  elif kind=='vector':
   value=[float(x) for x in value[:3]]+[1.];assert len(value)==4 and all(math.isfinite(x) for x in value)
  else:assert kind=='texture' and isinstance(value,dict)
  self.params[name]=dict(type=kind,value=value,source=origin)
 def scalar(self,name,node,index):
  value=self.constant(node,index);assert isinstance(value,(int,float)) and not isinstance(value,bool)
  self.add(name,'scalar',value,dict(node=node['name'],input_index=index,input=node['inputs'][index]['name']));return float(value)
 def vector(self,name,node,index):
  value=self.constant(node,index);assert isinstance(value,list) and len(value)>=3
  self.add(name,'vector',value,dict(node=node['name'],input_index=index,input=node['inputs'][index]['name']))
 def no_normal(self):
  ref=self.linked(self.bsdf,self.index(self.bsdf,'Normal'))
  if ref is None:return
  bump=self.typed(ref,'ShaderNodeBump','Normal')
  noise=self.typed(self.ref(bump,self.index(bump,'Height')),'ShaderNodeTexNoise')
  self.omitted.append(dict(reason=CONCRETE_POLICY,source_nodes=[noise['name'],bump['name']],
   source_noise_inputs=noise['inputs'],source_bump_inputs=bump['inputs']))
 def uvscale(self,image):
  ref=self.linked(image,0)
  if ref is None:return 1.,'implicit source render UV0'
  scale=self.typed(ref,'ShaderNodeVectorMath','Vector');assert scale['operation']=='SCALE'
  self.typed(self.ref(scale,0),'ShaderNodeTexCoord','UV')
  value=self.constant(scale,3);assert isinstance(value,(int,float)) and math.isfinite(value)
  return float(value),dict(node=scale['name'],input_index=3,input='Scale')
 def image(self,name,ref,kind):
  image=self.typed(ref,'ShaderNodeTexImage','Color');texture=self.textures.get(image['image_sha256'])
  assert texture is not None and texture['kind']==kind,('Sampler kind/hash mismatch',image['name'],kind)
  self.add(name+'Texture','texture',dict(native=texture['native'],sha256=texture['sha256'],source=texture['source'],sampler=kind),dict(node=image['name'],image_sha256=image['image_sha256']))
  scale,origin=self.uvscale(image);self.add(name+'UVScale','scalar',scale,origin)
  return image
 def solid(self):
  self.vector('BaseColor',self.bsdf,self.index(self.bsdf,'Base Color'))
  self.scalar('Roughness',self.bsdf,self.index(self.bsdf,'Roughness'));self.scalar('Metallic',self.bsdf,self.index(self.bsdf,'Metallic'));self.no_normal()
  return 'Solid'
 def pbr(self,base_ref):
  rough=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Roughness')),'ShaderNodeSeparateColor','Green')
  metal=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Metallic')),'ShaderNodeSeparateColor','Blue')
  assert rough['name']==metal['name'],'Separate ORM maps require an explicit new structural template'
  orm_ref=self.ref(rough,0);normal=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Normal')),'ShaderNodeNormalMap','Normal')
  base_node=self.nodes[base_ref[0]];ao=0.;tint=[1.,1.,1.,1.];tintweight=0.;ao_origin=tint_origin=weight_origin='structural identity'
  if base_node['type']=='ShaderNodeMixRGB':
   mix=self.typed(base_ref,'ShaderNodeMixRGB','Color');assert mix['blend_type']=='MULTIPLY' and not mix.get('use_clamp')
   factor=self.constant(mix,0);assert isinstance(factor,(int,float)) and 0<=factor<=1
   base_ref=self.ref(mix,1);other=self.linked(mix,2)
   if other is None:
    tint=self.constant(mix,2);assert isinstance(tint,list)
    tintweight=float(factor);tint_origin=dict(node=mix['name'],input_index=2);weight_origin=dict(node=mix['name'],input_index=0)
   else:
    red=self.typed(other,'ShaderNodeSeparateColor','Red');assert red['name']==rough['name']
    ao=float(factor);ao_origin=dict(node=mix['name'],input_index=0)
  self.image('BaseColor',base_ref,'color');self.image('Normal',self.ref(normal,self.index(normal,'Color')),'normal');self.image('ORM',orm_ref,'orm')
  self.scalar('NormalStrength',normal,self.index(normal,'Strength'))
  self.add('SourceTintColor','vector',tint,tint_origin);self.add('SourceTintWeight','scalar',tintweight,weight_origin);self.add('SourceAOWeight','scalar',ao,ao_origin)
  self.add('RoughnessScale','scalar',1.,'source ORM.G unchanged');self.add('MetallicScale','scalar',1.,'source ORM.B unchanged')
  return 'PBR'
 def pv(self,base_ref):
  mix=self.typed(base_ref,'ShaderNodeMixRGB','Color');assert mix['blend_type']=='MIX' and not mix.get('use_clamp')
  maxnode=self.typed(self.ref(mix,0),'ShaderNodeMath');assert maxnode['operation']=='MAXIMUM'
  axes={}
  for index in (0,1):
   less=self.typed(self.ref(maxnode,index),'ShaderNodeMath');assert less['operation']=='LESS_THAN'
   fract=self.typed(self.ref(less,0),'ShaderNodeMath');assert fract['operation']=='FRACT'
   multiply=self.typed(self.ref(fract,0),'ShaderNodeMath');assert multiply['operation']=='MULTIPLY'
   separate_ref=self.ref(multiply,0);separate=self.typed(separate_ref,'ShaderNodeSeparateXYZ')
   self.typed(self.ref(separate,0),'ShaderNodeTexCoord','UV');axis=separate['outputs'][separate_ref[1]];assert axis in ('X','Y') and axis not in axes
   axes[axis]=(multiply,less)
  assert set(axes)=={'X','Y'}
  for axis,frequency,width in [('X','PVColumns','PVLineWidthU'),('Y','PVRows','PVLineWidthV')]:
   mul,less=axes[axis];f=self.scalar(frequency,mul,1);w=self.scalar(width,less,1);assert f>0 and 0<w<1
  self.vector('PVInterior',mix,1);self.vector('PVGrid',mix,2)
  self.scalar('Roughness',self.bsdf,self.index(self.bsdf,'Roughness'));self.scalar('Metallic',self.bsdf,self.index(self.bsdf,'Metallic'));self.no_normal()
  return 'PV'
 def compile(self):
  ref=self.linked(self.bsdf,self.index(self.bsdf,'Base Color'))
  if ref is None:template=self.solid()
  elif self.nodes[ref[0]]['type']=='ShaderNodeMixRGB' and self.nodes[ref[0]].get('blend_type')=='MIX':template=self.pv(ref)
  else:template=self.pbr(ref)
  self.add('SurfaceTint','vector',[1,1,1]);self.add('DamageSolidColor','vector',[.065,.055,.045]);self.add('DamageSolidWeight','scalar',0.);self.add('DamageRoughnessWeight','scalar',0.)
  return dict(template=template,parent=parent_path(template),structural_signature=structural_signature(template),parameters=self.params,omitted_source_detail=self.omitted,
   source_graph_sha256=hashlib.sha256(json.dumps(self.graph,sort_keys=True).encode()).hexdigest())

def compile_graph(graph,textures):return Source(graph,textures).compile()
def instance_parameters(compiled,damaged=False):
 result={name:dict(type=p['type'],value=p['value']) for name,p in compiled['parameters'].items()}
 detailed=compiled['template'] in ('PBR','PV')
 if damaged:
  if detailed:result['SurfaceTint']['value']=[.16,.13,.11,1.]
  else:
   result['DamageSolidWeight']['value']=1.;result['DamageRoughnessWeight']['value']=1.
 return result
def evaluate_pv(parameters,source_uv):
 value=lambda name:parameters[name]['value']
 u,v=source_uv;x=u*value('PVColumns');y=v*value('PVRows')
 factor=float((x-math.floor(x))<value('PVLineWidthU') or (y-math.floor(y))<value('PVLineWidthV'))
 return [a*(1-factor)+b*factor for a,b in zip(value('PVInterior'),value('PVGrid'))]
