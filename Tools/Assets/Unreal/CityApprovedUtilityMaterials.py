"""Three shared stock masters; source concrete noise/bump explicitly omitted.

Frozen source graphs remain fully validated. Source values/textures are instance
parameters; names, paths and numeric values never select a shader parent.
"""
import re
import unreal
from Assets.Unreal.CityApprovedUtilityPlan import SCOPE,preflight,sha,PLAN,write
from Assets.Unreal.CityApprovedUtilityTemplates import TEMPLATES,compile_graph,instance_parameters,parent_path,material_policy
from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation
from Assets.Unreal.CityApprovedUtilityRender import apply_native_render_expectations
from Assets.Unreal.CityApprovedUtilityNative import LIB,OUT,create,save,emit_files,parameter,finish_material,textures_and_guard,unchanged,recipe_hashes,object_path
def token(text):return re.sub('[^A-Za-z0-9_]','_',text)
def binding(asset,graph,damaged=False):return SCOPE+'/Materials/MI_'+token(asset['key'])+'_'+token(graph['region'])+('_Damaged' if damaged else '')

class TemplateGraph:
 def __init__(self,material,compiled,textures):self.material=material;self.compiled=compiled;self.textures=textures;self.parameters={};self.uv=None
 def node(self,kind,**properties):
  obj=LIB.create_material_expression(self.material,getattr(unreal,'MaterialExpression'+kind))
  for name,value in properties.items():obj.set_editor_property(name,value)
  return obj,''
 def connect(self,value,target,pin=''):
  pin={'AGreaterThanB':'A > B','AEqualsB':'A == B','ALessThanB':'A < B'}.get(pin,pin)
  assert LIB.connect_material_expressions(value[0],value[1],target[0],pin),(value[0].get_name(),target[0].get_name(),pin)
 def scalar(self,value):return self.node('Constant',r=float(value))
 def binary(self,kind,a,b):
  n=self.node(kind);self.connect(a,n,'A');self.connect(b,n,'B');return n
 def unary(self,kind,value):
  n=self.node(kind);self.connect(value,n);return n
 def mask(self,value,channel):
  n=self.node('ComponentMask',r=channel==0,g=channel==1,b=channel==2,a=channel==3);self.connect(value,n);return n
 def append(self,a,b):return self.binary('AppendVector',a,b)
 def blend(self,a,b,factor):
  n=self.node('LinearInterpolate');self.connect(a,n,'A');self.connect(b,n,'B');self.connect(factor,n,'Alpha');return n
 def param(self,name):
  if name in self.parameters:return self.parameters[name]
  p=self.compiled['parameters'][name];kind,value=p['type'],p['value']
  if kind=='scalar':obj=parameter(self.material,unreal.MaterialExpressionScalarParameter,name,value)
  elif kind=='vector':obj=parameter(self.material,unreal.MaterialExpressionVectorParameter,name,unreal.LinearColor(*value))
  else:
   assert kind=='texture';obj=LIB.create_material_expression(self.material,unreal.MaterialExpressionTextureSampleParameter2D)
   obj.set_editor_property('parameter_name',name);obj.set_editor_property('texture',self.textures[value['sha256']])
   obj.set_editor_property('sampler_type',{'color':unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,'normal':unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,'orm':unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[value['sampler']])
  self.parameters[name]=obj,'';return self.parameters[name]
 def source_uv(self):
  if self.uv is None:
   native=self.node('TextureCoordinate',coordinate_index=0);self.uv=self.append(self.mask(native,0),self.unary('OneMinus',self.mask(native,1)))
  return self.uv
 def sample(self,prefix):
  sample=self.param(prefix+'Texture');source=self.binary('Multiply',self.source_uv(),self.param(prefix+'UVScale'))
  native=self.append(self.mask(source,0),self.unary('OneMinus',self.mask(source,1)));self.connect(native,sample,'UVs');return sample
 def less(self,a,b):
  result=self.node('If',equals_threshold=0.)
  for pin,value in [('A',a),('B',b),('AGreaterThanB',self.scalar(0)),('AEqualsB',self.scalar(0)),('ALessThanB',self.scalar(1))]:self.connect(value,result,pin)
  return result
 def build(self):
  template=self.compiled['template'];normal=None
  if template=='Solid':base=self.param('BaseColor');rough=self.param('Roughness');metal=self.param('Metallic')
  elif template=='PBR':
   base=self.sample('BaseColor');orm=self.sample('ORM');decoded=self.sample('Normal')
   base=self.blend(base,self.binary('Multiply',base,self.param('SourceTintColor')),self.param('SourceTintWeight'))
   base=self.blend(base,self.binary('Multiply',base,self.mask(orm,0)),self.param('SourceAOWeight'))
   rough=self.binary('Multiply',self.mask(orm,1),self.param('RoughnessScale'));metal=self.binary('Multiply',self.mask(orm,2),self.param('MetallicScale'))
   xy=self.binary('Multiply',self.append(self.mask(decoded,0),self.mask(decoded,1)),self.param('NormalStrength'));normal=self.unary('Normalize',self.append(xy,self.mask(decoded,2)))
  else:
   assert template=='PV';uv=self.source_uv()
   u=self.unary('Frac',self.binary('Multiply',self.mask(uv,0),self.param('PVColumns')));v=self.unary('Frac',self.binary('Multiply',self.mask(uv,1),self.param('PVRows')))
   grid=self.binary('Max',self.less(u,self.param('PVLineWidthU')),self.less(v,self.param('PVLineWidthV')))
   base=self.blend(self.param('PVInterior'),self.param('PVGrid'),grid);rough=self.param('Roughness');metal=self.param('Metallic')
  base=self.blend(self.binary('Multiply',base,self.param('SurfaceTint')),self.param('DamageSolidColor'),self.param('DamageSolidWeight'))
  rough=self.blend(rough,self.scalar(.95),self.param('DamageRoughnessWeight'))
  for value,prop in [(base,unreal.MaterialProperty.MP_BASE_COLOR),(rough,unreal.MaterialProperty.MP_ROUGHNESS),(metal,unreal.MaterialProperty.MP_METALLIC)]:assert LIB.connect_material_property(value[0],value[1],prop)
  if normal:assert LIB.connect_material_property(normal[0],normal[1],unreal.MaterialProperty.MP_NORMAL)
  assert set(self.parameters)==set(self.compiled['parameters']),'Every declared parameter must participate in the shared graph'

def assign_and_verify(instance,parameters,textures):
 observed={}
 for name,p in parameters.items():
  kind,value=p['type'],p['value']
  if kind=='scalar':
   LIB.set_material_instance_scalar_parameter_value(instance,name,value);actual=LIB.get_material_instance_scalar_parameter_value(instance,name)
   assert abs(actual-value)<1e-6,(name,actual,value)
  elif kind=='vector':
   LIB.set_material_instance_vector_parameter_value(instance,name,unreal.LinearColor(*value));color=LIB.get_material_instance_vector_parameter_value(instance,name);actual=[color.r,color.g,color.b,color.a]
   assert max(abs(a-b) for a,b in zip(actual,value))<1e-6,(name,actual,value)
  else:
   assert kind=='texture';expected=textures[value['sha256']];LIB.set_material_instance_texture_parameter_value(instance,name,expected);actual=LIB.get_material_instance_texture_parameter_value(instance,name)
   assert actual and object_path(actual)==value['native']==object_path(expected),(name,actual,value);actual=object_path(actual)
  observed[name]=dict(type=kind,actual=actual,expected=value)
 return observed

def main():
 plan=apply_native_render_expectations(apply_solar_foundation(preflight()));policy=material_policy(plan);textures,concrete,shared,protected=textures_and_guard(plan);parents={};bindings={};files=[];mappings=[]
 stale=OUT/'ApprovedUtilityMaterials.json'
 if stale.exists():stale.unlink()
 for asset in plan['assets']:
  mapping={}
  for graph in asset['graphs']:
   compiled=compile_graph(graph,plan['textures']);template=compiled['template'];path=parent_path(template)
   if template not in parents:
    parent=create(path,unreal.Material,unreal.MaterialFactoryNew());LIB.delete_all_material_expressions(parent)
    for name,value in dict(used_with_instanced_static_meshes=True,blend_mode=unreal.BlendMode.BLEND_OPAQUE,tangent_space_normal=True,two_sided=False).items():parent.set_editor_property(name,value)
    TemplateGraph(parent,compiled,textures).build();parents[template]=dict(path=path,structure=TEMPLATES[template],structural_signature=compiled['structural_signature'],representative_defaults=compiled['parameters'],statistics=finish_material(parent));files.append(path)
   parent=unreal.load_asset(path);states={};observed={}
   for damaged in (False,True):
    destination=binding(asset,graph,damaged);instance=create(destination,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());LIB.set_material_instance_parent(instance,parent)
    label='damaged' if damaged else 'healthy';observed[label]=assign_and_verify(instance,instance_parameters(compiled,damaged),textures);save(instance);files.append(destination);states[label]=destination
   mapping[graph['region']]=dict(**states,parent=path,template=template);mappings.append(dict(key=asset['key'],region=graph['region'],compiled=compiled,instance_parameter_readbacks=observed))
  bindings[asset['key']]=mapping;unreal.log('APPROVED_UTILITY_SHARED_MATERIAL_READY '+asset['key'])
 assert set(parents)==set(TEMPLATES) and len(mappings)==51 and len(files)==105
 unchanged(protected)
 receipt=dict(phase='materials',engine_version=unreal.SystemLibrary.get_engine_version(),plan_sha256=sha(PLAN),recipe_hashes=recipe_hashes(),native_import_overrides=plan['native_import_overrides'],
  source_mappings=[{k:a[k] for k in ('key','issue','version','approval_sha256','source_head')} for a in plan['assets']],bindings=bindings,parents=parents,template_count=len(parents),material_mappings=mappings,
  files=emit_files(files),shared_textures=shared,stock_concrete=plan['stock_concrete'],shared_inputs_unchanged=True,material_policy=policy,
  proposed_visual_limitations=policy['proposed_visual_limitations'],native_graph_compile_run=True,visual_acceptance='pending125 review',no_catalog_runtime_changes=True)
 OUT.mkdir(parents=True,exist_ok=True);write(OUT/'ApprovedUtilityMaterials.json',receipt)
 unreal.log('APPROVED_UTILITY_SHARED_MATERIAL_PHASE_PASS 3 structural parents,51 healthy/damaged mappings,105 scoped packages')
