"""Standard-node materials for masked night windows and subtle shared pavement."""
import unreal
import os
root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ready = os.path.join(root,"Artifacts","Diagnostics","D38-surfaces-material-ready.txt")
if os.path.exists(ready): os.remove(ready)
folder = "/Game/CitySim/Art/Materials"
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
def get(path, kind, factory):
    return unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
def node(mat, kind, x=0, y=0): return lib.create_material_expression(mat,kind,x,y)
def link(a,b,pin,output=""):
    if not lib.connect_material_expressions(a,output,b,pin): raise RuntimeError("Material connection failed")
def prop(a,pin):
    if not lib.connect_material_property(a,"",pin): raise RuntimeError("Material output failed")
def scalar(mat,name,value):
    n=node(mat,unreal.MaterialExpressionScalarParameter);n.set_editor_property("parameter_name",name);n.set_editor_property("default_value",value);return n
def color(mat,name,value):
    n=node(mat,unreal.MaterialExpressionVectorParameter);n.set_editor_property("parameter_name",name);n.set_editor_property("default_value",unreal.LinearColor(*value,1));return n
def material(name):
    m=get(folder+'/'+name,unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m)
    m.set_editor_property("used_with_instanced_static_meshes",True)
    m.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE)
    return m
def save(m):
    lib.layout_material_expressions(m);lib.recompile_material(m)
    if not unreal.EditorAssetLibrary.save_loaded_asset(m): raise RuntimeError("Cannot save generated material")
collection=get(folder+'/MPC_CityLighting',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
params=collection.get_editor_property('scalar_parameters')
if not params:
    param=unreal.CollectionScalarParameter();param.set_editor_property('parameter_name','NightGlow');param.set_editor_property('default_value',0.0)
    collection.set_editor_property('scalar_parameters',[param])
unreal.EditorAssetLibrary.save_loaded_asset(collection)
m=material('M_CityWindowDetail')
base=color(m,'CityColor',(.10,.14,.19));prop(base,unreal.MaterialProperty.MP_BASE_COLOR)
rough=scalar(m,'Roughness',.65);prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
vertex=node(m,unreal.MaterialExpressionVertexColor)
mask=node(m,unreal.MaterialExpressionOneMinus);link(vertex,mask,'','A')
clock=node(m,unreal.MaterialExpressionCollectionParameter);clock.set_editor_property('collection',collection);clock.set_editor_property('parameter_name','NightGlow')
amount=node(m,unreal.MaterialExpressionMultiply);link(mask,amount,'A');link(clock,amount,'B')
warm=color(m,'WindowColor',(.8,.46,.16));emissive=node(m,unreal.MaterialExpressionMultiply);link(amount,emissive,'A');link(warm,emissive,'B');prop(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR);save(m)
# Tiny seamless texture: R is low-contrast grain; G is a broad slab joint mask.
task=unreal.AssetImportTask();task.set_editor_property('filename',os.path.join(root,'ArtSource','Textures','CityPavement.png'));task.set_editor_property('destination_path','/Game/CitySim/Art/Textures');task.set_editor_property('destination_name','T_CityPavement');task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True);assets.import_asset_tasks([task])
texture=unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Art/Textures/T_CityPavement');texture.set_editor_property('srgb',False);texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS);unreal.EditorAssetLibrary.save_loaded_asset(texture)
m=material('M_CityPavement')
base=color(m,'CityColor',(.48,.47,.42))
uv=node(m,unreal.MaterialExpressionTextureCoordinate);uv.set_editor_property('u_tiling',4.0);uv.set_editor_property('v_tiling',4.0)
sample=node(m,unreal.MaterialExpressionTextureSample);sample.set_editor_property('texture',texture);sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS);link(uv,sample,'')
grain=node(m,unreal.MaterialExpressionMultiply);grain.set_editor_property('const_b',2.0);link(sample,grain,'A','R')
strength=scalar(m,'GrainStrength',.6);variation=node(m,unreal.MaterialExpressionLinearInterpolate);variation.set_editor_property('const_a',1.0);link(grain,variation,'B');link(strength,variation,'Alpha')
seams=scalar(m,'SeamStrength',0.0);joint=node(m,unreal.MaterialExpressionLinearInterpolate);joint.set_editor_property('const_a',1.0);link(sample,joint,'B','G');link(seams,joint,'Alpha')
a=node(m,unreal.MaterialExpressionMultiply);link(base,a,'A');link(variation,a,'B');b=node(m,unreal.MaterialExpressionMultiply);link(a,b,'A');link(joint,b,'B');prop(b,unreal.MaterialProperty.MP_BASE_COLOR)
rough=scalar(m,'Roughness',.9);prop(rough,unreal.MaterialProperty.MP_ROUGHNESS);save(m)
# A success artifact makes Python failures visible even when the editor exits zero.
with open(os.path.join(root,'Artifacts','Diagnostics','D38-surfaces-material-ready.txt'),'w') as f: f.write('ready')
