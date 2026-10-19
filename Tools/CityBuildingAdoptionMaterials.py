"""D91 shared standard opaque Unreal materials, scoped to newly authored assets."""
import unreal
lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
SCOPE='/Game/CitySim/Art'

def create(path,kind,factory):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
def save(obj):assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
def node(m,kind):return lib.create_material_expression(m,kind)
def link(a,b,pin,output=''):assert lib.connect_material_expressions(a,output,b,pin)
def prop(a,pin,output=''):assert lib.connect_material_property(a,output,pin)
def scalar(m,value):
    n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n
def color(m,value):
    n=node(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*value,1));return n
def sample(m,texture,sampler):
    n=node(m,unreal.MaterialExpressionTextureSample);n.set_editor_property('texture',texture);n.set_editor_property('sampler_type',sampler);return n
def multiply(m,a,b):
    n=node(m,unreal.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
def mix(m,a,b,alpha,output=''):
    n=node(m,unreal.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(alpha,n,'Alpha',output);return n
def master(name):
    m=create(SCOPE+'/Materials/M_'+name,unreal.Material,unreal.MaterialFactoryNew());assert m
    lib.delete_all_material_expressions(m);m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    return m
def finish(m):
    lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    s=lib.get_statistics(m);stats={k:s.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
    assert stats['num_pixel_shader_instructions']>0,m.get_path_name()
    return stats

def build(root,doc,surfaces):
    textures={};statistics={};changed=[]
    if '-D91ReuseMaterials' in unreal.SystemLibrary.get_command_line():
        names=['BuildingWindowOpaque','HomeWindowOpaque','HomeSolidDetails','CivicBrick','CivicAsphalt']
        for name in names:
            m=unreal.load_asset(SCOPE+'/Materials/M_'+name);assert m and m.get_editor_property('used_with_instanced_static_meshes') and m.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_OPAQUE
            s=lib.get_statistics(m);statistics[name]={k:s.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')};assert statistics[name]['num_pixel_shader_instructions']>0
            changed.append(SCOPE+'/Materials/M_'+name)
        tints={'CivicAsphaltRoof':(.13,.15,.17)}
        for e in surfaces['assets']:
            for t in e['tints'].values():tints[t['new_material']]=t['color']
        for name,value in tints.items():
            path=SCOPE+'/Materials/MI_'+name;instance=unreal.load_asset(path);assert instance
            tint=lib.get_material_instance_vector_parameter_value(instance,'CityColor');assert all(abs(a-b)<.0001 for a,b in zip((tint.r,tint.g,tint.b),value));changed.append(path)
        changed += [SCOPE+'/Textures/T_'+name for name in ('HomeGlass','CivicAsphalt_BaseColor','CivicAsphalt_Normal','CivicAsphalt_ORM')]
        return dict(shader_statistics=statistics,changed=changed,tints=tints,reused_compiled_materials=True)
    new_files=[(doc['glass_texture'],'HomeGlass',False)]
    new_files += [(v['file'],'CivicAsphalt_'+k,True) for k,v in doc['textures']['CivicAsphalt']['maps'].items()]
    for filename,name,wrap in new_files:
        task=unreal.AssetImportTask()
        for k,v in dict(filename=str(root/filename),destination_path=SCOPE+'/Textures',destination_name='T_'+name,automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.TextureFactory();assets.import_asset_tasks([task]);t=unreal.load_asset(SCOPE+'/Textures/T_'+name);assert t
        normal=name.endswith('_Normal');orm=name.endswith('_ORM')
        t.set_editor_property('srgb',not(normal or orm));t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if orm else unreal.TextureCompressionSettings.TC_DEFAULT)
        address=unreal.TextureAddress.TA_WRAP if wrap else unreal.TextureAddress.TA_CLAMP
        t.set_editor_property('address_x',address);t.set_editor_property('address_y',address)
        if normal:t.set_editor_property('flip_green_channel',False)
        save(t);textures[name]=t;changed.append(t.get_path_name().split('.')[0])
    for suffix in ('BaseColor','Normal','ORM'):
        textures['CityBrick_'+suffix]=unreal.load_asset(SCOPE+'/Textures/T_CityBrick_'+suffix);assert textures['CityBrick_'+suffix]
    for name in ('BuildingWindowOpaque','HomeWindowOpaque'):
        m=master(name);vertex=node(m,unreal.MaterialExpressionVertexColor)
        glass=sample(m,textures['HomeGlass'],unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        prop(mix(m,glass,color(m,(.76,.73,.64)),vertex,'A'),unreal.MaterialProperty.MP_BASE_COLOR)
        prop(mix(m,scalar(m,.18),scalar(m,.55),vertex,'A'),unreal.MaterialProperty.MP_ROUGHNESS);prop(scalar(m,.5),unreal.MaterialProperty.MP_SPECULAR)
        inverse=node(m,unreal.MaterialExpressionOneMinus);link(vertex,inverse,'','A')
        clock=node(m,unreal.MaterialExpressionCollectionParameter);clock.set_editor_property('collection',unreal.load_asset(SCOPE+'/Materials/MPC_CityLighting'));clock.set_editor_property('parameter_name','NightGlow')
        prop(multiply(m,multiply(m,inverse,clock),color(m,(.8,.46,.16))),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        statistics[name]=finish(m);changed.append(m.get_path_name().split('.')[0])
    m=master('HomeSolidDetails');vertex=node(m,unreal.MaterialExpressionVertexColor)
    linear=node(m,unreal.MaterialExpressionPower);linear.set_editor_property('const_exponent',2.2);link(vertex,linear,'Base')
    inverse=node(m,unreal.MaterialExpressionOneMinus);link(vertex,inverse,'','A');weight=multiply(m,inverse,scalar(m,2))
    brick=sample(m,textures['CityBrick_BaseColor'],unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    prop(mix(m,linear,multiply(m,linear,brick),weight),unreal.MaterialProperty.MP_BASE_COLOR)
    prop(mix(m,scalar(m,.55),scalar(m,.8),weight),unreal.MaterialProperty.MP_ROUGHNESS)
    statistics['HomeSolidDetails']=finish(m);changed.append(m.get_path_name().split('.')[0])
    parents={}
    for prefix in ('CityBrick','CivicAsphalt'):
        name='CivicBrick' if prefix=='CityBrick' else 'CivicAsphalt';m=master(name);info=doc['textures'][prefix]
        base=sample(m,textures[prefix+'_BaseColor'],unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        normal=sample(m,textures[prefix+'_Normal'],unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        orm=sample(m,textures[prefix+'_ORM'],unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        tint=node(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','CityColor');tint.set_editor_property('default_value',unreal.LinearColor(.13,.15,.17,1))
        normalized=multiply(m,base,color(m,info['normalization']))
        contrasted=mix(m,color(m,(1,1,1)),normalized,scalar(m,info['strength']))
        prop(multiply(m,contrasted,tint),unreal.MaterialProperty.MP_BASE_COLOR);prop(normal,unreal.MaterialProperty.MP_NORMAL)
        prop(orm,unreal.MaterialProperty.MP_ROUGHNESS,'G');prop(orm,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R');prop(scalar(m,0),unreal.MaterialProperty.MP_METALLIC)
        statistics[name]=finish(m);changed.append(m.get_path_name().split('.')[0]);parents[name]=m
    tints={'CivicAsphaltRoof':(.13,.15,.17)}
    for entry in surfaces['assets']:
        for t in entry['tints'].values():
            name=t['new_material'];value=tuple(t['color'])
            assert name not in tints or tints[name]==value;tints[name]=value
    for name,value in tints.items():
        path=SCOPE+'/Materials/MI_'+name;instance=create(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());assert instance
        lib.set_material_instance_parent(instance,parents['CivicAsphalt' if name=='CivicAsphaltRoof' else 'CivicBrick'])
        lib.set_material_instance_vector_parameter_value(instance,'CityColor',unreal.LinearColor(*value,1));save(instance);changed.append(path)
    return dict(shader_statistics=statistics,changed=changed,tints={k:list(v) for k,v in tints.items()})
