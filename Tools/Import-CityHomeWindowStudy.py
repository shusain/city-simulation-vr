"""D91 native standard-node material/import check in the isolated content-only project."""
from pathlib import Path
import unreal, json, hashlib
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'Artifacts/D91'
scale='-D91HomeScale' in unreal.SystemLibrary.get_command_line()
details=scale or '-D91HomeDetails' in unreal.SystemLibrary.get_command_line()
OUT=BASE/'Scale' if scale else BASE/'Details' if details else BASE
PROJECT=BASE/'UnrealProject'
assert Path(unreal.Paths.project_dir()).resolve()==PROJECT.resolve(), 'Study must never run in the gameplay project'
doc=json.loads((ROOT/('ArtSource/Manifests/city_home_scale_study.json' if scale else 'ArtSource/Manifests/city_home_detail_study.json' if details else 'ArtSource/Manifests/city_home_window_study.json')).read_text())
SCOPE='/Game/CitySim/Art/Prototypes/D91'
mesh_only=scale or '-D91MeshesOnly' in unreal.SystemLibrary.get_command_line()
lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
def get(path,kind,factory):
    assert path.startswith(SCOPE+'/')
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
def save(o):assert unreal.EditorAssetLibrary.save_loaded_asset(o)
def node(m,kind):return lib.create_material_expression(m,kind)
def link(a,b,pin,output=''):assert lib.connect_material_expressions(a,output,b,pin)
def prop(a,pin):assert lib.connect_material_property(a,'',pin)
def scalar(m,value):
    n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n
def color(m,value):
    n=node(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*value,1));return n
if not (details or mesh_only):
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(ROOT/doc['texture']),destination_path=SCOPE+'/Textures',destination_name='T_HomeGlass',automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.TextureFactory();assets.import_asset_tasks([task])
    texture=unreal.load_asset(SCOPE+'/Textures/T_HomeGlass');assert texture
    texture.set_editor_property('srgb',True);texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP);save(texture)
materials={};statistics={}
for translucent in ((False,) if details else (False,True)):
    name='HomeWindowTranslucent' if translucent else 'HomeWindowOpaque'
    if mesh_only or details:
        m=unreal.load_asset(SCOPE+'/Materials/M_'+name);assert m
        stats=lib.get_statistics(m);statistics[name]={k:stats.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
        assert statistics[name]['num_pixel_shader_instructions']>0
        materials[name]=m
        continue
    m=get(SCOPE+'/Materials/M_'+name,unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_OPAQUE)
    if translucent:m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    vertex=node(m,unreal.MaterialExpressionVertexColor)
    sample=node(m,unreal.MaterialExpressionTextureSample);sample.set_editor_property('texture',texture);sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mix=node(m,unreal.MaterialExpressionLinearInterpolate);link(sample,mix,'A');link(color(m,(.76,.73,.64)),mix,'B');link(vertex,mix,'Alpha','A');prop(mix,unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(m,unreal.MaterialExpressionLinearInterpolate);link(scalar(m,.18),rough,'A');link(scalar(m,.55),rough,'B');link(vertex,rough,'Alpha','A');prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    prop(scalar(m,.5),unreal.MaterialProperty.MP_SPECULAR)
    if translucent:
        opacity=node(m,unreal.MaterialExpressionLinearInterpolate);link(scalar(m,.35),opacity,'A');link(scalar(m,1),opacity,'B');link(vertex,opacity,'Alpha','A');prop(opacity,unreal.MaterialProperty.MP_OPACITY)
    inv=node(m,unreal.MaterialExpressionOneMinus);link(vertex,inv,'','A')
    clock=node(m,unreal.MaterialExpressionCollectionParameter);clock.set_editor_property('collection',unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting'));clock.set_editor_property('parameter_name','NightGlow')
    amount=node(m,unreal.MaterialExpressionMultiply);link(inv,amount,'A');link(clock,amount,'B')
    emissive=node(m,unreal.MaterialExpressionMultiply);link(amount,emissive,'A');link(color(m,(.8,.46,.16)),emissive,'B');prop(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    stats=lib.get_statistics(m);statistics[name]={k:stats.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
    assert statistics[name]['num_pixel_shader_instructions']>0, 'Native shader compilation failed'
    materials[name]=m
if details:
    m=unreal.load_asset(SCOPE+'/Materials/M_HomeSolidDetails') if mesh_only else get(SCOPE+'/Materials/M_HomeSolidDetails',unreal.Material,unreal.MaterialFactoryNew())
    assert m
    if not mesh_only:
        lib.delete_all_material_expressions(m);m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
        vertex=node(m,unreal.MaterialExpressionVertexColor)
        # Match the existing citizen FBX palette path: Blender exports sRGB
        # vertex channels, while the shader needs linear base-color values.
        linear=node(m,unreal.MaterialExpressionPower);linear.set_editor_property('const_exponent',2.2);link(vertex,linear,'Base')
        brick=node(m,unreal.MaterialExpressionTextureSample);brick.set_editor_property('texture',unreal.load_asset('/Game/CitySim/Art/Textures/T_CityBrick_BaseColor'));brick.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tint=node(m,unreal.MaterialExpressionMultiply);link(linear,tint,'A');link(brick,tint,'B')
        inverse=node(m,unreal.MaterialExpressionOneMinus);link(vertex,inverse,'','A')
        weight=node(m,unreal.MaterialExpressionMultiply);link(inverse,weight,'A');link(scalar(m,2),weight,'B')
        blend=node(m,unreal.MaterialExpressionLinearInterpolate);link(linear,blend,'A');link(tint,blend,'B');link(weight,blend,'Alpha');prop(blend,unreal.MaterialProperty.MP_BASE_COLOR)
        rough=node(m,unreal.MaterialExpressionLinearInterpolate);link(scalar(m,.55),rough,'A');link(scalar(m,.8),rough,'B');link(weight,rough,'Alpha');prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
        lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    stats=lib.get_statistics(m);statistics['HomeSolidDetails']={k:stats.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
    assert statistics['HomeSolidDetails']['num_pixel_shader_instructions']>0
    materials['HomeSolidDetails']=m
records=[]
for e in doc['assets']:
    path=ROOT/e['export'];assert hashlib.sha256(path.read_bytes()).hexdigest()==e['sha256'] and e['roundtrip_pass']
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(path),destination_path=SCOPE+'/Meshes',destination_name=e['name'],automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
    options.set_editor_property('reset_to_fbx_on_material_conflict',True)
    data=options.static_mesh_import_data
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():data.set_editor_property(k,v)
    task.options=options;assets.import_asset_tasks([task]);mesh=unreal.load_asset(SCOPE+'/Meshes/'+e['name']);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert set(slots)==set(e['slots']), (slots,e['slots'])
    for i,slot in enumerate(slots):
        mat=materials.get(slot) or unreal.load_asset('/Game/CitySim/Art/Materials/MI_'+slot);assert mat,slot;mesh.set_material(i,mat)
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
    b=mesh.get_bounding_box();size=b.max-b.min
    assert all(abs(a-c*100)<.2 for a,c in zip((size.x,size.y,size.z),e['dimensions_m']))
    assert abs(b.min.z+20)<.1
    save(mesh);records.append(dict(name=e['name'],triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),dimensions_cm=[size.x,size.y,size.z],materials=[mesh.get_material(i).get_path_name() for i in range(len(slots))]))
(OUT/'Import.json').write_text(json.dumps(dict(task='D91',project=str(PROJECT),meshes=records,shader_statistics=statistics,production_import=False),indent=2)+'\n')
unreal.log('CITYSIM_D91_UNREAL_IMPORT_PASS '+str(len(records))+' meshes; native shaders and dimensions/sections verified')
